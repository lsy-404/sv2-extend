#pragma once
#include "javascript_gate.h"
#include "windows_voice_catalog_probe.h"
#include "windows_voice_error_probe.h"
#include "windows_render_modal_probe.h"
#include "windows_audio_render_probe.h"
#include "windows_network_live_probe.h"
#include "../loader/windows/network_scope.h"
namespace {nlohmann::json script_call(const std::string&,const nlohmann::json&,bool);}
namespace editor_autostart {nlohmann::json render_call(const std::string&,const nlohmann::json&);nlohmann::json render_status();nlohmann::json render_request();bool run_verification_script(const wchar_t*);}
namespace parity_probe {
using json=nlohmann::json;
static std::map<std::string,uint64_t> identities;
static json reports=json::object();static const loader_api_v1 *runtime_api=nullptr;static int owner;
static windows_network_probe::Probe network_probe;
static json route(const std::string &method,const json &args,bool javascript){if(method=="editor.verification_report"){reports[javascript?"javascript":"lua"]=args;return {{"ok",true},{"result",json::object()}};}return script_call(method,args,javascript);}
static void reply(loader_json_completion done,void *context,const json &value){auto text=value.dump();done(text.data(),text.size(),context);}
static void dispatch(const char *raw_method,const char *raw_args,loader_json_completion done,void *context,void*){
 auto args=json::parse(raw_args,nullptr,false);std::string method=raw_method;bool ok=args.is_object();json result;
 if(!ok){reply(done,context,{{"ok",false}});return;}
 if(method=="test.parity.state"){
  json changed=json::array();for(auto &item:native_panel::panes)for(auto &entry:item.second.controls){auto key=item.second.id+"/"+entry.first;auto found=identities.find(key);if(found==identities.end())identities[key]=entry.second->verification_identity;else if(found->second!=entry.second->verification_identity)changed.push_back(key);}
  json panes=json::array();for(auto &item:native_panel::panes)panes.push_back({{"id",item.second.id},{"ready",item.second.ready},{"controls",item.second.controls.size()}});
  result={{"hostPid",GetCurrentProcessId()},{"rootReady",editor_autostart::root_ready()},{"javascriptReady",javascript_gate::ready()},{"reports",reports},{"ui",native_panel::snapshot()},{"panes",panes},{"recreatedControls",changed}};
 }else if(method=="test.parity.run"){
  auto language=args.value("language",std::string());bool lua=language=="lua"||language=="voice-lua"||language=="export-lua",js=language=="javascript"||language=="voice-javascript"||language=="export-javascript";ok=lua||js;if(ok){const char *directory=runtime_api->host_data_directory();ok=directory!=nullptr;if(ok){const wchar_t *name=language=="voice-lua"?L"extend-voice-lua.lua":language=="voice-javascript"?L"extend-voice-js.js":language=="export-lua"?L"extend-export-lua.lua":language=="export-javascript"?L"extend-export-js.js":lua?L"extend-parity-lua.lua":L"extend-parity-js.js";auto path=std::filesystem::u8path(directory)/L"Roaming"/L"Dreamtonics"/L"Synthesizer V Studio 2"/L"scripts"/name;reports.erase(lua?"lua":"javascript");ok=editor_autostart::run_verification_script(path.c_str());}}result={{"requested",ok}};
 }else if(method=="test.parity.ui"){
  auto target=args.value("id",std::string());ok=target=="opensv2.lua-extend"||target=="opensv2.javascript-extend"||target=="sv2.mcp";
  if(ok){auto operation=args.value("operation",std::string("ui.set_setting"));ok=operation=="ui.set_setting"||operation=="ui.action";if(ok){auto values=args;values.erase("operation");values["generation"]=native_panel::snapshot()["generation"];auto response=native_panel::call(operation,values);ok=response.value("ok",false);if(!ok)result=response;}}
 }else if(method=="test.parity.open"){
  auto target=args.value("id",std::string());ok=target.empty()||target=="opensv2.lua-extend"||target=="opensv2.javascript-extend"||target=="sv2.mcp";if(ok)ok=native_category_registry::open_category(target.c_str())==0;
 }else if(method=="test.parity.network"){
  auto operation=args.value("operation",std::string("state"));int code=0;if(operation=="block"||operation=="allow")code=runtime_api->set_host_network_blocked(operation=="block");else if(operation=="open")ok=network_probe.start();else if(operation=="check")network_probe.check_closed();else if(operation=="close")network_probe.close();else if(operation=="diagnostic_socket_allow")windows_socket_gate::setblocked(false);else if(operation=="diagnostic_socket_block")windows_socket_gate::setblocked(true);else if(operation!="state")ok=false;
  loader_network_policy_state_v1 policy{LOADER_ABI_V1,sizeof policy,0,0,0,0,0};int policy_read=runtime_api->get_host_network_policy?runtime_api->get_host_network_policy(&policy):LOADER_ERR_NOT_SUPPORTED;loader_network_state_v1 network{LOADER_ABI_V1,sizeof network,0,0,0,0,0,0,0};int network_read=runtime_api->get_host_network_state(&network);auto probe=network_probe.status();auto job=network_scope::job();auto actual=network_scope::connections(GetCurrentProcessId());
  result={{"code",code},{"policy",{{"read",policy_read},{"generation",policy.generation},{"requested",policy.requested_blocked},{"applied",policy.applied_blocked},{"pending",policy.pending},{"error",policy.error}}},{"network",{{"read",network_read},{"generation",network.generation},{"requested",network.requested_blocked},{"active",network.tracked_host_tasks_active},{"drained",network.tracked_tasks_drained},{"verified",network.whole_host_verified},{"unverified",network.unverified_paths}}},{"probe",{{"pending",probe.pending},{"connected",probe.connected},{"error",probe.connectError},{"oldChecked",probe.socketChecked},{"oldClosed",probe.socketClosed},{"oldError",probe.socketError},{"newChecked",probe.newSocketChecked},{"newDenied",probe.newSocketDenied},{"newError",probe.newSocketError}}},{"job",{{"scope",job.child_scope},{"children",job.child_count},{"error",job.error}}},{"tables",{{"complete",actual.complete},{"tcp4",actual.tcp4_active},{"tcp6",actual.tcp6_active},{"udp4",actual.udp4_active},{"udp6",actual.udp6_active}}}};
 }else if(method=="test.parity.lease"){
  std::string op=args.value("operation",std::string());int code=op=="acquire"?runtime_api->acquire_profile_exclusive("test.parity","export-interlock"):op=="release"?runtime_api->release_profile_exclusive("test.parity","export-interlock"):LOADER_OK;loader_profile_lease_state_v1 state{1,sizeof(state),0,0,0};int read=runtime_api->get_profile_lease_state(&state);result={{"code",code},{"read",read},{"mode",state.mode},{"generation",state.generation}};
 }else if(method=="test.parity.render"){result=editor_autostart::render_call(args.value("operation",std::string()),args);}
 else if(method=="test.parity.render_status"){result=editor_autostart::render_status();}
 else if(method=="test.parity.render_request"){result=editor_autostart::render_request();}
 else if(method=="test.parity.audio"){result=audio_render_probe::snapshot();}
 else if(method=="test.parity.render_modal"){result=render_modal_probe::inspect();}
 else if(method=="test.parity.voice_error"){result=voice_error_probe::latest;}
 else if(method=="test.parity.voice_diagnostics"){result=voice_catalog_probe::snapshot(GetCurrentThreadId());}
 else if(method=="test.parity.storage"){
  const uint8_t bytes[]={0,3,4,250};int write=runtime_api->plugin_storage_write_scoped("test.parity","state",bytes,sizeof bytes);size_t size=0;int query=runtime_api->plugin_storage_read_scoped("test.parity","state",nullptr,&size);uint8_t result_bytes[4]{};size_t capacity=sizeof result_bytes;int read=runtime_api->plugin_storage_read_scoped("test.parity","state",result_bytes,&capacity);ok=write==0&&query==0&&read==0&&size==4&&capacity==4&&!memcmp(bytes,result_bytes,4);result={{"write",write},{"query",query},{"read",read},{"win32",GetLastError()}};
 }else ok=false;
 reply(done,context,{{"ok",ok},{"result",result}});
}
static void install(const loader_api_v1 &api,HWND runtime_window){runtime_api=&api;WSADATA wsa{};(void)runtime_window;if(WSAStartup(MAKEWORD(2,2),&wsa))throw std::runtime_error("socket initialization failed");json methods=json::array(),tools=json::array();for(const char *operation:{"state","run","ui","open","storage","render","render_status","render_request","voice_diagnostics","voice_error","render_modal","lease","audio","network"}){std::string method=std::string("test.parity.")+operation;methods.push_back(method);tools.push_back({{"name",std::string("svext_test_parity_")+operation},{"method",method},{"description","Verify runtime plugin parity in an owned test profile."},{"inputSchema",{{"type","object"},{"properties",json::object()}}}});}auto descriptor=json{{"methods",methods},{"tools",tools}}.dump();if(api.register_capability_provider("test.parity",dispatch,&owner,descriptor.c_str())!=0)throw std::runtime_error("test registration failed");}
}
