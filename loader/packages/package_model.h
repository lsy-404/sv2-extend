#pragma once
// Platform-independent package model: version ranges, the Loader capability
// catalog, manifest validation and requirement resolution. The including
// translation unit provides nlohmann::json.
#include <array>
#include <cctype>
#include <map>
#include <set>
#include <string>
#include <vector>
#include "panel_icon.h"

namespace package_model {
using json=nlohmann::json;
constexpr const char *loader_version="0.10.0";

struct Version{std::array<unsigned long long,3> parts{};bool operator<(const Version &other)const{return parts<other.parts;}bool operator==(const Version &other)const{return parts==other.parts;}};
inline bool parse_version(const std::string &text,Version &out){
 if(text.empty()||text.size()>64)return false;size_t index=0;unsigned count=0;out={};
 while(index<text.size()){
  if(count==3||!isdigit((unsigned char)text[index]))return false;unsigned long long value=0;size_t digits=0;
  while(index<text.size()&&isdigit((unsigned char)text[index])){if(++digits>9)return false;value=value*10+(text[index++]-'0');}
  out.parts[count++]=value;if(index==text.size())break;if(text[index++]!='.'||index==text.size())return false;
 }
 return count>0;
}
// Space-separated comparators, all of which must hold: ">=1.2 <2", "=1.0", "1.0", "*".
inline bool satisfies(const std::string &range,const std::string &version,bool *valid=nullptr){
 Version actual;bool ok=parse_version(version,actual)&&!range.empty()&&range.size()<=64;bool result=ok;size_t index=0;unsigned terms=0;
 while(ok&&index<range.size()){
  while(index<range.size()&&range[index]==' ')++index;if(index==range.size())break;
  size_t end=range.find(' ',index);if(end==std::string::npos)end=range.size();std::string term=range.substr(index,end-index);index=end;++terms;
  if(term=="*")continue;
  std::string op;while(!term.empty()&&(term[0]=='>'||term[0]=='<'||term[0]=='='))op+=term[0],term.erase(0,1);
  Version wanted;if(!parse_version(term,wanted)){ok=false;break;}
  if(op.empty()||op=="=")result=result&&actual==wanted;else if(op==">=")result=result&&!(actual<wanted);else if(op==">")result=result&&wanted<actual;
  else if(op=="<=")result=result&&!(wanted<actual);else if(op=="<")result=result&&actual<wanted;else ok=false;
 }
 ok=ok&&terms>0;if(valid)*valid=ok;return ok&&result;
}

struct Capability{const char *id;const char *version;bool grantable;const char *title;std::vector<const char*> methods;};
// Host facets serve a package's own contributions and need no grant. API
// capabilities group the public methods a package reaches only after the user
// enables the matching permission.
inline const std::vector<Capability> &catalog(){
 static const std::vector<Capability> entries={
  {"mcp.tools","1",false,"发布 MCP 工具",{}},
  {"panel","1",false,"在 Extend 中注册面板与菜单",{}},
  {"storage","1",false,"读写扩展包自己的数据",{"storage.read","storage.write"}},
  {"profile.clean","1",false,"启动干净的宿主配置",{}},
  {"runtime.lua","5.5.1",false,"在隔离的 Lua 运行时中执行扩展包代码",{}},
  {"runtime.protected","1",false,"读取自带密文并在内存中解密、加载扩展包逻辑",{}},
  {"editor.read","1",true,"读取工程、选区与音高",{"selection.read","pitch.read_range"}},
  {"editor.write","1",true,"修改音符、音符组与音高",{"notes.set_pitch","notes.add","notes.remove","groups.create","groups.remove","pitch.replace_range"}},
  {"transport.read","1",true,"读取播放状态",{"transport.status"}},
  {"transport.control","1",true,"控制播放",{"transport.play","transport.stop"}},
  {"voicebanks.read","1",true,"读取声库",{"voicebanks.current","voicebanks.project","voicebanks.list"}},
  {"voicebanks.select","1",true,"切换声库",{"voicebanks.select"}},
  {"exports.read","1",true,"读取导出状态",{"exports.snapshot","exports.status"}},
  {"exports.write","1",true,"开始与取消导出",{"exports.start","exports.cancel"}},
  {"http","1",true,"发起 HTTPS 请求",{"http.request"}},
  {"http.server","1",true,"在本机回环地址提供 HTTP 服务",{"server.listen","server.close"}},
  {"update","1",true,"访问扩展包自己声明的更新端点",{"update.fetch"}}};
 return entries;
}
inline const Capability *capability(const std::string &id){for(const auto &entry:catalog())if(id==entry.id)return &entry;return nullptr;}
inline const Capability *capability_for_method(const std::string &method){for(const auto &entry:catalog())for(auto name:entry.methods)if(method==name)return &entry;return nullptr;}

inline bool identifier(const std::string &id){if(id.empty()||id.size()>96||!isalpha((unsigned char)id[0]))return false;for(unsigned char c:id)if(!isalnum(c)&&c!='.'&&c!='_'&&c!='-')return false;return true;}
// Domains no capability provider may claim.
inline bool reserved_domain(const std::string &name){auto domain=name.substr(0,name.find('.'));for(auto reserved:{"account","session","license","private","auth","ui","runtime","package","storage"})if(domain==reserved)return true;return false;}

// A call targets a capability method, or a handler in the package's own code.
struct Call{std::string method,handler;json arguments=json::object();};
struct Tool{std::string name,method;json description,inputSchema;Call call;};
// A menu is at most two levels: actions, or titled groups of actions.
struct MenuItem{std::string id,title;bool group=false;Call call;};
struct Manifest{
 std::string id,name,version,loader="*",main,update_url,icon,active_icon;bool enabled=true,native=false,panel=false,match_icon_colors=true;
 std::map<std::string,std::string> capabilities,packages;std::vector<std::string> permissions,methods;std::vector<Tool> tools;std::vector<MenuItem> menu;
 bool permits(const std::string &key)const{for(const auto &entry:permissions)if(entry==key)return true;return false;}
};
inline bool text(const json &value,size_t limit){return value.is_string()&&!value.get_ref<const std::string&>().empty()&&value.get_ref<const std::string&>().size()<=limit;}
inline bool ranges(const json &value,std::map<std::string,std::string> &out,size_t limit){
 if(!value.is_object()||value.size()>limit)return false;
 for(auto item=value.begin();item!=value.end();++item){bool valid=false;if(!identifier(item.key())||!item.value().is_string())return false;satisfies(item.value().get<std::string>(),"0",&valid);if(!valid)return false;out[item.key()]=item.value().get<std::string>();}
 return true;
}
inline bool keys_within(const json &object,std::initializer_list<const char*> allowed){for(auto item=object.begin();item!=object.end();++item){bool found=false;for(auto key:allowed)found=found||item.key()==key;if(!found)return false;}return true;}
// A catalog method must be covered by a required facet or a declared
// permission; any other target belongs to a required package and is resolved
// when it is called.
inline bool parse_call(const json &value,const Manifest &manifest,Call &out){
 if(!value.is_object()||!keys_within(value,{"method","handler","arguments"})||value.contains("method")==value.contains("handler"))return false;
 out.arguments=value.value("arguments",json::object());if(!out.arguments.is_object()||out.arguments.dump().size()>65536)return false;
 if(value.contains("handler")){if(!value["handler"].is_string())return false;out.handler=value["handler"].get<std::string>();return identifier(out.handler)&&!manifest.main.empty();}
 if(!value["method"].is_string())return false;out.method=value["method"].get<std::string>();if(!identifier(out.method))return false;
 if(auto owner=capability_for_method(out.method))return owner->grantable?manifest.permits(owner->id):manifest.capabilities.count(owner->id)!=0;
 return !reserved_domain(out.method)&&!manifest.packages.empty();
}
// Returns an empty string on success, otherwise a stable reason code.
inline std::string parse(const json &definition,Manifest &out){
 out={};
 if(!definition.is_object()||!definition.contains("schema")||!definition["schema"].is_number_integer()||definition["schema"]!=1)return "schema";
 if(!definition.contains("id")||!definition["id"].is_string()||!identifier(definition["id"].get<std::string>()))return "id";
 out.id=definition["id"].get<std::string>();
 if(definition.contains("enabled")){if(!definition["enabled"].is_boolean())return "enabled";out.enabled=definition["enabled"].get<bool>();}
 out.native=definition.contains("nativeLibraries");
 bool declarative=definition.contains("permissions")||definition.contains("contributes")||definition.contains("main");
 if(!out.native&&!declarative)return "empty";
 if(definition.contains("name")){if(!text(definition["name"],128))return "name";out.name=definition["name"].get<std::string>();}
 if(definition.contains("version")){Version parsed;if(!definition["version"].is_string()||!parse_version(definition["version"].get<std::string>(),parsed))return "version";out.version=definition["version"].get<std::string>();}
 if(declarative&&(out.name.empty()||out.version.empty()))return "metadata";
 if(out.name.empty())out.name=out.id;if(out.version.empty())out.version="0";
 if(definition.contains("requires")){
  const auto &required=definition["requires"];if(!required.is_object()||!keys_within(required,{"loader","capabilities","packages"}))return "requires";
  if(required.contains("loader")){bool valid=false;if(!required["loader"].is_string())return "requires";out.loader=required["loader"].get<std::string>();satisfies(out.loader,"0",&valid);if(!valid)return "requires";}
  if(required.contains("capabilities")&&!ranges(required["capabilities"],out.capabilities,32))return "requires";
  if(required.contains("packages")&&(!ranges(required["packages"],out.packages,16)||out.packages.count(out.id)))return "requires";
 }
 if(definition.contains("permissions")){
  // Each entry is a grantable capability or "package.<id>" for a required package.
  const auto &list=definition["permissions"];if(out.native||!list.is_array()||list.size()>32)return "permissions";
  for(const auto &entry:list){
   if(!entry.is_string())return "permissions";auto key=entry.get<std::string>();auto known=capability(key);
   bool dependency=key.rfind("package.",0)==0&&out.packages.count(key.substr(8));
   if(out.permits(key)||!identifier(key)||(known?!known->grantable:!dependency&&key.rfind("package.",0)==0))return "permissions";
   out.permissions.push_back(key);
  }
 }
 if(definition.contains("main")){
  if(out.native||!text(definition["main"],512)||!out.capabilities.count("runtime.lua"))return "main";out.main=definition["main"].get<std::string>();
  if(out.main.size()<5||out.main.compare(out.main.size()-4,4,".lua")!=0)return "main";
 }
 if(definition.contains("contributes")){
  // Unknown contribution kinds fail closed so a package is never half-enabled.
  const auto &contributes=definition["contributes"];if(out.native||!contributes.is_object()||!keys_within(contributes,{"mcpTools","panel","methods"}))return "contributes";
  if(contributes.contains("mcpTools")){
   const auto &tools=contributes["mcpTools"];if(!tools.is_array()||tools.empty()||tools.size()>64||!out.capabilities.count("mcp.tools")||reserved_domain(out.id))return "mcpTools";
   std::set<std::string> names;
   for(const auto &entry:tools){
    if(!entry.is_object()||!keys_within(entry,{"name","description","inputSchema","call"})||!entry.contains("name")||!entry["name"].is_string()||!entry.contains("description")||!text(entry["description"],1024)||!entry.contains("inputSchema")||!entry["inputSchema"].is_object()||!entry["inputSchema"].contains("type")||entry["inputSchema"]["type"]!="object"||!entry.contains("call"))return "mcpTools";
    Tool tool;tool.name=entry["name"].get<std::string>();tool.method=out.id+"."+tool.name;
    if(tool.name.rfind("svext_",0)!=0||!identifier(tool.name)||!identifier(tool.method)||!names.insert(tool.name).second)return "mcpTools";
    if(!parse_call(entry["call"],out,tool.call))return "undeclared-call";
    tool.description=entry["description"];tool.inputSchema=entry["inputSchema"];out.tools.push_back(std::move(tool));
   }
  }
  if(contributes.contains("methods")){
   // Handlers other packages may call as "<id>.<name>" once they depend on this package.
   const auto &methods=contributes["methods"];if(!methods.is_array()||methods.empty()||methods.size()>64||out.main.empty()||reserved_domain(out.id))return "methods";
   for(const auto &entry:methods){
    if(!entry.is_string())return "methods";auto name=entry.get<std::string>();bool repeated=false;for(const auto &known:out.methods)repeated=repeated||known==name;
    if(repeated||!identifier(name)||name.find('.')!=std::string::npos||name.rfind("svext_",0)==0||!identifier(out.id+"."+name))return "methods";
    out.methods.push_back(name);
   }
  }
  if(contributes.contains("panel")){
   const auto &panel=contributes["panel"];if(!panel.is_object()||!keys_within(panel,{"menu"})||!out.capabilities.count("panel"))return "panel";
   out.panel=true;std::set<std::string> ids;
   auto action=[&](const json &entry){
    MenuItem item;if(!entry.is_object()||!keys_within(entry,{"id","title","call"})||!entry.contains("id")||!entry["id"].is_string()||!entry.contains("title")||!text(entry["title"],128)||!entry.contains("call"))return std::string("panel");
    item.id=entry["id"].get<std::string>();item.title=entry["title"].get<std::string>();if(!identifier(item.id)||!ids.insert(item.id).second)return std::string("panel");
    if(!parse_call(entry["call"],out,item.call))return std::string("undeclared-call");
    out.menu.push_back(std::move(item));return std::string();
   };
   if(panel.contains("menu")){
    if(!panel["menu"].is_array())return "panel";
    for(const auto &entry:panel["menu"]){
     if(entry.is_object()&&entry.contains("items")){
      if(!keys_within(entry,{"title","items"})||!entry.contains("title")||!text(entry["title"],128)||!entry["items"].is_array()||entry["items"].empty())return "panel";
      MenuItem group;group.group=true;group.title=entry["title"].get<std::string>();out.menu.push_back(std::move(group));
      for(const auto &child:entry["items"]){auto failure=action(child);if(!failure.empty())return failure;}
     }else{auto failure=action(entry);if(!failure.empty())return failure;}
    }
    if(out.menu.size()>24)return "panel";
   }
  }
 }
 if(definition.contains("update")){
  // A declared HTTPS endpoint the package checks for its own updates; the Loader pins update.fetch to it.
  const auto &update=definition["update"];if(out.native||!update.is_object()||!keys_within(update,{"url"})||!text(update.value("url",json()),512))return "update";
  out.update_url=update["url"].get<std::string>();if(out.update_url.rfind("https://",0)!=0)return "update";
 }
 if(out.permits("update")&&out.update_url.empty())return "update";
 if(definition.contains("icon")){
  if(!definition["icon"].is_string()||!panel_icon::valid_svg(definition["icon"].get_ref<const std::string&>()))return "icon";
  out.icon=definition["icon"].get<std::string>();
 }
 if(definition.contains("activeIcon")){
  if(!definition["activeIcon"].is_string()||!panel_icon::valid_svg(definition["activeIcon"].get_ref<const std::string&>()))return "activeIcon";
  out.active_icon=definition["activeIcon"].get<std::string>();
 }
 if(definition.contains("matchIconColors")){
  if(!definition["matchIconColors"].is_boolean())return "matchIconColors";
  out.match_icon_colors=definition["matchIconColors"].get<bool>();
 }
 return "";
}

// Static requirements only; method availability is reported at runtime.
struct Environment{std::set<std::string> capabilities;std::map<std::string,std::string> packages;};
inline std::string resolve(const Manifest &manifest,const Environment &environment){
 if(!satisfies(manifest.loader,loader_version))return "loader-version";
 for(const auto &entry:manifest.capabilities){auto known=capability(entry.first);if(!known||!environment.capabilities.count(entry.first)||!satisfies(entry.second,known->version))return "missing-capability:"+entry.first;}
 for(const auto &entry:manifest.packages){auto found=environment.packages.find(entry.first);if(found==environment.packages.end()||!satisfies(entry.second,found->second))return "missing-package:"+entry.first;}
 return "";
}
// A declared permission the current Loader cannot serve is not offered to the user.
inline std::vector<std::string> grantable(const Manifest &manifest,const Environment &environment){
 std::vector<std::string> keys;for(const auto &key:manifest.permissions)if(!capability(key)||environment.capabilities.count(key))keys.push_back(key);return keys;
}
inline json describe(const Environment &environment){
 json list=json::array();
 for(const auto &entry:catalog()){json methods=json::array();for(auto method:entry.methods)methods.push_back(method);list.push_back({{"id",entry.id},{"version",entry.version},{"supported",environment.capabilities.count(entry.id)!=0},{"grantable",entry.grantable},{"title",entry.title},{"methods",methods}});}
 return {{"version",loader_version},{"capabilities",list}};
}
}
