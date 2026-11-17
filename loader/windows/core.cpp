#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <filesystem>
#include <fstream>
#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <string>
#include <vector>
#include <atomic>
#include "vendor/json.hpp"
#include "../../sdk/include/loader_api.h"
#include "../../sdk/include/extend_platform.h"
#include "native_panel.h"
#include "native_category_registry.h"
#include "data_namespace.h"
#include "editor_gate.h"
#include "editor_autostart.h"
#include "script_runtime.h"
#include "script_settings.h"
#include "javascript_gate.h"
#include "javascript_bundle.h"
#include "plugin_storage.h"
#include "voice_service.h"
#include "export_native_service.h"
#include "profile_lease.h"
#include "startup_policy.h"
#include "network_control.h"
#include "network_scope.h"
#include "windows_socket_gate.h"
#include "windows_child_gate.h"
#include "windows_host_navigation.h"
#include "windows_webview_gate.h"
#include "windows_webview_startup.h"
#include "windows_web_retirement.h"
#include "network_bootstrap.h"
using json=nlohmann::json;namespace fs=std::filesystem;
namespace {
DWORD main_thread;HWND dispatcher;loader_api_v1 api{};std::string executable,host_directory;fs::path root;json package_results=json::array();bool managed=false,app_container=false;
std::vector<HMODULE> modules;
std::unique_ptr<script_runtime::Service> script_service;
std::unique_ptr<voice_service::Service> native_voice;
std::unique_ptr<export_service::Service> native_export;
std::unique_ptr<profile_lease::Service> profile_operations;
std::unique_ptr<startup_policy::Service> startup_network;
std::unique_ptr<windows_network::Controller> network_controller;
windows_network::State bootstrap_network_state;bool network_initialization_attempted=false;windows_webview_startup::Proof early_web_proof;
windows_web_retirement::Service web_retirement;bool web_retirement_polling=false;
script_settings::Runtime lua_runtime{&api,"opensv2.lua-extend","Lua Extend",false},js_runtime{&api,"opensv2.javascript-extend","JS Extend",false};
uint64_t capability_generation=1;
std::unique_ptr<editor_lua::Service> editor_service;int editor_gate_result=LOADER_ERR_NOT_SUPPORTED,editor_start_result=LOADER_ERR_NOT_SUPPORTED;
bool editor_online(){return editor_service&&!editor_service->epoch.empty()&&GetTickCount64()-editor_service->heartbeat<=2000;}
void log_message(const char *);
void network_tick();
void web_retirement_tick();
void category_update(void *pane,const char *id,HWND window,const RECT &bounds,bool visible){native_panel::dock_update(pane,id,window,bounds,visible);}
void category_remove(void *pane){native_panel::dock_remove(pane);}
bool category_sync_pending=false;
bool category_sync(){
 if(!native_category_registry::prepared)return true;auto snapshot=native_panel::categories();std::vector<native_category_registry::Entry> entries;entries.reserve(snapshot.size());for(const auto &entry:snapshot)entries.push_back({entry.id.c_str(),entry.title.c_str(),entry.visible});
 bool was_pending=category_sync_pending;category_sync_pending=!native_category_registry::sync(entries.data(),entries.size(),native_panel::extend_visible());if(category_sync_pending&&!was_pending)log_message("native-category-sync-unavailable");return !category_sync_pending;
}
void category_tick(){
 if(!editor_autostart::root_ready())return;
 if(!native_panel::host||!IsWindow(native_panel::host))EnumThreadWindows(main_thread,native_panel::find_host,reinterpret_cast<LPARAM>(&native_panel::host));
 if(!native_panel::host||!IsWindow(native_panel::host))return;
 if(native_category_registry::prepared){native_category_registry::set_host(native_panel::host);if(category_sync_pending)category_sync();native_category_registry::tick();}
 native_panel::diagnostic();
 auto state=native_category_registry::status();json entries=json::array();for(const auto &item:native_category_registry::categories)if(item.present){bool opened=false;bool known=native_category_registry::category_open(item.id,&opened)==0;entries.push_back({{"id",item.id},{"visible",item.visible},{"openKnown",known},{"open",known&&opened}});}auto body=json{{"hostPid",GetCurrentProcessId()},{"mainThreadId",main_thread},{"ready",state.ready},{"binding",native_category_registry::prepare_result},{"toolbarCount",state.toolbar_count},{"paneCount",state.pane_count},{"categories",entries}}.dump(2);static std::string previous;if(body==previous)return;
 std::ofstream output(root/L"outputs"/L"windows-category.json",std::ios::binary|std::ios::trunc);if(!output)return;output<<body;output.close();if(output)previous=body;
}
struct Provider{loader_editor_dispatch dispatch;void *owner;std::set<std::string> methods;json descriptor;};
std::map<std::string,Provider> providers;
struct Request{std::string method,arguments,provider;loader_json_completion completion;void *context;ULONGLONG deadline;std::atomic<bool> delivered{false};};
// Bounded tombstones keep late callback contexts valid until host exit.
std::vector<std::unique_ptr<Request>> requests;std::mutex requests_mutex;
bool on_main(){return GetCurrentThreadId()==main_thread;}
void log_message(const char *text){if(!text)return;std::string message="SV2_EXTEND pid="+std::to_string(GetCurrentProcessId())+" "+text+"\n";OutputDebugStringA(message.c_str());}
void reply(Request *r,const std::string &value){if(!r->delivered.exchange(true))r->completion(value.data(),value.size(),r->context);}
void error(Request *r,const char *code){reply(r,json{{"ok",false},{"error",{{"code",code}}}}.dump());}
bool id_valid(const std::string &id){if(id.empty()||id.size()>96||!isalpha((unsigned char)id[0]))return false;for(unsigned char c:id)if(!isalnum(c)&&c!='.'&&c!='_'&&c!='-')return false;return true;}
bool public_method(const std::string &method){if(!id_valid(method))return false;const auto domain=method.substr(0,method.find('.'));return domain!="account"&&domain!="session"&&domain!="license"&&domain!="private"&&domain!="auth"&&domain!="ui"&&domain!="runtime";}
void provider_reply(const char *body,size_t length,void *context){auto r=(Request*)context;if(!body||length>8*1024*1024){error(r,"INVALID_PROVIDER_REPLY");return;}auto parsed=json::parse(body,body+length,nullptr,false);if(parsed.is_discarded()||!parsed.is_object()){error(r,"INVALID_PROVIDER_REPLY");return;}reply(r,parsed.dump());}
int register_provider(const char *id,loader_editor_dispatch dispatch,void *owner,const char *descriptor){
 if(!on_main()||!id||!id_valid(id)||!dispatch||!descriptor||strlen(descriptor)>65536||providers.count(id)||providers.size()>=32)return LOADER_ERR_INVALID;
 auto value=json::parse(descriptor,nullptr,false);if(!value.is_object()||!value.contains("methods")||!value["methods"].is_array()||value["methods"].empty()||value["methods"].size()>256)return LOADER_ERR_INVALID;
 Provider candidate{dispatch,owner,{},value};
 for(const auto &method:value["methods"]){if(!method.is_string())return LOADER_ERR_INVALID;std::string name=method;if(!public_method(name)||!candidate.methods.insert(name).second)return LOADER_ERR_INVALID;for(const auto &pair:providers)if(pair.second.methods.count(name))return LOADER_ERR_INVALID;}
 if(value.contains("tools")){
  if(!value["tools"].is_array()||value["tools"].size()>256)return LOADER_ERR_INVALID;
  std::set<std::string> names;for(const auto &pair:providers)if(pair.second.descriptor.contains("tools"))for(const auto &tool:pair.second.descriptor["tools"])names.insert(tool["name"].get<std::string>());
  for(const auto &tool:value["tools"]){if(!tool.is_object()||!tool.contains("name")||!tool["name"].is_string()||!tool.contains("method")||!tool["method"].is_string()||!tool.contains("description")||!tool["description"].is_string()||!tool.contains("inputSchema")||!tool["inputSchema"].is_object()||!tool["inputSchema"].contains("type")||tool["inputSchema"]["type"]!="object")return LOADER_ERR_INVALID;auto name=tool["name"].get<std::string>();if(name.rfind("svext_",0)!=0||!id_valid(name)||!names.insert(name).second||!candidate.methods.count(tool["method"].get<std::string>()))return LOADER_ERR_INVALID;}
 }
 providers.emplace(id,std::move(candidate));++capability_generation;return LOADER_OK;
}
int unregister_provider(const char *id,void *owner){if(!on_main()||!id)return LOADER_ERR_INVALID;auto found=providers.find(id);if(found==providers.end()||found->second.owner!=owner)return LOADER_ERR_INVALID;providers.erase(found);++capability_generation;std::vector<Request*> pending;{std::lock_guard<std::mutex> guard(requests_mutex);for(auto &r:requests)if(r->provider==id&&!r->delivered)pending.push_back(r.get());}for(auto r:pending)error(r,"PROVIDER_REMOVED");return 0;}
json capability_snapshot(){json methods=json::array(),tools=json::array();for(auto &entry:providers){for(auto &method:entry.second.methods)methods.push_back(method);if(entry.second.descriptor.contains("tools"))for(auto &tool:entry.second.descriptor["tools"])tools.push_back(tool);}return {{"methods",methods},{"tools",tools},{"capabilityGeneration",capability_generation}};}
json script_call(const std::string &method,const json &args,bool javascript){
 if(!on_main())return editor_lua::Service::fail("MAIN_THREAD_REQUIRED");if(!args.is_object())return editor_lua::Service::fail("INVALID_ARGUMENT");
 if(const char *denied=script_settings::denial(javascript?js_runtime:lua_runtime,method,args))return editor_lua::Service::fail(denied);
 if(method=="runtime.info")return editor_lua::Service::success({{"pid",GetCurrentProcessId()},{"mainThread",true},{"apiVersion",1},{"transport","in-process-call"},{"usesEnvironmentState",false},{"usesFilesOrPipes",false},{"javascriptNativeAvailable",javascript_gate::ready()}});
 if(method=="runtime.capabilities")return editor_lua::Service::success(capability_snapshot());
 if(method.rfind("voice.",0)==0&&native_voice)return native_voice->call(method,args);
 if(method.rfind("export.",0)==0&&native_export)return native_export->call(method,args);
 if(method.rfind("ui.",0)==0)return native_panel::call(method,args);
 if(method.rfind("call.",0)==0)return script_service?script_service->call(method,args):editor_lua::Service::fail("UNSUPPORTED_HOST_CAPABILITY");
 return editor_service?editor_service->call(method,args):editor_lua::Service::fail("UNSUPPORTED_HOST_CAPABILITY");
}
void dispatch_request(Request *r){
 if(r->delivered)return;
 if(GetTickCount64()>=r->deadline){error(r,"TIMEOUT");return;}
 if(r->method=="runtime.info"){reply(r,json{{"ok",true},{"result",{{"hostPid",GetCurrentProcessId()},{"platform",loader_platform_key()},{"resident",true},{"editorReady",editor_online()},{"wholeHostNetworkVerified",false},{"namespaceRedirects",data_namespace::redirected.load()},{"namespaceOtherQueries",data_namespace::other.load()}}}}.dump());return;}
 if(r->method=="runtime.capabilities"){reply(r,json{{"ok",true},{"result",capability_snapshot()}}.dump());return;}
 if(r->method=="editor.capabilities"){json list=json::array();for(auto &pair:providers)list.push_back(json{{"id",pair.first},{"capabilities",pair.second.descriptor}});reply(r,json{{"ok",true},{"result",{{"providers",list},{"editorReady",editor_online()}}}}.dump());return;}
 for(auto &pair:providers)if(pair.second.methods.count(r->method)){r->provider=pair.first;auto dispatch=pair.second.dispatch;auto owner=pair.second.owner;dispatch(r->method.c_str(),r->arguments.c_str(),provider_reply,r,owner);return;}
 error(r,"METHOD_UNAVAILABLE");
}
void request(const char *method,const char *arguments,loader_json_completion completion,void *context){
 if(!completion)return;
 auto fail=[&](const char *code){std::string body=json{{"ok",false},{"error",{{"code",code}}}}.dump();completion(body.data(),body.size(),context);};
 if(!method||strlen(method)>128||!arguments||strlen(arguments)>1024*1024){fail("INVALID_ARGUMENT");return;}
 auto parsed=json::parse(arguments,nullptr,false);if(!parsed.is_object()){fail("INVALID_ARGUMENT");return;}
 auto value=std::make_unique<Request>();value->method=method;value->arguments=arguments;value->completion=completion;value->context=context;value->deadline=GetTickCount64()+6000;auto r=value.get();bool full=false;
 {std::lock_guard<std::mutex> guard(requests_mutex);full=requests.size()>=65536;if(!full)requests.push_back(std::move(value));}
 if(full){fail("REQUEST_LIMIT");return;}if(on_main())dispatch_request(r);else if(!PostMessageW(dispatcher,WM_APP+1,0,(LPARAM)r))error(r,"DISPATCH_UNAVAILABLE");
}
void namespace_receipt(){
 if(!managed)return;static uint64_t previous=UINT64_MAX;auto count=data_namespace::redirected.load()+data_namespace::other.load();if(count==previous)return;
 std::ofstream output(root/L"outputs"/L"windows-namespace.json",std::ios::binary|std::ios::trunc);if(!output)return;output<<json{{"mode","shell-entry-detours"},{"hostPid",GetCurrentProcessId()},{"redirectedQueries",data_namespace::redirected.load()},{"otherQueries",data_namespace::other.load()},{"lastOtherCSIDL",data_namespace::last_csidl.load()},{"lastOtherKnownId",data_namespace::last_known.load()}}.dump(2);output.close();if(output)previous=count;
}
void editor_receipt(){if(!managed)return;static unsigned attempts=0;static ULONGLONG attempted_at=0;auto now=GetTickCount64();auto state=editor_autostart::status();if(editor_start_result==LOADER_OK&&!state.scan_seen&&editor_autostart::root_ready()&&attempts<20&&now-attempted_at>=500){attempted_at=now;attempts++;editor_autostart::request_initial_scan();state=editor_autostart::status();}json value{{"hostPid",GetCurrentProcessId()},{"getenvGateResult",editor_gate_result},{"autostartResult",editor_start_result},{"installed",state.installed},{"scanSeen",state.scan_seen},{"runRequested",state.run_requested},{"scanAttempts",attempts},{"initialScanReason",state.initial_scan_reason},{"editorReady",editor_online()}};static std::string previous;auto body=value.dump(2);if(body==previous)return;std::ofstream output(root/L"outputs"/L"windows-editor.json",std::ios::binary|std::ios::trunc);if(!output)return;output<<body;output.close();if(output)previous=body;}
LRESULT CALLBACK window_proc(HWND window,UINT message,WPARAM w,LPARAM l){if(message==windows_host_navigation::message)return windows_host_navigation::receive(l);if(message==WM_APP+4){network_tick();return 0;}if(message==WM_APP+2){native_panel::drain();return 0;}if(message==WM_APP+1){dispatch_request((Request*)l);return 0;}if(message==WM_TIMER){network_tick();native_panel::drain();if(native_voice)native_voice->tick();if(native_export)native_export->tick();if(editor_service)editor_service->tick();std::vector<Request*> expired;{std::lock_guard<std::mutex> guard(requests_mutex);for(auto &r:requests)if(!r->delivered&&GetTickCount64()>=r->deadline)expired.push_back(r.get());}for(auto r:expired)error(r,"TIMEOUT");category_tick();namespace_receipt();editor_receipt();return 0;}return DefWindowProcW(window,message,w,l);}
void network_tick(){
 if(managed&&!network_initialization_attempted&&editor_autostart::root_ready()){
  network_initialization_attempted=true;
  try{auto host=fs::u8path(executable).wstring();network_controller=std::make_unique<windows_network::Controller>(host,bootstrap_network_state.generation);network_controller->adopt(bootstrap_network_state.requested_blocked);}
  catch(const std::exception&){bootstrap_network_state.policy_applied=false;bootstrap_network_state.error=LOADER_ERR_NETWORK_UNVERIFIED;log_message("network-controller-initialization-failed");}
 }
 if(network_controller){auto state=network_controller->state();if(state.policy_applied&&!state.pending&&!state.error&&!state.applied_blocked){windows_socket_gate::setblocked(false);windows_child_gate::setblocked(false);windows_webview_gate::setblocked(false);}}
 web_retirement_tick();
}
int network_policy(loader_network_policy_state_v1 *state){
 if(!on_main()||!state||state->abi_version!=LOADER_ABI_V1||state->struct_size<sizeof *state)return LOADER_ERR_INVALID;
 if(!managed)return LOADER_ERR_NOT_SUPPORTED;network_tick();auto value=network_controller?network_controller->state():bootstrap_network_state;
 *state={LOADER_ABI_V1,sizeof *state,value.generation,value.requested_blocked?1u:0u,value.applied_blocked?1u:0u,value.pending?1u:0u,value.error?value.error:(!value.policy_applied&&!value.pending?LOADER_ERR_NETWORK_UNVERIFIED:0)};return LOADER_OK;
}
int network_state(loader_network_state_v1 *state){
 if(!on_main()||!state||state->abi_version!=LOADER_ABI_V1||state->struct_size<sizeof *state)return LOADER_ERR_INVALID;
 if(!managed)return LOADER_ERR_NOT_SUPPORTED;network_tick();auto policy=network_controller?network_controller->state():bootstrap_network_state;auto sockets=windows_socket_gate::state();auto children=windows_child_gate::state();auto web=windows_webview_gate::state();auto scope=network_scope::job();auto actual=network_scope::connections(GetCurrentProcessId());auto children_after=windows_child_gate::state();
 uint32_t connections=actual.tcp4_active+actual.tcp6_active+actual.udp4_active+actual.udp6_active;
 bool drained=sockets.installed&&sockets.blocked&&!sockets.pending&&!sockets.discovery_error&&!sockets.active&&!sockets.unknown&&!sockets.close_failed&&actual.complete&&!connections;
 bool early=early_web_proof.valid(policy.generation,policy.requested_blocked,policy.applied_blocked,policy.policy_applied,policy.pending,policy.error!=0,network_controller!=nullptr);
 bool child_scope=children.installed&&children.blocked&&scope.child_scope&&children.generation==children_after.generation&&editor_gate_result==LOADER_OK&&windows_webview_gate::covered(web,early);
 uint64_t unverified=child_scope?0:LOADER_NET_WEBVIEW2|LOADER_NET_WEBRTC;
 if(!policy.policy_applied||!policy.applied_blocked||policy.pending||policy.error)unverified|=LOADER_NET_WINHTTP|LOADER_NET_WININET|LOADER_NET_WINSOCK;
 if(!drained)unverified|=LOADER_NET_WINSOCK;
 bool verified=policy.requested_blocked&&policy.policy_applied&&policy.applied_blocked&&!policy.pending&&!policy.error&&drained&&child_scope;
 uint64_t intercepted=sockets.installed?LOADER_NET_WINSOCK:0;if(policy.policy_applied&&policy.applied_blocked)intercepted|=LOADER_NET_WINHTTP|LOADER_NET_WININET;if(child_scope)intercepted|=LOADER_NET_WEBVIEW2|LOADER_NET_WEBRTC;
 *state={LOADER_ABI_V1,sizeof *state,policy.generation,intercepted,unverified,policy.requested_blocked?1u:0u,connections+scope.child_count,drained?1u:0u,verified?1u:0u};return LOADER_OK;
}
int verify(uint64_t generation,loader_network_state_v1 *state){int result=network_state(state);if(result)return result;if(generation!=state->generation)return LOADER_ERR_STALE_GENERATION;return state->whole_host_verified?LOADER_OK:LOADER_ERR_NETWORK_UNVERIFIED;}
int block(int value){if(!on_main()||(value!=0&&value!=1))return LOADER_ERR_INVALID;network_tick();if(!network_controller)return LOADER_ERR_PROFILE_BUSY;if(!network_controller->request(value!=0))return LOADER_ERR_PROFILE_BUSY;if(value){windows_webview_gate::setblocked(true);windows_child_gate::setblocked(true);windows_socket_gate::setblocked(true);}return LOADER_OK;}
int blocked(){return (network_controller?network_controller->state():bootstrap_network_state).requested_blocked?1:0;}
int http(const char*,loader_http_completion done,void *context){if(done)done(0,LOADER_ERR_NOT_SUPPORTED,nullptr,0,context);return LOADER_ERR_NOT_SUPPORTED;}
int storage(const char*,const uint8_t*,size_t){return LOADER_ERR_NOT_SUPPORTED;}
int storage_scoped(const char *id,const char *key,const uint8_t *bytes,size_t length){return plugin_storage::write(id,key,bytes,length);}
int storage_read(const char *id,const char *key,uint8_t *bytes,size_t *length){return plugin_storage::read(id,key,bytes,length);}
int register_editor(loader_editor_dispatch,void*,const char*){return LOADER_ERR_NOT_SUPPORTED;}
int unregister_editor(loader_editor_dispatch,void*){return LOADER_ERR_NOT_SUPPORTED;}
int listen(const char*,loader_http_json_handler,void*,loader_http_endpoint_v1*){return LOADER_ERR_NOT_SUPPORTED;}
int stop(const char*,loader_http_json_handler,void*){return LOADER_ERR_NOT_SUPPORTED;}
const char *host_data(){return managed?host_directory.c_str():nullptr;}
int guard(const char *owner,const char *id,int enabled){return startup_network?startup_network->set(owner,id,enabled):LOADER_ERR_NOT_SUPPORTED;}
int policy(loader_startup_network_state_v1 *state){return startup_network?startup_network->state(state):LOADER_ERR_NOT_SUPPORTED;}
void guards(const char *owner,loader_json_completion done,void *context){if(!done)return;auto value=startup_network?startup_network->list(owner):json{{"ok",false},{"error",{{"code","NOT_SUPPORTED"}}}};auto body=value.dump();done(body.data(),body.size(),context);}
int acquire_profile(const char *owner,const char *operation){
 if(!on_main()||!profile_operations)return LOADER_ERR_INVALID;
 bool busy=native_export&&(!native_export->active.empty()||native_export->recovery_required);
 if(export_native::root){void *project=nullptr,*renderer=nullptr;bool running=false,waiting=false,stopping=false;if(!export_service::project(project,renderer)||!export_service::render_flags(renderer,running,waiting,stopping))busy=true;else busy=busy||running||waiting||stopping;}
 int result=profile_operations->acquire(owner,operation,busy);if(result==LOADER_OK&&native_export)native_export->profile_exclusive=profile_operations->exclusive();return result;
}
int release_profile(const char *owner,const char *operation){if(!on_main()||!profile_operations)return LOADER_ERR_INVALID;int result=profile_operations->release(owner,operation);if(result==LOADER_OK&&native_export)native_export->profile_exclusive=profile_operations->exclusive();return result;}
int lease_state(loader_profile_lease_state_v1 *state){return profile_operations?profile_operations->state(state):LOADER_ERR_NOT_SUPPORTED;}
windows_web_retirement::Observation web_observation(){
 windows_web_retirement::Observation o;auto policy=network_controller?network_controller->state():bootstrap_network_state;
 o.networkGeneration=policy.generation;o.requested=policy.requested_blocked;o.policyApplied=policy.policy_applied&&policy.applied_blocked;o.policyPending=policy.pending;o.policyError=policy.error!=0;o.now=GetTickCount64();
 loader_profile_lease_state_v1 lease{LOADER_ABI_V1,sizeof lease,0,0,0};if(profile_operations&&profile_operations->state(&lease)==LOADER_OK){o.exclusive=lease.mode==LOADER_PROFILE_LEASE_EXCLUSIVE&&lease.host_pid==int32_t(GetCurrentProcessId());o.leaseGeneration=lease.generation;}
 auto web=windows_webview_gate::state();auto before=windows_child_gate::state();auto job=network_scope::job();auto after=windows_child_gate::state();
 o.gateBlocked=web.installed&&web.blocked&&before.installed&&before.blocked;o.coverage=web.coverage&&web.factoryBound;o.pending=web.pendingEnvironment||web.pendingController;o.controllers=web.controllers!=0;o.jobKnown=job.error==ERROR_SUCCESS&&job.self_present;o.jobEmpty=job.child_scope;o.childrenStable=before.generation==after.generation;return o;
}
void web_retirement_tick(){
 if(!web_retirement.active()||web_retirement_polling)return;web_retirement_polling=true;
 auto before=web_observation();bool attempted=web_retirement.may_retire(before);bool complete=attempted&&windows_webview_gate::retire();
 auto after=web_observation();if(attempted&&!complete)after.pending=true;web_retirement.tick(after);web_retirement_polling=false;
}
int retire_renderer(uint64_t,loader_json_completion,void*){return LOADER_ERR_NOT_SUPPORTED;}
int retire_web(uint64_t expected,loader_json_completion done,void *context){
 if(!on_main()||!done)return LOADER_ERR_INVALID;if(!managed||!windows_webview_gate::state().installed)return LOADER_ERR_NOT_SUPPORTED;
 int result=web_retirement.start(expected,web_observation(),done,context);if(result==LOADER_OK)PostMessageW(dispatcher,WM_APP+4,0,0);return result;
}
bool regular(const fs::path &path,bool directory=false){std::error_code error;auto status=fs::symlink_status(path,error);if(error||fs::is_symlink(status))return false;DWORD attr=GetFileAttributesW(path.c_str());return attr!=INVALID_FILE_ATTRIBUTES&&!(attr&FILE_ATTRIBUTE_REPARSE_POINT)&&(directory?fs::is_directory(status):fs::is_regular_file(status));}
void startup_receipt();
void load_packages(){
 auto folder=root/L"plugins";if(!regular(folder,true))return;
 std::set<std::string> ids;unsigned count=0;
 for(auto &item:fs::directory_iterator(folder)){
  try{
   if(count>=32)break;auto package=item.path();auto manifest=package/L"plugin.json";
   if(!regular(package,true)||!regular(manifest)||fs::file_size(manifest)>65536)continue;
   std::ifstream stream(manifest);json definition=json::parse(stream,nullptr,false);
   if(!definition.is_object()||definition.value("schema",0)!=1||!definition.contains("id")||!definition["id"].is_string())continue;
   auto id=definition["id"].get<std::string>();if(!id_valid(id)||ids.count(id))continue;
   if(definition.contains("enabled")&&(!definition["enabled"].is_boolean()||!definition["enabled"].get<bool>()))continue;
   if(!definition.contains("nativeLibraries")||!definition["nativeLibraries"].is_object())continue;
   auto libraries=definition["nativeLibraries"];auto key=loader_platform_key();if(!libraries.contains(key)||!libraries[key].is_string())continue;
   std::string relative=libraries[key];if(relative.size()>512||relative.find(':')!=std::string::npos)continue;
   fs::path path=fs::u8path(relative);if(path.is_absolute()||path.extension()!=L".dll")continue;
   bool valid=true;auto cursor=package;for(const auto &part:path){if(part==L".."||part==L"."){valid=false;break;}cursor/=part;if(!regular(cursor,cursor!=package/path)){valid=false;break;}}
   if(!valid)continue;
   auto absolute=(package/path).lexically_normal();auto base=package.lexically_normal();auto rel=absolute.lexically_relative(base);if(rel.empty()||*rel.begin()==L"..")continue;
   std::vector<std::wstring> helper_paths;
   if(definition.contains("nativeHelpers")){
    if(!definition["nativeHelpers"].is_object())continue;auto helpers=definition["nativeHelpers"].value(key,json::array());if(!helpers.is_array()||helpers.size()>8)continue;bool helper_valid=true;
    for(const auto &entry:helpers){if(!entry.is_string()){helper_valid=false;break;}auto text=entry.get<std::string>();fs::path helper=fs::u8path(text);if(text.empty()||text.size()>512||text.find(':')!=std::string::npos||helper.is_absolute()||helper.extension()!=L".exe"){helper_valid=false;break;}auto cursor=package;for(const auto &part:helper){if(part==L".."||part==L"."){helper_valid=false;break;}cursor/=part;if(!regular(cursor,cursor!=package/helper)){helper_valid=false;break;}}std::wstring physical;if(!helper_valid||!windows_network::canonical_executable_path((package/helper).wstring(),physical)){helper_valid=false;break;}helper_paths.push_back(std::move(physical));}
    if(!helper_valid)continue;
   }
   auto module=LoadLibraryExW(absolute.c_str(),nullptr,LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR|LOAD_LIBRARY_SEARCH_SYSTEM32);if(!module){package_results.push_back({{"id",id},{"loaded",false}});log_message("package-load-failed");continue;}
   bool helpers_registered=true;for(const auto &helper:helper_paths)if(!windows_child_gate::register_helper(helper)){helpers_registered=false;break;}if(!helpers_registered){for(const auto &helper:helper_paths)windows_child_gate::unregister_helper(helper);FreeLibrary(module);package_results.push_back({{"id",id},{"loaded",false}});log_message("package-helper-registration-failed");continue;}
   modules.push_back(module);ids.insert(id);count++;auto initialize=(loader_plugin_init_v1_fn)GetProcAddress(module,"loader_plugin_init_v1");int result=initialize?initialize(&api):LOADER_ERR_INVALID;package_results.push_back({{"id",id},{"loaded",true},{"initResult",result}});log_message(result==0?"package-resident-ready":"package-init-failed");
   if(result!=LOADER_OK)for(const auto &helper:helper_paths)windows_child_gate::unregister_helper(helper);
   if(result==LOADER_ERR_STORAGE_IO||result==LOADER_ERR_POLICY_INVALID){startup_receipt();log_message("startup-policy-or-storage-failed");TerminateProcess(GetCurrentProcess(),ERROR_DLL_INIT_FAILED);ExitProcess(ERROR_DLL_INIT_FAILED);}
  }catch(const std::exception&){log_message("package-invalid");}
 }
}
bool token_is_app_container(){HANDLE token=nullptr;if(!OpenProcessToken(GetCurrentProcess(),TOKEN_QUERY,&token))return false;DWORD value=0,size=0;bool result=GetTokenInformation(token,TokenIsAppContainer,&value,sizeof(value),&size)&&size==sizeof(value)&&value!=0;CloseHandle(token);return result;}
bool flag_enabled(const wchar_t *key){wchar_t value[4]{};return GetEnvironmentVariableW(key,value,4)==1&&value[0]==L'1';}
bool read_path(const wchar_t *key,fs::path &out){wchar_t path[32768]{};DWORD n=GetEnvironmentVariableW(key,path,32768);if(!n||n>=32768)return false;fs::path parsed(path);if(!parsed.is_absolute()||!parsed.has_root_name())return false;out=parsed.lexically_normal();return out.native().size()<32768;}
void startup_receipt(){std::error_code ec;auto directory=root/L"outputs";fs::create_directories(directory,ec);if(ec||!regular(directory,true)){log_message("runtime-receipt-directory-unavailable");return;}auto file=directory/L"windows-runtime.json";if(fs::exists(file,ec)&&!regular(file)){log_message("runtime-receipt-path-invalid");return;}std::ofstream stream(file,std::ios::binary|std::ios::trunc);if(!stream){log_message("runtime-receipt-write-failed");return;}stream<<json{{"hostPid",GetCurrentProcessId()},{"mainThreadId",main_thread},{"appContainer",app_container},{"managed",managed},{"hostDataNamespace",managed},{"pluginUi",true},{"packages",package_results}}.dump(2);stream.close();if(!stream)log_message("runtime-receipt-write-failed");}
}
void loader_export_set_root(void *root){export_native::set_root(root);}
void loader_category_before_root(){if(managed&&on_main()){if(native_category_registry::prepare(reinterpret_cast<unsigned char*>(GetModuleHandleW(nullptr)),main_thread,category_update,category_remove,native_panel::owns_child))category_sync();else log_message("native-category-unavailable");}}
void loader_category_after_root(){if(managed&&on_main()){if(!SetTimer(dispatcher,1,100,nullptr))log_message("runtime-timer-start-failed");category_tick();}}
void loader_category_cleanup_root(){if(managed&&on_main())native_category_registry::set_host(nullptr);}
extern "C" __declspec(dllexport) int ExtendInitialize(void*){
 if(dispatcher)return main_thread==GetCurrentThreadId()?0:LOADER_ERR_INVALID;
 windows_webview_startup::Proof::Scope early_scope(early_web_proof);
 app_container=token_is_app_container();managed=flag_enabled(L"OPEN_SV2_WINDOWS_MANAGED");if(managed&&app_container)return LOADER_ERR_NOT_SUPPORTED;if(!managed&&!flag_enabled(L"OPEN_SV2_WINDOWS_FIXTURE"))return LOADER_ERR_NOT_SUPPORTED;
 main_thread=GetCurrentThreadId();if(!read_path(L"OPEN_SV2_LOADER_DATA_DIR",root))return LOADER_ERR_STORAGE_NOT_CONFIGURED;if(!regular(root,true))return LOADER_ERR_INVALID;if(!plugin_storage::install(root))return LOADER_ERR_STORAGE_IO;
 if(managed){fs::path actual;if(!read_path(L"OPEN_SV2_HOST_DATA_DIR",actual)||CompareStringOrdinal(actual.c_str(),-1,(root/L"host-data").c_str(),-1,TRUE)!=CSTR_EQUAL||!regular(actual,true)||actual.native().size()+12>=MAX_PATH||!data_namespace::install(actual.wstring()))return LOADER_ERR_INVALID;host_directory=actual.u8string();}
 WNDCLASSW cls{};cls.lpfnWndProc=window_proc;cls.hInstance=GetModuleHandleW(nullptr);cls.lpszClassName=L"SV2Extend.Resident.Dispatch.v1";
 if(!RegisterClassW(&cls))return LOADER_ERR_INVALID;
 dispatcher=CreateWindowExW(0,cls.lpszClassName,L"",0,0,0,0,0,HWND_MESSAGE,nullptr,cls.hInstance,nullptr);if(!dispatcher||!SetTimer(dispatcher,1,100,nullptr)||native_panel::install(dispatcher,root.c_str())!=LOADER_OK)return LOADER_ERR_INVALID;
 windows_host_navigation::install(dispatcher,main_thread);
 wchar_t host[32768];DWORD n=GetModuleFileNameW(nullptr,host,32768);if(!n||n>=32768)return LOADER_ERR_INVALID;executable=fs::path(host).u8string();
 if(managed){
  startup_network=std::make_unique<startup_policy::Service>(main_thread);loader_startup_network_state_v1 startup{LOADER_ABI_V1,sizeof startup,0,0,0,0,0};
  if(startup_network->state(&startup)!=LOADER_OK||!windows_socket_gate::install()||!windows_child_gate::install()||!windows_webview_gate::install())return LOADER_ERR_POLICY_INVALID;
  windows_child_gate::navigation=windows_host_navigation::route;
  wchar_t handle[32]{};DWORD length=GetEnvironmentVariableW(L"OPEN_SV2_NETWORK_BOOTSTRAP_HANDLE",handle,32);wchar_t *end=nullptr;auto raw=length&&length<32?_wcstoui64(handle,&end,10):0;windows_network_bootstrap::Record receipt;
  bool observed=raw&&end&&!*end&&windows_network_bootstrap::read(reinterpret_cast<HANDLE>(uintptr_t(raw)),receipt);
  if(observed){early_web_proof.attest(receipt.generation,receipt.requested!=0,receipt.applied!=0,receipt.verified!=0);bootstrap_network_state.generation=receipt.generation;bootstrap_network_state.requested_blocked=receipt.requested!=0;bootstrap_network_state.applied_blocked=receipt.applied!=0;bootstrap_network_state.policy_applied=true;}
  if(startup.requested_blocked){if(!observed||!receipt.requested||!receipt.applied||!windows_socket_gate::setblocked(true))return LOADER_ERR_POLICY_INVALID;startup_network->mark_applied();}
  else{bootstrap_network_state.requested_blocked=false;windows_child_gate::setblocked(false);windows_webview_gate::setblocked(false);}
 }
 profile_operations=std::make_unique<profile_lease::Service>(main_thread,managed);
 api.abi_version=1;api.struct_size=sizeof api;api.capabilities=LOADER_CAP_NETWORK_STATE|LOADER_CAP_PLUGIN_UI|LOADER_CAP_SCOPED_STORAGE|(managed?(LOADER_CAP_HOST_DATA_NAMESPACE|LOADER_CAP_PROFILE_LEASE):0);api.host_pid=GetCurrentProcessId();api.on_main_thread=1;api.host_executable=executable.c_str();api.log=log_message;
 api.set_host_network_blocked=block;api.host_network_blocked=blocked;api.plugin_http_get=http;api.get_host_network_state=network_state;api.verify_host_network_quiescence=verify;api.plugin_storage_write=storage;
 api.register_editor_adapter=register_editor;api.unregister_editor_adapter=unregister_editor;api.editor_request_json=request;api.request_capability_json=request;api.register_capability_provider=register_provider;api.unregister_capability_provider=unregister_provider;
 api.listen_local_json=listen;api.stop_local_json=stop;api.host_data_directory=host_data;api.register_plugin_ui=native_panel::register_ui;api.unregister_plugin_ui=native_panel::unregister_ui;api.update_plugin_ui=native_panel::update_ui;
 api.set_startup_network_guard=guard;api.get_startup_network_policy=policy;api.list_startup_network_guards=guards;api.acquire_profile_exclusive=acquire_profile;api.release_profile_exclusive=release_profile;api.get_profile_lease_state=lease_state;api.retire_host_web_renderers=retire_renderer;api.retire_host_web_contexts=retire_web;api.plugin_storage_write_scoped=storage_scoped;api.plugin_storage_read_scoped=storage_read;
 api.get_host_network_policy=network_policy;api.register_host_navigation_handler=windows_host_navigation::add;api.unregister_host_navigation_handler=windows_host_navigation::remove;if(managed)api.capabilities|=LOADER_CAP_NETWORK_POLICY_SWITCH|LOADER_CAP_STARTUP_NETWORK_GUARD|LOADER_CAP_WEB_CONTEXT_RETIREMENT|LOADER_CAP_HOST_NAVIGATION;
 if(managed){editor_service=std::make_unique<editor_lua::Service>(&api);editor_gate_result=editor_gate::install(editor_service.get());if(editor_gate_result==LOADER_OK){auto entry=root/L"host-data"/L"Roaming"/L"Dreamtonics"/L"Synthesizer V Studio 2"/L"scripts"/L"opensv2-loader-runtime.lua";editor_start_result=editor_autostart::install(editor_service.get(),entry.c_str());}}
 native_panel::set_categories_changed(category_sync);native_panel::set_open_category(native_category_registry::open_category);native_category_registry::set_scripts_callbacks(native_panel::scripts_update,native_panel::scripts_remove);
 script_service=std::make_unique<script_runtime::Service>(&api);editor_gate::set_router(script_call);
 native_export=std::make_unique<export_service::Service>();native_export->available=managed&&editor_gate_result==LOADER_OK&&export_native::install(main_thread);
 native_voice=std::make_unique<voice_service::Service>(main_thread);if(managed)native_voice->installed=voice_adapter::install(main_thread)==0;
 lua_runtime.ready=editor_gate_result==LOADER_OK;if(managed)javascript_gate::install(editor_gate::call_json,loaderJavaScriptModule);js_runtime.ready=javascript_gate::ready();script_settings::register_runtime(lua_runtime,true);script_settings::register_runtime(js_runtime,false);
 load_packages();startup_receipt();log_message("windows-resident-ready");return 0;
}
extern "C" __declspec(dllexport) const loader_api_v1 *ExtendGetAPI(){return dispatcher?&api:nullptr;}
BOOL WINAPI DllMain(HINSTANCE,DWORD,LPVOID){return TRUE;}
