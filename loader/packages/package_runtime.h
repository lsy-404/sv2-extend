#pragma once
// One isolated Lua state per package. Package code sees only pure-computation
// libraries and the `extend` object; every effect goes through Host::request,
// where the platform Loader applies the package's grants. The Lua sources are
// compiled as C++, so Lua errors unwind as exceptions through these frames.
// The including translation unit provides nlohmann::json.
#include <cmath>
#include <cstdlib>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include "vendor/lua/lua.h"
#include "vendor/lua/lualib.h"
#include "vendor/lua/lauxlib.h"

namespace package_runtime {
using json=nlohmann::json;
using Done=std::function<void(const json&)>;
// request completes on the Loader main thread, at most once; log is main-thread.
struct Host{std::function<void(const std::string &method,const json &arguments,Done done)> request;std::function<void(const std::string &text)> log;};
constexpr const char *lua_version="5.5.1";
constexpr size_t memory_limit=16*1024*1024,source_limit=262144;
constexpr long long instruction_budget=20000000;
inline json failure(const char *code,const std::string &message=std::string()){json error{{"code",code}};if(!message.empty())error["message"]=message.substr(0,512);return {{"ok",false},{"error",error}};}

class Runtime:public std::enable_shared_from_this<Runtime>{
public:
 // Runs the main chunk with `extend` as its argument; it returns the handler table.
 static std::shared_ptr<Runtime> create(Host host,const std::string &id,const std::string &version,const std::string &source,std::string &error){
  std::shared_ptr<Runtime> runtime(new Runtime(std::move(host)));
  if(source.size()>source_limit){error="main chunk too large";return nullptr;}
  auto L=runtime->state=lua_newstate(allocate,runtime.get(),static_cast<unsigned>(reinterpret_cast<uintptr_t>(runtime.get())>>4));
  if(!L){error="out of memory";return nullptr;}
  *static_cast<Runtime**>(lua_getextraspace(L))=runtime.get();lua_pushlightuserdata(L,const_cast<std::string*>(&id));lua_pushlightuserdata(L,const_cast<std::string*>(&version));lua_pushlightuserdata(L,const_cast<std::string*>(&source));
  lua_pushcfunction(L,prepare);lua_insert(L,-4);
  if(runtime->call(3,0)!=LUA_OK){error=runtime->message();return nullptr;}
  return runtime;
 }
 ~Runtime(){
  auto abandoned=std::move(pending);pending.clear();if(state)lua_close(state);state=nullptr;
  for(auto &entry:abandoned)entry.second(failure("PACKAGE_UNLOADED"));
 }
 bool handles(const std::string &name){lua_rawgeti(state,LUA_REGISTRYINDEX,module);lua_pushlstring(state,name.data(),name.size());lua_rawget(state,-2);bool found=lua_isfunction(state,-1);lua_pop(state,2);return found;}
 // The handler receives (arguments, reply); reply(result) or reply(nil, error) completes the call.
 void invoke(const std::string &handler,const json &arguments,Done done){
  auto alive=shared_from_this();auto L=state;int id=++serial;pending[id]=std::move(done);
  lua_pushcfunction(L,start);lua_pushlightuserdata(L,const_cast<std::string*>(&handler));lua_pushlightuserdata(L,const_cast<json*>(&arguments));lua_pushinteger(L,id);
  if(call(3,0)!=LUA_OK)finish(id,failure("HANDLER_FAILED",message()));
 }
private:
 explicit Runtime(Host value):host(std::move(value)){}
 Host host;lua_State *state=nullptr;int module=LUA_NOREF,serial=0,depth=0;size_t used=0;long long budget=0;bool exhausted=false;std::map<int,Done> pending;
 static Runtime *self(lua_State *L){return *static_cast<Runtime**>(lua_getextraspace(L));}
 static void *allocate(void *owner,void *block,size_t before,size_t after){
  auto runtime=static_cast<Runtime*>(owner);if(!block)before=0;
  if(!after){std::free(block);runtime->used-=before;return nullptr;}
  if(after>before&&runtime->used+(after-before)>memory_limit)return nullptr;
  auto next=std::realloc(block,after);if(next)runtime->used+=after-before;return next;
 }
 // Once the budget is spent every instruction of the running thread raises, so pcall cannot keep a loop alive.
 static void hook(lua_State *L,lua_Debug*){
  auto runtime=self(L);int step=lua_gethookcount(L);
  if(!runtime->exhausted){runtime->budget-=step>0?step:1;if(runtime->budget>0){if(step==1)lua_sethook(L,hook,LUA_MASKCOUNT,1000);return;}runtime->exhausted=true;}
  lua_sethook(L,hook,LUA_MASKCOUNT,1);luaL_error(L,"instruction budget exceeded");
 }
 int call(int arguments,int results){
  if(!depth++){budget=instruction_budget;exhausted=false;lua_sethook(state,hook,LUA_MASKCOUNT,1000);}
  int status=lua_pcall(state,arguments,results,0);--depth;return status;
 }
 std::string message(){size_t length=0;auto text=lua_tolstring(state,-1,&length);std::string value=text?std::string(text,length):"error";lua_pop(state,1);return value;}
 void finish(int id,const json &response){auto found=pending.find(id);if(found==pending.end())return;auto done=std::move(found->second);pending.erase(found);done(response);}

