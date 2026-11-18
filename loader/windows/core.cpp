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
#include "../packages/package_model.h"
#include "../packages/package_runtime.h"
#include "package_network.h"
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
windows_network::Controller* network_controller=nullptr;
windows_network::State bootstrap_network_state;bool network_initialization_attempted=false;windows_webview_startup::Proof early_web_proof;
script_settings::Runtime lua_runtime{&api,"opensv2.lua-extend","Lua Extend",false},js_runtime{&api,"opensv2.javascript-extend","JS Extend",false};
uint64_t capability_generation=1;
std::unique_ptr<editor_lua::Service> editor_service;int editor_gate_result=LOADER_ERR_NOT_SUPPORTED,editor_start_result=LOADER_ERR_NOT_SUPPORTED;
bool editor_online(){return editor_service&&!editor_service->epoch.empty()&&GetTickCount64()-editor_service->heartbeat<=2000;}
void log_message(const char *);
void network_tick();
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
struct Provider{loader_editor_dispatch dispatch;void *owner;std::set<std::string> methods;json descriptor;std::string package;};
struct Package{package_model::Manifest manifest;fs::path folder;json definition,grants=json::object();std::string state="pending",reason,last;bool visiting=false,provider=false,panel=false,enabled=true,resident=false;int init_result=0;std::shared_ptr<package_runtime::Runtime> runtime;std::unique_ptr<package_network::Server> server;unsigned long long server_serial=0;};
std::map<std::string,std::unique_ptr<Package>> packages;package_model::Environment environment;std::string current_package;json reload_packages();
std::map<std::string,Provider> providers;
struct Request{std::string method,arguments,provider;loader_json_completion completion;void *context;ULONGLONG deadline;std::atomic<bool> delivered{false};};
// Bounded tombstones keep late callback contexts valid until host exit.
std::vector<std::unique_ptr<Request>> requests;std::mutex requests_mutex;
bool on_main(){return GetCurrentThreadId()==main_thread;}
void log_message(const char *text){if(!text)return;std::string message="SV2_EXTEND pid="+std::to_string(GetCurrentProcessId())+" "+text+"\n";OutputDebugStringA(message.c_str());}
void reply(Request *r,const std::string &value){if(!r->delivered.exchange(true))r->completion(value.data(),value.size(),r->context);}
void error(Request *r,const char *code){reply(r,json{{"ok",false},{"error",{{"code",code}}}}.dump());}
bool id_valid(const std::string &id){if(id.empty()||id.size()>96||!isalpha((unsigned char)id[0]))return false;for(unsigned char c:id)if(!isalnum(c)&&c!='.'&&c!='_'&&c!='-')return false;return true;}
bool public_method(const std::string &method){return id_valid(method)&&!package_model::reserved_domain(method);}
void provider_reply(const char *body,size_t length,void *context){auto r=(Request*)context;if(!body||length>8*1024*1024){error(r,"INVALID_PROVIDER_REPLY");return;}auto parsed=json::parse(body,body+length,nullptr,false);if(parsed.is_discarded()||!parsed.is_object()){error(r,"INVALID_PROVIDER_REPLY");return;}reply(r,parsed.dump());}
int register_provider(const char *id,loader_editor_dispatch dispatch,void *owner,const char *descriptor){
 if(!on_main()||!id||!id_valid(id)||!dispatch||!descriptor||strlen(descriptor)>65536||providers.count(id)||providers.size()>=32)return LOADER_ERR_INVALID;
 auto value=json::parse(descriptor,nullptr,false);if(!value.is_object()||!value.contains("methods")||!value["methods"].is_array()||value["methods"].empty()||value["methods"].size()>256)return LOADER_ERR_INVALID;
 Provider candidate{dispatch,owner,{},value,current_package};
 for(const auto &method:value["methods"]){if(!method.is_string())return LOADER_ERR_INVALID;std::string name=method;if(!public_method(name)||!candidate.methods.insert(name).second)return LOADER_ERR_INVALID;for(const auto &pair:providers)if(pair.second.methods.count(name))return LOADER_ERR_INVALID;}
 if(value.contains("tools")){
  if(!value["tools"].is_array()||value["tools"].size()>256)return LOADER_ERR_INVALID;
  std::set<std::string> names;for(const auto &pair:providers)if(pair.second.descriptor.contains("tools"))for(const auto &tool:pair.second.descriptor["tools"])names.insert(tool["name"].get<std::string>());
  for(const auto &tool:value["tools"]){if(!tool.is_object()||!tool.contains("name")||!tool["name"].is_string()||!tool.contains("method")||!tool["method"].is_string()||!tool.contains("description")||!tool["description"].is_string()||!tool.contains("inputSchema")||!tool["inputSchema"].is_object()||!tool["inputSchema"].contains("type")||tool["inputSchema"]["type"]!="object")return LOADER_ERR_INVALID;auto name=tool["name"].get<std::string>();if(name.rfind("svext_",0)!=0||!id_valid(name)||!names.insert(name).second||!candidate.methods.count(tool["method"].get<std::string>()))return LOADER_ERR_INVALID;}
 }
 providers.emplace(id,std::move(candidate));++capability_generation;return LOADER_OK;
}
int unregister_provider(const char *id,void *owner){if(!on_main()||!id)return LOADER_ERR_INVALID;auto found=providers.find(id);if(found==providers.end()||found->second.owner!=owner)return LOADER_ERR_INVALID;providers.erase(found);++capability_generation;std::vector<Request*> pending;{std::lock_guard<std::mutex> guard(requests_mutex);for(auto &r:requests)if(r->provider==id&&!r->delivered)pending.push_back(r.get());}for(auto r:pending)error(r,"PROVIDER_REMOVED");return 0;}
json capability_snapshot(){json methods=json::array(),tools=json::array();for(auto &entry:providers){for(auto &method:entry.second.methods)methods.push_back(method);if(entry.second.descriptor.contains("tools"))for(auto &tool:entry.second.descriptor["tools"])tools.push_back(tool);}json list=json::array();for(auto &entry:packages){auto &package=*entry.second;json grants=json::object();for(const auto &key:package_model::grantable(package.manifest,environment))grants[key]=package.grants.value("grant."+key,false);list.push_back({{"id",entry.first},{"name",package.manifest.name},{"version",package.manifest.version},{"kind",package.manifest.native?"native":"declarative"},{"state",package.state},{"reason",package.reason},{"permissions",grants}});}return {{"methods",methods},{"tools",tools},{"capabilityGeneration",capability_generation},{"loader",package_model::describe(environment)},{"packages",list}};}
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
 if(r->method=="runtime.packages.reload"){reply(r,json{{"ok",true},{"result",{{"packages",reload_packages()}}}}.dump());return;}
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
LRESULT CALLBACK window_proc(HWND window,UINT message,WPARAM w,LPARAM l){if(message==windows_host_navigation::message)return windows_host_navigation::receive(l);if(message==WM_APP+4){network_tick();return 0;}if(message==WM_APP+2){native_panel::drain();return 0;}if(message==WM_APP+1){dispatch_request((Request*)l);return 0;}if(message==WM_APP+5){std::unique_ptr<std::function<void()>> task(reinterpret_cast<std::function<void()>*>(l));(*task)();return 0;}if(message==WM_TIMER){network_tick();native_panel::drain();if(native_voice)native_voice->tick();if(native_export)native_export->tick();if(editor_service)editor_service->tick();std::vector<Request*> expired;{std::lock_guard<std::mutex> guard(requests_mutex);for(auto &r:requests)if(!r->delivered&&GetTickCount64()>=r->deadline)expired.push_back(r.get());}for(auto r:expired)error(r,"TIMEOUT");category_tick();namespace_receipt();editor_receipt();return 0;}return DefWindowProcW(window,message,w,l);}
void network_tick(){
 if(managed&&!network_initialization_attempted&&editor_autostart::root_ready()){
  network_initialization_attempted=true;
  try{auto host=fs::u8path(executable).wstring();network_controller=windows_network::make_resident_controller(reinterpret_cast<const void*>(&network_tick),host,bootstrap_network_state.generation);if(!network_controller)throw std::runtime_error("resident module unavailable");network_controller->adopt(bootstrap_network_state.requested_blocked);}
  catch(const std::exception&){bootstrap_network_state.policy_applied=false;bootstrap_network_state.error=LOADER_ERR_NETWORK_UNVERIFIED;log_message("network-controller-initialization-failed");}
 }
 if(managed&&editor_autostart::root_ready()&&!windows_webview_transport::state().installed&&!windows_webview_gate::start_transport())log_message("webview-transport-initialization-failed");
 if(network_controller){auto state=network_controller->state();if(state.policy_applied&&!state.pending&&!state.error&&!state.requested_blocked&&!state.applied_blocked){windows_socket_gate::setblocked(false);windows_child_gate::setblocked(false);windows_webview_gate::setblocked(false);}}
 if(network_controller){auto policy=network_controller->state();if(policy.requested_blocked){auto transport=windows_webview_transport::state();if(transport.blocked&&!transport.pending&&!transport.active&&!windows_socket_gate::state().blocked)windows_socket_gate::setblocked(true);}}
 if(managed){auto web=windows_webview_gate::state();auto transport=windows_webview_transport::state();json value{{"hostPid",GetCurrentProcessId()},{"installed",web.installed},{"blocked",web.blocked},{"factoryBound",web.factoryBound},{"coverage",web.coverage},{"denied",web.denied},{"controllers",web.controllers},{"pendingEnvironments",web.pendingEnvironment},{"pendingControllers",web.pendingController},{"transport",{{"installed",transport.installed},{"blocked",transport.blocked},{"pending",transport.pending},{"active",transport.active},{"coverage",transport.coverage}}}};static std::string previous;auto body=value.dump(2);if(body!=previous){std::ofstream output(root/L"outputs"/L"windows-webview.json",std::ios::binary|std::ios::trunc);if(output){output<<body;output.close();if(output)previous=body;}}}
}
int network_policy(loader_network_policy_state_v1 *state){
 if(!on_main()||!state||state->abi_version!=LOADER_ABI_V1||state->struct_size<sizeof *state)return LOADER_ERR_INVALID;
 if(!managed)return LOADER_ERR_NOT_SUPPORTED;network_tick();auto value=network_controller?network_controller->state():bootstrap_network_state;
 *state={LOADER_ABI_V1,sizeof *state,value.generation,value.requested_blocked?1u:0u,value.applied_blocked?1u:0u,value.pending?1u:0u,value.error?value.error:(!value.policy_applied&&!value.pending?LOADER_ERR_NETWORK_UNVERIFIED:0)};return LOADER_OK;
}
int network_state(loader_network_state_v1 *state){
 if(!on_main()||!state||state->abi_version!=LOADER_ABI_V1||state->struct_size<sizeof *state)return LOADER_ERR_INVALID;
 if(!managed)return LOADER_ERR_NOT_SUPPORTED;network_tick();auto policy=network_controller?network_controller->state():bootstrap_network_state;auto sockets=windows_socket_gate::state();auto children=windows_child_gate::state();auto web=windows_webview_gate::state();auto actual=network_scope::connections(GetCurrentProcessId());
 uint32_t connections=(actual.tcp4_active-actual.tcp4_loopback)+(actual.tcp6_active-actual.tcp6_loopback)+(actual.udp4_active-actual.udp4_loopback)+(actual.udp6_active-actual.udp6_loopback);
 bool drained=sockets.installed&&sockets.blocked&&!sockets.pending&&!sockets.discovery_error&&!sockets.active&&!sockets.unknown&&!sockets.close_failed&&actual.complete&&!connections;
 bool early=early_web_proof.valid(policy.generation,policy.requested_blocked,policy.applied_blocked,policy.policy_applied,policy.pending,policy.error!=0,network_controller!=nullptr);
 auto job=network_scope::job();bool observed=job.error==ERROR_SUCCESS&&job.self_present;
 if(observed)for(auto pid:job.members)if(!windows_webview_gate::process_covered(pid)){observed=false;break;}
 auto after=network_scope::job();std::sort(job.members.begin(),job.members.end());std::sort(after.members.begin(),after.members.end());observed=observed&&after.error==ERROR_SUCCESS&&after.self_present&&job.members==after.members;
 bool child_scope=observed&&children.installed&&children.blocked&&editor_gate_result==LOADER_OK&&windows_webview_gate::covered(web,early);
 uint64_t unverified=child_scope?0:LOADER_NET_WEBVIEW2|LOADER_NET_WEBRTC;
 if(!policy.policy_applied||!policy.applied_blocked||policy.pending||policy.error)unverified|=LOADER_NET_WINHTTP|LOADER_NET_WININET|LOADER_NET_WINSOCK;
 if(!drained)unverified|=LOADER_NET_WINSOCK;
 bool verified=policy.requested_blocked&&policy.policy_applied&&policy.applied_blocked&&!policy.pending&&!policy.error&&drained&&child_scope;
 uint64_t intercepted=sockets.installed?LOADER_NET_WINSOCK:0;if(policy.policy_applied&&policy.applied_blocked)intercepted|=LOADER_NET_WINHTTP|LOADER_NET_WININET;if(child_scope)intercepted|=LOADER_NET_WEBVIEW2|LOADER_NET_WEBRTC;
 *state={LOADER_ABI_V1,sizeof *state,policy.generation,intercepted,unverified,policy.requested_blocked?1u:0u,connections+windows_webview_transport::state().active,drained?1u:0u,verified?1u:0u};return LOADER_OK;
}
int verify(uint64_t generation,loader_network_state_v1 *state){int result=network_state(state);if(result)return result;if(generation!=state->generation)return LOADER_ERR_STALE_GENERATION;return state->whole_host_verified?LOADER_OK:LOADER_ERR_NETWORK_UNVERIFIED;}
int block(int value){if(!on_main()||(value!=0&&value!=1))return LOADER_ERR_INVALID;network_tick();if(!network_controller)return LOADER_ERR_PROFILE_BUSY;if(!network_controller->request(value!=0))return LOADER_ERR_PROFILE_BUSY;if(value){windows_webview_gate::setblocked(true);windows_child_gate::setblocked(true);auto transport=windows_webview_transport::state();if(!transport.pending&&!transport.active)windows_socket_gate::setblocked(true);}return LOADER_OK;}
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
int retire_renderer(uint64_t,loader_json_completion,void*){return LOADER_ERR_NOT_SUPPORTED;}
bool regular(const fs::path &path,bool directory=false){std::error_code error;auto status=fs::symlink_status(path,error);if(error||fs::is_symlink(status))return false;DWORD attr=GetFileAttributesW(path.c_str());return attr!=INVALID_FILE_ATTRIBUTES&&!(attr&FILE_ATTRIBUTE_REPARSE_POINT)&&(directory?fs::is_directory(status):fs::is_regular_file(status));}
void startup_receipt();
// Declarative packages have no code of their own: Loader performs every call
// on their behalf, so the grant check below sees a trustworthy caller.
struct Forward{loader_json_completion complete;void *context;};
void forward_reply(const char *body,size_t length,void *context){auto forward=static_cast<Forward*>(context);forward->complete(body,length,forward->context);delete forward;}
const char *permission_key(const Package &package,const std::string &target,std::string &key){
 if(auto known=package_model::capability_for_method(target)){if(!package.manifest.permits(known->id))return "PERMISSION_UNDECLARED";if(!environment.capabilities.count(known->id))return "CAPABILITY_UNSUPPORTED";key=known->id;return nullptr;}
 // Dependencies form an acyclic graph, so a call into a required package cannot come back to its caller.
 for(const auto &pair:providers)if(pair.second.methods.count(target)){auto owner=packages.find(pair.second.package);if(owner==packages.end()||owner->second.get()==&package||!package.manifest.packages.count(owner->first))return "TARGET_NOT_ALLOWED";key="package."+owner->first;return package.manifest.permits(key)?nullptr:"PERMISSION_UNDECLARED";}
 return "METHOD_UNAVAILABLE";
}
json storage_call(const Package &package,const std::string &method,const json &arguments){
 if(!arguments.contains("key")||!arguments["key"].is_string())return {{"ok",false},{"error",{{"code","INVALID_ARGUMENT"}}}};
 auto id=package.manifest.id;auto key=arguments["key"].get<std::string>();int status=LOADER_OK;json result=json::object();
 if(method=="storage.write"){if(!arguments.contains("value"))return {{"ok",false},{"error",{{"code","INVALID_ARGUMENT"}}}};auto data=arguments["value"].dump();status=plugin_storage::write(id.c_str(),key.c_str(),reinterpret_cast<const uint8_t*>(data.data()),data.size());}
 else{
  size_t length=0;status=plugin_storage::read(id.c_str(),key.c_str(),nullptr,&length);
  if(status==LOADER_ERR_NOT_FOUND)return {{"ok",true},{"result",{{"found",false},{"value",nullptr}}}};
  if(status==LOADER_OK||status==LOADER_ERR_BUFFER_TOO_SMALL){std::vector<uint8_t> data(length);status=plugin_storage::read(id.c_str(),key.c_str(),data.data(),&length);auto value=status==LOADER_OK?json::parse(data.begin(),data.begin()+length,nullptr,false):json();if(status==LOADER_OK&&value.is_discarded())status=LOADER_ERR_STORAGE_IO;result={{"found",true},{"value",value}};}
 }
 if(status!=LOADER_OK)return {{"ok",false},{"error",{{"code",status==LOADER_ERR_INVALID?"INVALID_ARGUMENT":"STORAGE_ERROR"},{"status",status}}}};
 return {{"ok",true},{"result",result}};
}
// Runs a task on the main thread, where package state and package code live.
bool post_main(std::function<void()> task){auto queued=new std::function<void()>(std::move(task));if(PostMessageW(dispatcher,WM_APP+5,0,reinterpret_cast<LPARAM>(queued)))return true;delete queued;return false;}
json listen_call(Package &package,const json &arguments){
 auto name=arguments.value("handler",json());auto mode=arguments.value("auth",json("token"));
 if(!name.is_string()||!mode.is_string()||(mode!="token"&&mode!="none"))return package_network::failure("INVALID_ARGUMENT");
 if(!package.runtime||!package.runtime->handles(name.get<std::string>()))return package_network::failure("HANDLER_UNAVAILABLE");
 auto target=&package;auto handler=name.get<std::string>();package.server.reset();
 // The serial ties a request to the listener that accepted it, so a closed listener's late requests are refused.
 static unsigned long long serials=0;auto serial=++serials;package.server_serial=serial;
 package.server=package_network::Server::listen([target,handler,serial](json request,package_network::Done respond){
  if(!post_main([target,handler,serial,request,respond]{if(target->server_serial!=serial||!target->server||!target->runtime)respond(package_network::failure("PACKAGE_UNLOADED"));else target->runtime->invoke(handler,request,respond);}))respond(package_network::failure("PACKAGE_UNLOADED"));
 },mode=="token");
 if(!package.server)return package_network::failure("LISTEN_FAILED");
 json result{{"port",package.server->port},{"url","http://127.0.0.1:"+std::to_string(package.server->port)}};if(mode=="token")result["token"]=package.server->token();
 return {{"ok",true},{"result",result}};
}
void execute(Package &package,const package_model::Call &call,const json &arguments,loader_json_completion complete,void *context){
 auto finish=[&](const json &value){auto body=value.dump();complete(body.data(),body.size(),context);};
 auto fail=[&](json error){finish({{"ok",false},{"error",std::move(error)}});};
 if(package.state!="active"){fail({{"code","PACKAGE_UNLOADED"},{"package",package.manifest.id}});return;}
 auto merged=arguments;for(auto item=call.arguments.begin();item!=call.arguments.end();++item)merged[item.key()]=item.value();
 if(!call.handler.empty()){
  if(!package.runtime){fail({{"code","RUNTIME_UNAVAILABLE"},{"package",package.manifest.id}});return;}
  package.runtime->invoke(call.handler,merged,[complete,context](const json &response){auto body=response.dump(-1,' ',false,json::error_handler_t::replace);complete(body.data(),body.size(),context);});return;
 }
 if(call.method=="storage.read"||call.method=="storage.write"){if(!package.manifest.capabilities.count("storage"))fail({{"code","PERMISSION_UNDECLARED"},{"package",package.manifest.id},{"method",call.method}});else finish(storage_call(package,call.method,merged));return;}
 std::string key;if(auto code=permission_key(package,call.method,key)){fail({{"code",code},{"package",package.manifest.id},{"method",call.method}});return;}
 if(!package.grants.value("grant."+key,false)){fail({{"code","PERMISSION_DENIED"},{"package",package.manifest.id},{"permission",key}});return;}
 if(call.method=="http.request"){
  package_network::request(merged,[complete,context](const json &response){auto body=std::make_shared<std::string>(response.dump(-1,' ',false,json::error_handler_t::replace));if(!post_main([complete,context,body]{complete(body->data(),body->size(),context);})){static const char lost[]="{\"ok\":false,\"error\":{\"code\":\"DISPATCH_UNAVAILABLE\"}}";complete(lost,sizeof lost-1,context);}});return;
 }
 if(call.method=="server.listen"){finish(listen_call(package,merged));return;}
 if(call.method=="server.close"){package.server.reset();finish({{"ok",true},{"result",json::object()}});return;}
 request(call.method.c_str(),merged.dump(-1,' ',false,json::error_handler_t::replace).c_str(),forward_reply,new Forward{complete,context});
}
void package_dispatch(const char *method,const char *arguments,loader_json_completion complete,void *context,void *owner){
 auto &package=*static_cast<Package*>(owner);auto parsed=json::parse(arguments,nullptr,false);
 for(const auto &tool:package.manifest.tools)if(tool.method==method&&parsed.is_object()){execute(package,tool.call,parsed,complete,context);return;}
 for(const auto &name:package.manifest.methods)if(package.manifest.id+"."+name==method&&parsed.is_object()){package_model::Call call;call.handler=name;execute(package,call,parsed,complete,context);return;}
 auto body=json{{"ok",false},{"error",{{"code","METHOD_UNAVAILABLE"}}}}.dump();complete(body.data(),body.size(),context);
}
std::string package_reason(const Package &package){
 const auto &reason=package.reason;auto detail=reason.substr(reason.find(':')+1);
 if(reason=="loader-version")return "需要加载器版本 "+package.manifest.loader+"，当前为 "+package_model::loader_version;
 if(reason.rfind("missing-capability:",0)==0)return "当前加载器不提供能力 "+detail;
 if(reason.rfind("missing-package:",0)==0)return "缺少依赖的扩展包 "+detail;
 if(reason.rfind("runtime-handler:",0)==0)return "扩展包代码没有提供处理函数 "+detail;
 if(reason=="runtime-load")return "扩展包代码加载失败";
 return "扩展包无法加载（"+reason+"）";
}
json package_state(const Package &package){
 json rows=json::array();std::string status;
 if(package.state=="disabled")status="已停用";
 else if(package.state!="active"){status="无法加载";rows.push_back({{"type","label"},{"text",package_reason(package)}});}
 else{
  unsigned missing=0;for(const auto &key:package_model::grantable(package.manifest,environment))if(!package.grants.value("grant."+key,false))++missing;
  status=missing?std::to_string(missing)+" 项权限未开启":"已启用";
  if(!package.manifest.tools.empty())rows.push_back({{"type","label"},{"text","MCP 工具："+std::to_string(package.manifest.tools.size())+" 个"}});
  for(const auto &item:package.manifest.menu){if(item.group)rows.push_back({{"type","label"},{"text",item.title}});else rows.push_back({{"type","button"},{"id",item.id},{"text",item.title}});}
  if(!package.last.empty())rows.push_back({{"type","label"},{"text",package.last}});
 }
 return {{"status",status},{"sidebar",{{"title",package.manifest.name},{"rows",rows}}}};
}
bool regular(const fs::path &path,bool directory);
bool path_inside(const fs::path &package,const std::string &relative,const wchar_t *extension,fs::path &absolute){
 if(relative.empty()||relative.size()>512||relative.find(':')!=std::string::npos)return false;fs::path path=fs::u8path(relative);if(path.is_absolute()||path.extension()!=extension)return false;
 auto cursor=package;for(const auto &part:path){if(part==L".."||part==L".")return false;cursor/=part;if(!regular(cursor,cursor!=package/path))return false;}
 absolute=(package/path).lexically_normal();auto rel=absolute.lexically_relative(package.lexically_normal());return !rel.empty()&&*rel.begin()!=L"..";
}
// Completions may arrive on a provider's thread; package code only ever runs on the main thread.
void runtime_reply(const char *body,size_t length,void *context){
 auto done=static_cast<package_runtime::Done*>(context);auto response=std::make_shared<json>(body?json::parse(body,body+length,nullptr,false):json());
 auto deliver=new std::function<void()>([done,response]{(*done)(*response);delete done;});
 if(on_main()){(*deliver)();delete deliver;}else if(!PostMessageW(dispatcher,WM_APP+5,0,reinterpret_cast<LPARAM>(deliver))){delete deliver;delete done;}
}
std::string start_runtime(Package &package){
 fs::path file;if(!path_inside(package.folder,package.manifest.main,L".lua",file)||fs::file_size(file)>package_runtime::source_limit)return "runtime-source";
 std::ifstream stream(file,std::ios::binary);std::string source((std::istreambuf_iterator<char>(stream)),std::istreambuf_iterator<char>());if(!stream&&!stream.eof())return "runtime-source";
 auto target=&package;package_runtime::Host host;
 host.request=[target](const std::string &method,const json &arguments,package_runtime::Done done){package_model::Call call;call.method=method;execute(*target,call,arguments,runtime_reply,new package_runtime::Done(std::move(done)));};
 host.log=[target](const std::string &text){log_message(("package "+target->manifest.id+": "+text).c_str());};
 std::string error;package.runtime=package_runtime::Runtime::create(std::move(host),package.manifest.id,package.manifest.version,source,error);
 if(!package.runtime){for(auto &character:error)if(character<32||character>126)character=' ';package.last=error.substr(0,400);log_message(("package "+package.manifest.id+": "+error).c_str());return "runtime-load";}
 for(const auto &name:package.manifest.methods)if(!package.runtime->handles(name))return "runtime-handler:"+name;
 for(const auto &tool:package.manifest.tools)if(!tool.call.handler.empty()&&!package.runtime->handles(tool.call.handler))return "runtime-handler:"+tool.call.handler;
 for(const auto &item:package.manifest.menu)if(!item.group&&!item.call.handler.empty()&&!package.runtime->handles(item.call.handler))return "runtime-handler:"+item.call.handler;
 return "";
}
// Recomputes a declarative package's state and brings its runtime and provider in line.
void settle(Package &package){
 if(package.state!="removed"){
  if(!package.enabled){package.state="disabled";package.reason.clear();}
  else{package.reason=package_model::resolve(package.manifest,environment);package.state=package.reason.empty()?"active":"failed";}
 }
 const auto &id=package.manifest.id;
 if(package.state=="active"&&!package.runtime&&!package.manifest.main.empty()){try{package.reason=start_runtime(package);}catch(const std::exception&){package.reason="runtime-source";}if(!package.reason.empty())package.state="failed";}
 if(package.state=="active"&&!package.provider&&(!package.manifest.tools.empty()||!package.manifest.methods.empty())){
  json methods=json::array(),tools=json::array();for(const auto &name:package.manifest.methods)methods.push_back(id+"."+name);for(const auto &tool:package.manifest.tools){methods.push_back(tool.method);tools.push_back({{"name",tool.name},{"method",tool.method},{"description",tool.description},{"inputSchema",tool.inputSchema}});}
  current_package=id;package.provider=register_provider(id.c_str(),package_dispatch,&package,json{{"methods",methods},{"tools",tools}}.dump().c_str())==LOADER_OK;current_package.clear();
  if(!package.provider){package.state="failed";package.reason="provider-registration";}
 }
 if(package.state!="active"&&package.provider){unregister_provider(id.c_str(),&package);package.provider=false;}
 if(package.state!="active"){package.server.reset();package.runtime.reset();}
 if(package.state=="active")environment.packages[id]=package.manifest.version;else environment.packages.erase(id);
}
struct MenuCall{Package *package;std::string title;};
void menu_reply(const char *body,size_t length,void *context){
 std::unique_ptr<MenuCall> call(static_cast<MenuCall*>(context));auto value=json::parse(body,body+length,nullptr,false);auto &package=*call->package;
 bool ok=value.is_object()&&value.value("ok",false);std::string code=ok||!value.is_object()||!value.contains("error")||!value["error"].is_object()?"":value["error"].value("code",std::string());
 package.last=call->title+(ok?"：完成":"：失败"+(code.empty()?"":"（"+code+"）"));
 if(package.panel)native_panel::update_ui(package.manifest.id.c_str(),package_state(package).dump().c_str(),&package);
}
void package_changed(const char *body,void *owner){
 auto &package=*static_cast<Package*>(owner);auto event=body?json::parse(body,nullptr,false):json();if(!event.is_object())return;auto kind=event.value("kind",std::string());
 if(kind=="action"&&event.contains("action")&&event["action"].is_string()){for(const auto &item:package.manifest.menu)if(!item.group&&item.id==event["action"]){execute(package,item.call,json::object(),menu_reply,new MenuCall{&package,item.title});return;}return;}
 if(kind!="settings"||!event.contains("settings")||!event["settings"].is_object())return;
 package.grants=event["settings"];package.enabled=package.grants.value("enabled",true);settle(package);
 native_panel::update_ui(package.manifest.id.c_str(),package_state(package).dump().c_str(),owner);
}
void package_panel(Package &package){
 json fields=json::array({{{"key","enabled"},{"title","启用扩展包"},{"type","boolean"},{"default",true}}});
 for(const auto &key:package_model::grantable(package.manifest,environment)){
  auto known=package_model::capability(key);std::string title=known?known->title:"调用扩展包 "+key.substr(8)+" 提供的方法",detail=key;
  if(known){detail.clear();for(auto method:known->methods)detail+=(detail.empty()?"":"、")+std::string(method);}
  fields.push_back({{"key","grant."+key},{"title",title},{"detail",detail},{"section","权限"},{"type","boolean"},{"default",false}});
 }
 auto descriptor=package_state(package);descriptor["id"]=package.manifest.id;descriptor["name"]=package.manifest.name;descriptor["version"]=package.manifest.version;descriptor["settings"]=fields;
 package.panel=true;if(native_panel::register_ui(descriptor.dump().c_str(),package_changed,&package)!=LOADER_OK){package.panel=false;log_message("package-panel-unavailable");}
}
void startup_receipt();
const char *load_native(Package &package){
 const auto &definition=package.definition;auto key=loader_platform_key();const auto &libraries=definition["nativeLibraries"];
 if(!libraries.is_object()||!libraries.contains(key)||!libraries[key].is_string())return "native-platform";
 fs::path absolute;if(!path_inside(package.folder,libraries[key].get<std::string>(),L".dll",absolute))return "native-path";
 std::vector<std::wstring> helper_paths;
 if(definition.contains("nativeHelpers")){
  if(!definition["nativeHelpers"].is_object())return "native-helpers";auto helpers=definition["nativeHelpers"].value(key,json::array());if(!helpers.is_array()||helpers.size()>8)return "native-helpers";
  for(const auto &entry:helpers){fs::path helper;std::wstring physical;if(!entry.is_string()||!path_inside(package.folder,entry.get<std::string>(),L".exe",helper)||!windows_network::canonical_executable_path(helper.wstring(),physical))return "native-helpers";helper_paths.push_back(std::move(physical));}
 }
 auto module=LoadLibraryExW(absolute.c_str(),nullptr,LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR|LOAD_LIBRARY_SEARCH_SYSTEM32);if(!module){log_message("package-load-failed");return "native-load";}
 bool helpers_registered=true;for(const auto &helper:helper_paths)if(!windows_child_gate::register_helper(helper)){helpers_registered=false;break;}
 if(!helpers_registered){for(const auto &helper:helper_paths)windows_child_gate::unregister_helper(helper);FreeLibrary(module);log_message("package-helper-registration-failed");return "native-helpers";}
 modules.push_back(module);auto initialize=(loader_plugin_init_v1_fn)GetProcAddress(module,"loader_plugin_init_v1");
 current_package=package.manifest.id;int result=initialize?initialize(&api):LOADER_ERR_INVALID;current_package.clear();package.init_result=result;package.resident=true;log_message(result==0?"package-resident-ready":"package-init-failed");
 if(result!=LOADER_OK)for(const auto &helper:helper_paths)windows_child_gate::unregister_helper(helper);
 if(result==LOADER_ERR_STORAGE_IO||result==LOADER_ERR_POLICY_INVALID){startup_receipt();log_message("startup-policy-or-storage-failed");TerminateProcess(GetCurrentProcess(),ERROR_DLL_INIT_FAILED);ExitProcess(ERROR_DLL_INIT_FAILED);}
 return result==LOADER_OK?nullptr:"native-init";
}
bool activate(Package &package){
 if(package.state!="pending")return package.state=="active";
 if(package.visiting){package.state="failed";package.reason="dependency-cycle";return false;}
 package.visiting=true;for(const auto &dependency:package.manifest.packages){auto found=packages.find(dependency.first);if(found!=packages.end())activate(*found->second);}package.visiting=false;
 if(package.state!="pending")return false;
 if(package.manifest.native){
  package.reason=package_model::resolve(package.manifest,environment);
  if(package.reason.empty())try{if(auto failure=load_native(package))package.reason=failure;}catch(const std::exception&){package.reason="invalid";}
  package.state=package.reason.empty()?"active":"failed";if(package.state=="active")environment.packages[package.manifest.id]=package.manifest.version;
 }else{settle(package);package_panel(package);}
 return package.state=="active";
}
// Native code stays resident until the host exits, so only declarative packages follow the folder at runtime.
void scan_packages(std::map<std::string,std::unique_ptr<Package>> &found,json &rejected){
 auto folder=root/L"plugins";if(!regular(folder,true))return;
 for(auto &item:fs::directory_iterator(folder)){
  try{
   if(found.size()>=32)break;auto directory=item.path();auto file=directory/L"plugin.json";
   if(!regular(directory,true)||!regular(file)||fs::file_size(file)>65536)continue;
   std::ifstream stream(file);auto package=std::make_unique<Package>();package->definition=json::parse(stream,nullptr,false);package->folder=directory;
   auto invalid=package_model::parse(package->definition,package->manifest);
   if(!invalid.empty()){if(!package->manifest.id.empty())rejected.push_back({{"id",package->manifest.id},{"loaded",false},{"reason","manifest-"+invalid}});continue;}
   if(!package->manifest.enabled||found.count(package->manifest.id))continue;
   auto id=package->manifest.id;found.emplace(id,std::move(package));
  }catch(const std::exception&){log_message("package-invalid");}
 }
}
json package_receipt(const Package &package){
 json result{{"id",package.manifest.id},{"kind",package.manifest.native?"native":"declarative"},{"state",package.state},{"loaded",package.manifest.native?package.resident:package.state=="active"}};
 if(package.resident)result["initResult"]=package.init_result;if(!package.reason.empty())result["reason"]=package.reason;return result;
}
void write_package_results(const json &rejected){package_results=rejected;for(auto &entry:packages)package_results.push_back(package_receipt(*entry.second));}
bool flag_enabled(const wchar_t *key);
void load_packages(){
 environment.capabilities={"mcp.tools","panel","storage","runtime.lua","http","http.server"};
 if(flag_enabled(L"OPEN_SV2_CLEAN_PROFILE_SUPPORTED"))environment.capabilities.insert("profile.clean");
 if(managed&&editor_gate_result==LOADER_OK)environment.capabilities.insert({"editor.read","editor.write","transport.read","transport.control"});
 if(native_voice&&native_voice->installed)environment.capabilities.insert({"voicebanks.read","voicebanks.select"});
 if(native_export&&native_export->available)environment.capabilities.insert({"exports.read","exports.write"});
 json rejected=json::array();scan_packages(packages,rejected);
 for(auto &entry:packages)activate(*entry.second);
 write_package_results(rejected);
}
void retire(Package &package){
 package.state="removed";package.reason.clear();settle(package);
 if(package.panel){native_panel::unregister_ui(package.manifest.id.c_str(),&package);package.panel=false;}
}
json reload_packages(){
 std::map<std::string,std::unique_ptr<Package>> found;json rejected=json::array();scan_packages(found,rejected);
 for(auto &entry:packages){
  auto &package=*entry.second;if(package.manifest.native)continue;auto next=found.find(entry.first);
  if(next==found.end()||next->second->manifest.native){if(package.state!="removed")retire(package);continue;}
  if(package.state!="removed"&&next->second->definition==package.definition)continue;
  if(package.state!="removed")retire(package);
  package.manifest=next->second->manifest;package.definition=next->second->definition;package.folder=next->second->folder;package.grants=json::object();package.last.clear();package.enabled=true;package.state="pending";
 }
 for(auto &entry:found)if(!packages.count(entry.first)){if(entry.second->manifest.native)rejected.push_back({{"id",entry.first},{"loaded",false},{"reason","restart-required"}});else packages.emplace(entry.first,std::move(entry.second));}
 for(auto &entry:packages)activate(*entry.second);
 // Packages that depend on a changed one are re-evaluated against the new set.
 for(auto &entry:packages){auto &package=*entry.second;if(package.manifest.native||package.state=="removed")continue;auto before=package.state;settle(package);if(package.panel&&before!=package.state)native_panel::update_ui(entry.first.c_str(),package_state(package).dump().c_str(),&package);}
 write_package_results(rejected);startup_receipt();return capability_snapshot()["packages"];
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
 if(!windows_network::pin_resident_module(reinterpret_cast<const void*>(&network_tick)))return LOADER_ERR_INVALID;
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
  if(startup_network->state(&startup)!=LOADER_OK||!windows_socket_gate::install()||!windows_child_gate::install()||!windows_webview_gate::install(root))return LOADER_ERR_POLICY_INVALID;
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
 api.set_startup_network_guard=guard;api.get_startup_network_policy=policy;api.list_startup_network_guards=guards;api.acquire_profile_exclusive=acquire_profile;api.release_profile_exclusive=release_profile;api.get_profile_lease_state=lease_state;api.retire_host_web_renderers=retire_renderer;api.retire_host_web_contexts=retire_renderer;api.plugin_storage_write_scoped=storage_scoped;api.plugin_storage_read_scoped=storage_read;
 api.get_host_network_policy=network_policy;api.register_host_navigation_handler=windows_host_navigation::add;api.unregister_host_navigation_handler=windows_host_navigation::remove;if(managed)api.capabilities|=LOADER_CAP_NETWORK_POLICY_SWITCH|LOADER_CAP_STARTUP_NETWORK_GUARD|LOADER_CAP_HOST_NAVIGATION;
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
