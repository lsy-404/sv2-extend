#pragma once
#include <string>
#include "vendor/json.hpp"
#include "../../sdk/include/loader_api.h"
namespace script_settings {
using json=nlohmann::json;
struct Runtime {const loader_api_v1 *api;const char *id;const char *title;bool ready;json values=json::object();};
inline json fields(bool lua){
 json result=json::array({{{"key","enabled"},{"title",lua?"启用 Lua Extend":"启用 JS Extend"},{"type","boolean"},{"default",true}}});
 const char *items[][4]={
  {"voicebanks.current","读取当前声库","声库","voicebanks"},{"voicebanks.project","读取工程声库","声库","voicebanks"},{"voicebanks.list","读取声库列表","声库","voicebanks"},{"voicebanks.select","切换声库","声库","voicebanks"},
  {"exports.snapshot","读取导出状态","音频导出","exports"},{"exports.start","开始导出","音频导出","exports"},{"exports.status","查询导出任务","音频导出","exports"},{"exports.cancel","取消导出任务","音频导出","exports"},
  {"selection.read","读取工程与选区","工程与音高","editor"},{"notes.set_pitch","修改音符音高","工程与音高","editor"},{"pitch.read_range","读取音高曲线","工程与音高","editor"},{"pitch.replace_range","修改音高曲线","工程与音高","editor"},
  {"transport.status","读取播放状态","播放","editor"},{"transport.play","开始播放","播放","editor"},{"transport.stop","停止播放","播放","editor"},
  {"http.start","发起本地或在线请求","HTTP 请求","http"},{"http.status","查询请求结果","HTTP 请求","http"},{"http.cancel","取消请求","HTTP 请求","http"}};
 for(const auto &item:items)result.push_back({{"key",item[0]},{"title",item[1]},{"detail",item[0]},{"section",item[2]},{"legacyGroup",item[3]},{"type","boolean"},{"default",true}});
 result.push_back({{"key","showDetails"},{"title","显示运行详情"},{"type","boolean"},{"default",false}});return result;
}
inline void changed(const char *body,void *owner){
 auto &runtime=*static_cast<Runtime*>(owner);auto event=body?json::parse(body,nullptr,false):json();
 if(!event.is_object()||event.value("kind",std::string())!="settings"||!event.contains("settings")||!event["settings"].is_object())return;
 runtime.values=event["settings"];bool enabled=runtime.values.value("enabled",true),details=runtime.values.value("showDetails",false);
 json rows=json::array({{{"type","label"},{"text",!enabled?"扩展功能已停用":runtime.ready?"脚本扩展功能可用":"当前宿主的原生接口不可用"}}});
 if(details)rows.push_back({{"type","label"},{"text","在插件设置中可单独开关各个函数。"}});
 auto state=json{{"status",!enabled?"已停用":runtime.ready?"已启用":"当前宿主不可用"},{"sidebar",{{"title",runtime.title},{"rows",rows}}}}.dump();
 if(runtime.api->update_plugin_ui)runtime.api->update_plugin_ui(runtime.id,state.c_str(),owner);
}
inline int register_runtime(Runtime &runtime,bool lua){
 if(!runtime.api||!runtime.api->register_plugin_ui)return LOADER_ERR_NOT_SUPPORTED;
 auto descriptor=json{{"id",runtime.id},{"name",runtime.title},{"version","1.1"},{"status",runtime.ready?"已就绪":"当前宿主不可用"},{"sidebar",{{"title",runtime.title},{"rows",json::array({{{"type","label"},{"text",runtime.ready?"原生扩展接口可用":"原生扩展接口不可用"}}})}}},{"settings",fields(lua)}}.dump();
 return runtime.api->register_plugin_ui(descriptor.c_str(),changed,&runtime);
}
inline bool enabled(const Runtime &runtime,const std::string &key){auto it=runtime.values.find(key);return it==runtime.values.end()||!it->is_boolean()||it->get<bool>();}
inline const char *denial(const Runtime &runtime,const std::string &method,const json &args){
 if(method.rfind("editor.",0)==0||method.rfind("ui.",0)==0||method=="runtime.info"||method=="call.result"||method=="voice.native_status"||method=="voice.catalog"||method=="voice.capture.begin"||method=="voice.capture.end"||method=="voice.select.begin"||method=="voice.select.validate"||method=="voice.select.end"||method=="export.capture.begin"||method=="export.capture.end"||method=="notes.native_status"||method=="groups.native_status"||method=="export.native_status")return nullptr;
 std::string target=method;if(method=="call.submit"&&args.is_object()&&args.contains("method")&&args["method"].is_string())target=args["method"].get<std::string>();
 bool finishing=target=="http.status"||target=="http.cancel"||target=="exports.status"||target=="exports.cancel"||target=="export.status"||target=="export.cancel";
 if(!finishing&&!enabled(runtime,"enabled"))return "EXTENSION_DISABLED";
 if(target.rfind("voice.",0)==0)target="voicebanks."+target.substr(6);else if(target.rfind("export.",0)==0)target="exports."+target.substr(7);
 return enabled(runtime,target)?nullptr:"FEATURE_DISABLED";
}
}