 static void push(lua_State *L,const json &value,int level){
  if(level>32)luaL_error(L,"value nested too deeply");luaL_checkstack(L,4,"value too large");
  if(value.is_boolean())lua_pushboolean(L,value.get<bool>());
  else if(value.is_number_integer())lua_pushinteger(L,static_cast<lua_Integer>(value.get<long long>()));
  else if(value.is_number())lua_pushnumber(L,value.get<double>());
  else if(value.is_string()){const auto &text=value.get_ref<const std::string&>();lua_pushlstring(L,text.data(),text.size());}
  else if(value.is_array()){lua_createtable(L,static_cast<int>(value.size()),0);lua_Integer index=0;for(const auto &item:value){push(L,item,level+1);lua_rawseti(L,-2,++index);}}
  else if(value.is_object()){lua_createtable(L,0,static_cast<int>(value.size()));for(auto item=value.begin();item!=value.end();++item){push(L,item.value(),level+1);lua_setfield(L,-2,item.key().c_str());}}
  else lua_pushnil(L);
 }
 // A table whose keys are exactly 1..n is an array; any other table needs string keys.
 static bool read(lua_State *L,int index,json &out,int level){
  index=lua_absindex(L,index);
  switch(lua_type(L,index)){
   case LUA_TNIL:out=nullptr;return true;
   case LUA_TBOOLEAN:out=lua_toboolean(L,index)!=0;return true;
   case LUA_TNUMBER:if(lua_isinteger(L,index))out=static_cast<long long>(lua_tointeger(L,index));else{double number=lua_tonumber(L,index);if(std::isfinite(number))out=number;else out=nullptr;}return true;
   case LUA_TSTRING:{size_t length=0;auto text=lua_tolstring(L,index,&length);out=std::string(text,length);return true;}
   case LUA_TTABLE:break;
   default:return false;
  }
  if(level>32||!lua_checkstack(L,4))return false;
  size_t length=static_cast<size_t>(lua_rawlen(L,index)),count=0;bool sequence=length>0,named=false,indexed=false;json object=json::object();
  lua_pushnil(L);
  while(lua_next(L,index)){
   ++count;json item;if(!read(L,-1,item,level+1)){lua_pop(L,2);return false;}
   if(lua_type(L,-2)==LUA_TSTRING){size_t size=0;auto key=lua_tolstring(L,-2,&size);object[std::string(key,size)]=std::move(item);sequence=false;named=true;}
   else if(!sequence||!lua_isinteger(L,-2)||lua_tointeger(L,-2)<1||static_cast<size_t>(lua_tointeger(L,-2))>length){lua_pop(L,2);return false;}
   else indexed=true;
   lua_pop(L,1);if(named&&indexed){lua_pop(L,1);return false;}
  }
  if(!sequence||count!=length){if(!object.empty()||!count){out=std::move(object);return true;}return false;}
  out=json::array();for(size_t item=1;item<=length;++item){json value;lua_rawgeti(L,index,static_cast<lua_Integer>(item));bool ok=read(L,-1,value,level+1);lua_pop(L,1);if(!ok)return false;out.push_back(std::move(value));}
  return true;
 }
 static json error_value(lua_State *L,int index){
  json value;if(lua_type(L,index)==LUA_TSTRING)return failure("HANDLER_ERROR",lua_tostring(L,index));
  if(read(L,index,value,0)&&value.is_object()&&value.contains("code")&&value["code"].is_string())return {{"ok",false},{"error",value}};
  return failure("HANDLER_ERROR");
 }

