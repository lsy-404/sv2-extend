#include "../plugins/sv2-mcp/windows/protocol.h"
#include <cassert>
using windows_mcp::json;
static json reply;
static void completed(const char *body,size_t length,void*){reply=body?json::parse(body,body+length):json();}
static json definition(const char *name,const char *method){return {{"name",name},{"method",method},{"description","description"},{"inputSchema",{{"type","object"},{"properties",json::object()},{"additionalProperties",false}}}};}
static void registry(const char *method,const char *args,loader_json_completion done,void *ctx){
 json value;
 if(std::string(method)=="editor.capabilities")value={{"ok",true},{"result",{{"providers",json::array({{{"id","editor.lua"},{"capabilities",{{"methods",json::array({"selection.read","voicebanks.current","voicebanks.project","exports.start"})},{"tools",json::array({definition("svext_read_editor","selection.read"),definition("svext_read_voicebank","voicebanks.current"),definition("svext_project_voicebanks","voicebanks.project"),definition("svext_export_wav","exports.start"),definition("svext_list_voicebanks","voicebanks.list")})}}}},{{"id","extension"},{"capabilities",{{"methods",json::array({"notes.read"})},{"tools",json::array({definition("svext_read","notes.read")})}}}}})}}}};
 else if(std::string(method)=="voicebanks.list"){value={{"ok",false},{"error",{{"code","METHOD_UNAVAILABLE"}}}};}
 else {assert(std::string(method)=="notes.read"||std::string(method)=="selection.read"||std::string(method)=="voicebanks.project");assert(json::parse(args)==json::object());value={{"ok",true},{"result",{{"notes",json::array()}}}};}
 auto data=value.dump();done(data.data(),data.size(),ctx);
}
static json call(loader_api_v1 *api,const char *input){windows_mcp::handle(api,input,strlen(input),completed,nullptr);return reply;}
static bool has(const json &list,const char *name){for(const auto &tool:list["result"]["tools"])if(tool["name"]==name)return true;return false;}
int main(){
 loader_api_v1 api{};api.request_capability_json=registry;
 auto list=call(&api,R"({"jsonrpc":"2.0","id":1,"method":"tools/list"})");assert(list["result"]["tools"].size()==6);assert(has(list,"sv_status")&&has(list,"sv_read_editor")&&has(list,"sv_read_voicebank")&&has(list,"sv_project_voicebanks")&&has(list,"sv_export_wav")&&has(list,"svext_read"));assert(!has(list,"svext_read_editor")&&!has(list,"sv_list_voicebanks"));
 auto result=call(&api,R"({"jsonrpc":"2.0","id":2,"method":"tools/call","params":{"name":"sv_read_editor"}})");assert(result["result"]["structuredContent"]["ok"]==true);
 auto inventory=call(&api,R"({"jsonrpc":"2.0","id":11,"method":"tools/call","params":{"name":"sv_project_voicebanks"}})");assert(inventory["result"]["structuredContent"]["ok"]==true);assert(!has(list,"svext_project_voicebanks"));
 auto custom=call(&api,R"({"jsonrpc":"2.0","id":3,"method":"tools/call","params":{"name":"svext_read"}})");assert(custom["result"]["structuredContent"]["ok"]==true);
 auto unavailable=call(&api,R"({"jsonrpc":"2.0","id":10,"method":"tools/call","params":{"name":"sv_list_voicebanks"}})");assert(unavailable["result"]["isError"]==true&&unavailable["result"]["structuredContent"]["error"]["code"]=="METHOD_UNAVAILABLE");
 auto old=call(&api,R"({"jsonrpc":"2.0","id":4,"method":"tools/call","params":{"name":"svext_read_editor"}})");assert(old["error"]["code"]==-32602);
 windows_mcp::set_settings({{"tool.sv_read_editor",false}});auto filtered=call(&api,R"({"jsonrpc":"2.0","id":5,"method":"tools/list"})");assert(!has(filtered,"sv_read_editor")&&has(filtered,"svext_read"));auto disabled_tool=call(&api,R"({"jsonrpc":"2.0","id":6,"method":"tools/call","params":{"name":"sv_read_editor"}})");assert(disabled_tool["error"]["code"]==-32002);
 windows_mcp::set_settings({{"enabled",false}});auto disabled=call(&api,R"({"jsonrpc":"2.0","id":7,"method":"initialize"})");assert(disabled["error"]["code"]==-32001);
 windows_mcp::set_settings(json::object());auto status=call(&api,R"({"jsonrpc":"2.0","id":8,"method":"tools/call","params":{"name":"sv_status"}})");assert(status["result"]["structuredContent"]["ok"]==true);
 auto bad=call(&api,R"({"jsonrpc":"2.0","id":9,"method":"tools/call","params":{"name":"sv_read_editor","arguments":[]}})");assert(bad["error"]["code"]==-32602);
}
