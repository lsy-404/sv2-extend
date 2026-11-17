#pragma once
#include <memory>
#include <mutex>
#include <string>
#include "../../../loader/windows/vendor/json.hpp"
#include "../../../sdk/include/loader_api.h"

namespace windows_mcp {
using json = nlohmann::json;
inline std::mutex settings_mutex;
inline json settings=json::object();
inline void set_settings(const json &value){std::lock_guard<std::mutex> lock(settings_mutex);settings=value.is_object()?value:json::object();}
inline bool enabled(const std::string &key){std::lock_guard<std::mutex> lock(settings_mutex);auto it=settings.find(key);return it==settings.end() || !it->is_boolean() || it->get<bool>();}
struct Builtin {const char *method,*name,*title,*description;};
inline const Builtin *builtins(size_t &count){static const Builtin definitions[]={
 {"capabilities","sv_status","读取服务状态","Read the resident SV2 MCP status and actual editor capabilities. No product account is required."},
 {"exports.snapshot","sv_export_snapshot","读取导出状态","Get a fresh 15-second current-project export guard and native render busy state."},
 {"exports.start","sv_export_wav","开始导出","Start an asynchronous WAV mixdown. Output parent must exist; existing files are never overwritten. Poll sv_export_status until terminal."},
 {"exports.status","sv_export_status","查询导出任务","Poll an export job. Completed native output is validated and atomically published without overwriting. Keep the same project open."},
 {"exports.cancel","sv_export_cancel","取消导出任务","Request cancellation of the current native export job; poll status to confirm."},
 {"selection.read","sv_read_editor","读取工程与选区","Read the current live editor selection, notes and edit guards."},
 {"voicebanks.current","sv_read_voicebank","读取当前声库","Read the host-selected voicebank identity through the native host adapter."},
 {"voicebanks.project","sv_project_voicebanks","读取工程声库","List assigned voicebank identities used by the current project and their reference counts."},
 {"voicebanks.list","sv_list_voicebanks","读取声库列表","List voicebanks reported as available by the running editor, not the sales catalog."},
 {"voicebanks.select","sv_select_voicebank","切换声库","Select a voicebank using a fresh host selection guard; verify the resulting identity."},
 {"notes.set_pitch","sv_set_note_pitch","修改音符音高","Set MIDI note pitch through the live editor, with a fresh guard and one undo record."},
 {"pitch.read_range","sv_read_pitch_range","读取音高曲线","Read pitchDelta points and a fresh range guard from the live editor."},
 {"pitch.replace_range","sv_set_pitch_curve","修改音高曲线","Replace an explicit bounded pitchDelta range, preserving unrelated control points and one undo record."},
 {"groups.create","sv_create_group","创建音符组","Create a note group on the current track using the current edit guard."},
 {"groups.remove","sv_remove_group","删除音符组","Remove an exclusively referenced library group using its fresh edit guard."},
 {"notes.add","sv_add_note","添加音符","Add one note to an exclusive group using a fresh edit guard."},
 {"notes.remove","sv_remove_note","删除音符","Remove one note from an exclusive group using a fresh edit guard."}
 };count=sizeof definitions/sizeof definitions[0];return definitions;}
inline const Builtin *builtin_for_method(const std::string &method){size_t count=0;auto list=builtins(count);for(size_t i=0;i<count;i++)if(method==list[i].method)return list+i;return nullptr;}
inline const Builtin *builtin_for_name(const std::string &name){size_t count=0;auto list=builtins(count);for(size_t i=0;i<count;i++)if(name==list[i].name)return list+i;return nullptr;}
struct Request {
 const loader_api_v1 *api;
 loader_json_completion complete;
 void *context;
 json id, params;
 bool list;
};
inline void finish(Request *request, const json &body) {
 auto encoded=body.dump(); request->complete(encoded.data(),encoded.size(),request->context); delete request;
}
inline void error(Request *request,int code,const char *message) {
 finish(request,{{"jsonrpc","2.0"},{"id",request->id},{"error",{{"code",code},{"message",message}}}});
}
inline void called(const char *body,size_t length,void *opaque) {
 auto request=static_cast<Request*>(opaque);
 if(!enabled("enabled")){error(request,-32001,"MCP is disabled");return;}
 auto value=body?json::parse(body,body+length,nullptr,false):json();
 if(!value.is_object() || !value.contains("ok") || !value["ok"].is_boolean()) {error(request,-32603,"Invalid provider response");return;}
 json result={{"content",json::array({{{"type","text"},{"text",value.dump()}}})},{"structuredContent",value},{"isError",!value["ok"].get<bool>()}};
 finish(request,{{"jsonrpc","2.0"},{"id",request->id},{"result",result}});
}
inline void capabilities(const char *body,size_t length,void *opaque) {
 auto request=static_cast<Request*>(opaque);
 if(!enabled("enabled")){error(request,-32001,"MCP is disabled");return;}
 auto value=body?json::parse(body,body+length,nullptr,false):json();
 if(!value.is_object() || !value.contains("ok") || !value["ok"].is_boolean() || !value["ok"].get<bool>() || !value.contains("result") || !value["result"].is_object() || !value["result"].contains("providers") || !value["result"]["providers"].is_array()) {error(request,-32603,"Capability registry unavailable");return;}
 json tools=json::array();std::string method;
 const auto *status=builtin_for_method("capabilities");if(enabled("tool.sv_status")){if(request->list)tools.push_back({{"name",status->name},{"description",status->description},{"inputSchema",{{"type","object"},{"properties",json::object()},{"additionalProperties",false}}}});else if(request->params["name"]==status->name){called(body,length,request);return;}}else if(!request->list&&request->params["name"]==status->name){error(request,-32002,"Tool is disabled in Extend settings");return;}
 for(const auto &provider:value["result"]["providers"]) {
  if(!provider.is_object() || !provider.contains("capabilities") || !provider["capabilities"].is_object()) continue;
  const auto &caps=provider["capabilities"];
  if(!caps.contains("methods") || !caps["methods"].is_array() || !caps.contains("tools") || !caps["tools"].is_array()) continue;
  for(const auto &definition:caps["tools"]) {
   if(!definition.is_object() || !definition.contains("name") || !definition["name"].is_string() || !definition.contains("method") || !definition["method"].is_string() || !definition.contains("description") || !definition["description"].is_string() || !definition.contains("inputSchema") || !definition["inputSchema"].is_object()) continue;
   bool official=provider.contains("id")&&provider["id"]=="editor.lua";const auto *mapped=official?builtin_for_method(definition["method"].get<std::string>()):nullptr;
   if(official&&!mapped)continue;auto tool=definition;if(mapped){tool["name"]=mapped->name;tool["description"]=mapped->description;if(definition["method"]=="pitch.replace_range")tool["inputSchema"]["properties"]["points"]["items"]={{"type","array"},{"minItems",2},{"maxItems",2},{"items",{{"type","number"}}}};}
   if(!enabled("tool."+tool["name"].get<std::string>())){if(!request->list && tool["name"]==request->params["name"]) {error(request,-32002,"Tool is disabled in Extend settings");return;}continue;}
   bool available=false;for(const auto &entry:caps["methods"])if(entry==definition["method"])available=true;
   if(!available)continue;
   if(request->list){tool.erase("method");tools.push_back(tool);}
   else if(tool["name"]==request->params["name"])method=definition["method"].get<std::string>();
  }
 }
 if(request->list){finish(request,{{"jsonrpc","2.0"},{"id",request->id},{"result",{{"tools",tools}}}});return;}
 if(method.empty()){auto known=builtin_for_name(request->params["name"].get<std::string>());if(!known){error(request,-32602,"Unknown tool");return;}if(!enabled(std::string("tool.")+known->name)){error(request,-32002,"Tool is disabled in Extend settings");return;}method=known->method;}
 auto arguments=request->params.value("arguments",json::object()).dump();
 request->api->request_capability_json(method.c_str(),arguments.c_str(),called,request);
}
inline void handle(const loader_api_v1 *api,const char *body,size_t length,loader_json_completion complete,void *context) {
 if(!complete)return;
 auto request=new Request{api,complete,context,nullptr,json::object(),false};
 if(!body || length>1048576){error(request,-32600,"Invalid request");return;}
 auto value=json::parse(body,body+length,nullptr,false);
 if(!value.is_object() || !value.contains("jsonrpc") || value["jsonrpc"]!="2.0" || !value.contains("method") || !value["method"].is_string()){error(request,-32600,"Invalid request");return;}
 if(!value.contains("id")){complete(nullptr,0,context);delete request;return;}
 request->id=value["id"];
 if(!request->id.is_string() && !request->id.is_number_integer()){request->id=nullptr;error(request,-32600,"Invalid request id");return;}
 if(value.contains("params") && !value["params"].is_object()){error(request,-32602,"Invalid parameters");return;}
 request->params=value.value("params",json::object());auto method=value["method"].get<std::string>();
 if(!enabled("enabled")){error(request,-32001,"MCP is disabled");return;}
 if(method=="initialize") {
  auto version=request->params.value("protocolVersion",json("2025-11-25"));
  if(!version.is_string() || (version!="2025-11-25" && version!="2025-06-18" && version!="2025-03-26"))version="2025-11-25";
  finish(request,{{"jsonrpc","2.0"},{"id",request->id},{"result",{{"protocolVersion",version},{"capabilities",{{"tools",{{"listChanged",false}}}}},{"serverInfo",{{"name","sv2-mcp"},{"version","0.3.0"}}}}}});return;
 }
 if(method=="ping"){finish(request,{{"jsonrpc","2.0"},{"id",request->id},{"result",json::object()}});return;}
 if(method!="tools/list" && method!="tools/call"){error(request,-32601,"Method not found");return;}
 request->list=method=="tools/list";
 if(!request->list && (!request->params.contains("name") || !request->params["name"].is_string() || (request->params.contains("arguments") && !request->params["arguments"].is_object()))){error(request,-32602,"Invalid tool parameters");return;}
 if(!api || !api->request_capability_json){error(request,-32603,"Capability registry unavailable");return;}
 api->request_capability_json("editor.capabilities","{}",capabilities,request);
}
}