 static int print(lua_State *L){
  std::string line;int count=lua_gettop(L);for(int index=1;index<=count;++index){size_t length=0;auto text=luaL_tolstring(L,index,&length);if(index>1)line+='\t';line.append(text,length);lua_pop(L,1);if(line.size()>2048)break;}
  auto runtime=self(L);if(runtime->host.log)runtime->host.log(line.substr(0,2048));return 0;
 }
 static int request(lua_State *L){
  auto runtime=self(L);size_t length=0;auto name=luaL_checklstring(L,1,&length);std::string method(name,length);json arguments=json::object();
  if(!lua_isnoneornil(L,2)&&(!read(L,2,arguments,0)||!arguments.is_object()))return luaL_error(L,"arguments must be a table with string keys");
  luaL_checktype(L,3,LUA_TFUNCTION);lua_pushvalue(L,3);int callback=luaL_ref(L,LUA_REGISTRYINDEX);
  std::weak_ptr<Runtime> weak=runtime->weak_from_this();
  runtime->host.request(method,arguments,[weak,callback](const json &response){if(auto alive=weak.lock())alive->resume(callback,response);});
  return 0;
 }
 static int deliver(lua_State *L){
  auto response=static_cast<const json*>(lua_touserdata(L,2));lua_settop(L,1);
  if(response->is_object()&&response->value("ok",false)){push(L,response->contains("result")?(*response)["result"]:json(),0);lua_pushnil(L);}
  else{lua_pushnil(L);push(L,response->is_object()&&response->contains("error")?(*response)["error"]:json{{"code","INVALID_RESPONSE"}},0);}
  lua_call(L,2,0);return 0;
 }
 void resume(int callback,const json &response){
  auto L=state;lua_pushcfunction(L,deliver);lua_rawgeti(L,LUA_REGISTRYINDEX,callback);luaL_unref(L,LUA_REGISTRYINDEX,callback);lua_pushlightuserdata(L,const_cast<json*>(&response));
  if(call(2,0)!=LUA_OK&&host.log)host.log("callback failed: "+message());
 }
 static int reply(lua_State *L){
  auto runtime=self(L);int id=static_cast<int>(lua_tointeger(L,lua_upvalueindex(1)));json response;
  if(lua_toboolean(L,2))response=error_value(L,2);
  else{json result;if(!read(L,1,result,0))response=failure("INVALID_RESULT");else response={{"ok",true},{"result",result.is_null()?json::object():result}};}
  runtime->finish(id,response);return 0;
 }
 static int start(lua_State *L){
  auto runtime=self(L);auto handler=static_cast<const std::string*>(lua_touserdata(L,1));auto arguments=static_cast<const json*>(lua_touserdata(L,2));
  lua_rawgeti(L,LUA_REGISTRYINDEX,runtime->module);lua_pushlstring(L,handler->data(),handler->size());lua_rawget(L,-2);
  if(!lua_isfunction(L,-1))return luaL_error(L,"handler %s is not defined",handler->c_str());
  push(L,*arguments,0);lua_pushvalue(L,3);lua_pushcclosure(L,reply,1);lua_call(L,2,0);return 0;
 }
 static int prepare(lua_State *L){
  auto runtime=self(L);auto id=static_cast<const std::string*>(lua_touserdata(L,1));auto version=static_cast<const std::string*>(lua_touserdata(L,2));auto source=static_cast<const std::string*>(lua_touserdata(L,3));
  const luaL_Reg libraries[]={{LUA_GNAME,luaopen_base},{LUA_COLIBNAME,luaopen_coroutine},{LUA_TABLIBNAME,luaopen_table},{LUA_STRLIBNAME,luaopen_string},{LUA_MATHLIBNAME,luaopen_math},{LUA_UTF8LIBNAME,luaopen_utf8}};
  for(const auto &library:libraries){luaL_requiref(L,library.name,library.func,1);lua_pop(L,1);}
  for(auto name:{"dofile","loadfile","load","collectgarbage"}){lua_pushnil(L);lua_setglobal(L,name);}
  lua_getglobal(L,LUA_STRLIBNAME);lua_pushnil(L);lua_setfield(L,-2,"dump");lua_pop(L,1);
  lua_pushcfunction(L,print);lua_setglobal(L,"print");
  auto name="@"+*id+"/main.lua";if(luaL_loadbufferx(L,source->data(),source->size(),name.c_str(),"t")!=LUA_OK)return lua_error(L);
  lua_createtable(L,0,3);lua_pushcfunction(L,request);lua_setfield(L,-2,"request");lua_pushcfunction(L,print);lua_setfield(L,-2,"log");
  lua_createtable(L,0,2);lua_pushlstring(L,id->data(),id->size());lua_setfield(L,-2,"id");lua_pushlstring(L,version->data(),version->size());lua_setfield(L,-2,"version");lua_setfield(L,-2,"package");
  lua_call(L,1,1);if(!lua_istable(L,-1))return luaL_error(L,"the main chunk must return a table of handlers");
  runtime->module=luaL_ref(L,LUA_REGISTRYINDEX);return 0;
 }
};
}
