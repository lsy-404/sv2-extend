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
#include "data_namespace.h"
#include "editor_gate.h"
#include "editor_autostart.h"
using json=nlohmann::json;namespace fs=std::filesystem;
namespace {
DWORD main_thread;HWND dispatcher;loader_api_v1 api{};std::string executable,host_directory;fs::path root;json package_results=json::array();bool managed=false,app_container=false;
std::vector<HMODULE> modules;
std::unique_ptr<editor_lua::Service> editor_service;int editor_gate_result=LOADER_ERR_NOT_SUPPORTED,editor_start_result=LOADER_ERR_NOT_SUPPORTED;
bool editor_online(){return editor_service&&!editor_service->epoch.empty()&&GetTickCount64()-editor_service->heartbeat<=2000;}
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
 providers.emplace(id,std::move(candidate));return LOADER_OK;
}
int unregister_provider(const char *id,void *owner){if(!on_main()||!id)return LOADER_ERR_INVALID;auto found=providers.find(id);if(found==providers.end()||found->second.owner!=owner)return LOADER_ERR_INVALID;providers.erase(found);std::vector<Request*> pending;{std::lock_guard<std::mutex> guard(requests_mutex);for(auto &r:requests)if(r->provider==id&&!r->delivered)pending.push_back(r.get());}for(auto r:pending)error(r,"PROVIDER_REMOVED");return 0;}
void dispatch_request(Request *r){
 if(r->delivered)return;
 if(GetTickCount64()>=r->deadline){error(r,"TIMEOUT");return;}
 if(r->method=="runtime.info"){reply(r,json{{"ok",true},{"result",{{"hostPid",GetCurrentProcessId()},{"platform",loader_platform_key()},{"resident",true},{"editorReady",editor_online()},{"wholeHostNetworkVerified",false},{"namespaceRedirects",data_namespace::redirected.load()},{"namespaceOtherQueries",data_namespace::other.load()}}}}.dump());return;}
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
void editor_receipt(){if(!managed)return;static unsigned attempts=0;static ULONGLONG attempted_at=0;auto now=GetTickCount64();auto state=editor_autostart::status();if(editor_start_result==LOADER_OK&&!state.scan_seen&&native_panel::host&&attempts<20&&now-attempted_at>=500){attempted_at=now;attempts++;editor_autostart::request_initial_scan();state=editor_autostart::status();}json value{{"hostPid",GetCurrentProcessId()},{"getenvGateResult",editor_gate_result},{"autostartResult",editor_start_result},{"installed",state.installed},{"scanSeen",state.scan_seen},{"runRequested",state.run_requested},{"scanAttempts",attempts},{"initialScanReason",state.initial_scan_reason},{"editorReady",editor_online()}};static std::string previous;auto body=value.dump(2);if(body==previous)return;std::ofstream output(root/L"outputs"/L"windows-editor.json",std::ios::binary|std::ios::trunc);if(!output)return;output<<body;output.close();if(output)previous=body;}
LRESULT CALLBACK window_proc(HWND window,UINT message,WPARAM w,LPARAM l){if(message==WM_APP+1){dispatch_request((Request*)l);return 0;}if(message==WM_TIMER){if(editor_service)editor_service->tick();std::vector<Request*> expired;{std::lock_guard<std::mutex> guard(requests_mutex);for(auto &r:requests)if(!r->delivered&&GetTickCount64()>=r->deadline)expired.push_back(r.get());}for(auto r:expired)error(r,"TIMEOUT");namespace_receipt();editor_receipt();return 0;}return DefWindowProcW(window,message,w,l);}
int network_state(loader_network_state_v1 *state){if(!state||state->abi_version!=1||state->struct_size<sizeof *state)return LOADER_ERR_INVALID;*state={1,sizeof *state,1,0,LOADER_NET_WINHTTP|LOADER_NET_WININET|LOADER_NET_WINSOCK|LOADER_NET_WEBVIEW2,0,0,0,0};return 0;}
int verify(uint64_t generation,loader_network_state_v1 *state){int result=network_state(state);if(result)return result;return generation!=state->generation?LOADER_ERR_STALE_GENERATION:LOADER_ERR_NETWORK_UNVERIFIED;}
int block(int){return LOADER_ERR_NOT_SUPPORTED;}int blocked(){return 0;}
int http(const char*,loader_http_completion done,void *context){if(done)done(0,LOADER_ERR_NOT_SUPPORTED,nullptr,0,context);return LOADER_ERR_NOT_SUPPORTED;}
int storage(const char*,const uint8_t*,size_t){return LOADER_ERR_NOT_SUPPORTED;}
int storage_scoped(const char*,const char*,const uint8_t*,size_t){return LOADER_ERR_NOT_SUPPORTED;}
int storage_read(const char*,const char*,uint8_t*,size_t*){return LOADER_ERR_NOT_SUPPORTED;}
int register_editor(loader_editor_dispatch,void*,const char*){return LOADER_ERR_NOT_SUPPORTED;}
int unregister_editor(loader_editor_dispatch,void*){return LOADER_ERR_NOT_SUPPORTED;}
int listen(const char*,loader_http_json_handler,void*,loader_http_endpoint_v1*){return LOADER_ERR_NOT_SUPPORTED;}
int stop(const char*,loader_http_json_handler,void*){return LOADER_ERR_NOT_SUPPORTED;}
const char *host_data(){return managed?host_directory.c_str():nullptr;}
int guard(const char*,const char*,int){return LOADER_ERR_NOT_SUPPORTED;}
int policy(loader_startup_network_state_v1*){return LOADER_ERR_NOT_SUPPORTED;}
void guards(const char*,loader_json_completion done,void *context){const char body[]="{\"ok\":false,\"error\":{\"code\":\"NOT_SUPPORTED\"}}";if(done)done(body,sizeof body-1,context);}
int lease(const char*,const char*){return LOADER_ERR_NOT_SUPPORTED;}
int lease_state(loader_profile_lease_state_v1*){return LOADER_ERR_NOT_SUPPORTED;}
int retire_web(uint64_t,loader_json_completion,void*){return LOADER_ERR_NOT_SUPPORTED;}
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
   auto module=LoadLibraryExW(absolute.c_str(),nullptr,LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR|LOAD_LIBRARY_SEARCH_SYSTEM32);if(!module){package_results.push_back({{"id",id},{"loaded",false}});log_message("package-load-failed");continue;}
   modules.push_back(module);ids.insert(id);count++;auto initialize=(loader_plugin_init_v1_fn)GetProcAddress(module,"loader_plugin_init_v1");int result=initialize?initialize(&api):LOADER_ERR_INVALID;package_results.push_back({{"id",id},{"loaded",true},{"initResult",result}});log_message(result==0?"package-resident-ready":"package-init-failed");
   if(result==LOADER_ERR_STORAGE_IO){startup_receipt();log_message("startup-storage-integrity-failed");TerminateProcess(GetCurrentProcess(),ERROR_DLL_INIT_FAILED);ExitProcess(ERROR_DLL_INIT_FAILED);}
  }catch(const std::exception&){log_message("package-invalid");}
 }
}
bool token_is_app_container(){HANDLE token=nullptr;if(!OpenProcessToken(GetCurrentProcess(),TOKEN_QUERY,&token))return false;DWORD value=0,size=0;bool result=GetTokenInformation(token,TokenIsAppContainer,&value,sizeof(value),&size)&&size==sizeof(value)&&value!=0;CloseHandle(token);return result;}
bool flag_enabled(const wchar_t *key){wchar_t value[4]{};return GetEnvironmentVariableW(key,value,4)==1&&value[0]==L'1';}
bool read_path(const wchar_t *key,fs::path &out){wchar_t path[32768]{};DWORD n=GetEnvironmentVariableW(key,path,32768);if(!n||n>=32768)return false;fs::path parsed(path);if(!parsed.is_absolute()||!parsed.has_root_name())return false;out=parsed.lexically_normal();return out.native().size()<32768;}
void startup_receipt(){std::error_code ec;auto directory=root/L"outputs";fs::create_directories(directory,ec);if(ec||!regular(directory,true)){log_message("runtime-receipt-directory-unavailable");return;}auto file=directory/L"windows-runtime.json";if(fs::exists(file,ec)&&!regular(file)){log_message("runtime-receipt-path-invalid");return;}std::ofstream stream(file,std::ios::binary|std::ios::trunc);if(!stream){log_message("runtime-receipt-write-failed");return;}stream<<json{{"hostPid",GetCurrentProcessId()},{"mainThreadId",main_thread},{"appContainer",app_container},{"managed",managed},{"hostDataNamespace",managed},{"pluginUi",true},{"packages",package_results}}.dump(2);stream.close();if(!stream)log_message("runtime-receipt-write-failed");}
}
extern "C" __declspec(dllexport) int ExtendInitialize(void*){
 if(dispatcher)return main_thread==GetCurrentThreadId()?0:LOADER_ERR_INVALID;
 app_container=token_is_app_container();managed=flag_enabled(L"OPEN_SV2_WINDOWS_MANAGED");if(managed&&!app_container)return LOADER_ERR_NOT_SUPPORTED;if(!managed&&!flag_enabled(L"OPEN_SV2_WINDOWS_FIXTURE"))return LOADER_ERR_NOT_SUPPORTED;
 main_thread=GetCurrentThreadId();if(!read_path(L"OPEN_SV2_LOADER_DATA_DIR",root))return LOADER_ERR_STORAGE_NOT_CONFIGURED;if(!regular(root,true))return LOADER_ERR_INVALID;
 if(managed){fs::path actual;if(!read_path(L"OPEN_SV2_HOST_DATA_DIR",actual)||CompareStringOrdinal(actual.c_str(),-1,(root/L"host-data").c_str(),-1,TRUE)!=CSTR_EQUAL||!regular(actual,true)||actual.native().size()+10>=MAX_PATH||!data_namespace::install(actual.wstring()))return LOADER_ERR_INVALID;host_directory=actual.u8string();}
 WNDCLASSW cls{};cls.lpfnWndProc=window_proc;cls.hInstance=GetModuleHandleW(nullptr);cls.lpszClassName=L"SV2Extend.Resident.Dispatch.v1";
 if(!RegisterClassW(&cls))return LOADER_ERR_INVALID;
 dispatcher=CreateWindowExW(0,cls.lpszClassName,L"",0,0,0,0,0,HWND_MESSAGE,nullptr,cls.hInstance,nullptr);if(!dispatcher||!SetTimer(dispatcher,1,100,nullptr)||native_panel::install(dispatcher,root.c_str())!=LOADER_OK)return LOADER_ERR_INVALID;
 wchar_t host[32768];DWORD n=GetModuleFileNameW(nullptr,host,32768);if(!n||n>=32768)return LOADER_ERR_INVALID;executable=fs::path(host).u8string();
 api.abi_version=1;api.struct_size=sizeof api;api.capabilities=LOADER_CAP_NETWORK_STATE|LOADER_CAP_PLUGIN_UI|(managed?LOADER_CAP_HOST_DATA_NAMESPACE:0);api.host_pid=GetCurrentProcessId();api.on_main_thread=1;api.host_executable=executable.c_str();api.log=log_message;
 api.set_host_network_blocked=block;api.host_network_blocked=blocked;api.plugin_http_get=http;api.get_host_network_state=network_state;api.verify_host_network_quiescence=verify;api.plugin_storage_write=storage;
 api.register_editor_adapter=register_editor;api.unregister_editor_adapter=unregister_editor;api.editor_request_json=request;api.request_capability_json=request;api.register_capability_provider=register_provider;api.unregister_capability_provider=unregister_provider;
 api.listen_local_json=listen;api.stop_local_json=stop;api.host_data_directory=host_data;api.register_plugin_ui=native_panel::register_ui;api.unregister_plugin_ui=native_panel::unregister_ui;api.update_plugin_ui=native_panel::update_ui;
 api.set_startup_network_guard=guard;api.get_startup_network_policy=policy;api.list_startup_network_guards=guards;api.acquire_profile_exclusive=lease;api.release_profile_exclusive=lease;api.get_profile_lease_state=lease_state;api.retire_host_web_renderers=retire_web;api.retire_host_web_contexts=retire_web;api.plugin_storage_write_scoped=storage_scoped;api.plugin_storage_read_scoped=storage_read;
 if(managed){editor_service=std::make_unique<editor_lua::Service>(&api);editor_gate_result=editor_gate::install(editor_service.get());if(editor_gate_result==LOADER_OK){auto entry=root/L"host-data"/L"Roaming"/L"Dreamtonics"/L"Synthesizer V Studio 2"/L"scripts"/L"opensv2-loader-runtime.lua";editor_start_result=editor_autostart::install(editor_service.get(),entry.c_str());}}
 load_packages();startup_receipt();log_message("windows-resident-ready");return 0;
}
extern "C" __declspec(dllexport) const loader_api_v1 *ExtendGetAPI(){return dispatcher?&api:nullptr;}
BOOL WINAPI DllMain(HINSTANCE,DWORD,LPVOID){return TRUE;}
