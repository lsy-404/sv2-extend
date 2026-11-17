#include "../plugins/sv2-mcp/windows/protocol.h"
#include <cassert>
using windows_mcp::json;
static json reply;
static void completed(const char *body,size_t length,void*){reply=body?json::parse(body,body+length):json();}
static void registry(const char *method,const char *args,loader_json_completion done,void *ctx){
 json value;
 if(std::string(method)=="editor.capabilities")value={{"ok",true},{"result",{{"providers",json::array({{{"capabilities",{{"methods",json::array({"notes.read"})},{"tools",json::array({{{"name","svext_read"},{"method","notes.read"},{"description","Read notes"},{"inputSchema",{{"type","object"}}}},{{"name","svext_missing"},{"method","notes.missing"},{"description","Unavailable"},{"inputSchema",{{"type","object"}}}}})}}}}})}}}};
 else {assert(std::string(method)=="notes.read");assert(json::parse(args)==json::object());value={{"ok",true},{"result",{{"notes",json::array()}}}};}
 auto data=value.dump();done(data.data(),data.size(),ctx);
}
static json call(loader_api_v1 *api,const char *input){windows_mcp::handle(api,input,strlen(input),completed,nullptr);return reply;}
int main(){
 loader_api_v1 api{};api.request_capability_json=registry;
 auto list=call(&api,R"({"jsonrpc":"2.0","id":1,"method":"tools/list"})");assert(list["result"]["tools"].size()==1);assert(list["result"]["tools"][0]["name"]=="svext_read");assert(!list["result"]["tools"][0].contains("method"));
 auto result=call(&api,R"({"jsonrpc":"2.0","id":2,"method":"tools/call","params":{"name":"svext_read"}})");assert(result["result"]["structuredContent"]["ok"]==true);
 auto missing=call(&api,R"({"jsonrpc":"2.0","id":3,"method":"tools/call","params":{"name":"svext_missing"}})");assert(missing["error"]["code"]==-32602);
 windows_mcp::set_settings({{"method.notes.read",false}});
 auto disabled_list=call(&api,R"({"jsonrpc":"2.0","id":5,"method":"tools/list"})");assert(disabled_list["result"]["tools"].empty());
 auto disabled_tool=call(&api,R"({"jsonrpc":"2.0","id":6,"method":"tools/call","params":{"name":"svext_read"}})");assert(disabled_tool["error"]["code"]==-32002);
 windows_mcp::set_settings({{"enabled",false}});
 auto disabled=call(&api,R"({"jsonrpc":"2.0","id":7,"method":"initialize"})");assert(disabled["error"]["code"]==-32001);
 windows_mcp::set_settings(json::object());
 auto restored=call(&api,R"({"jsonrpc":"2.0","id":8,"method":"tools/list"})");assert(restored["result"]["tools"].size()==1);
 auto bad=call(&api,R"({"jsonrpc":"2.0","id":4,"method":"tools/call","params":{"name":"svext_read","arguments":[]}})");assert(bad["error"]["code"]==-32602);
}
