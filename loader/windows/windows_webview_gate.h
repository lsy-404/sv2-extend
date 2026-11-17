#pragma once
#include "windows_webview_transport.h"
#include "windows_child_gate.h"
#include <atomic>
#include <cstring>
#include <cstdint>
#include <mutex>
#include <vector>
#include <algorithm>
#include <new>
#include <deque>
#include <memory>
#include "vendor/minhook/include/MinHook.h"
namespace windows_webview_gate {
using Factory=HRESULT(WINAPI*)(bool,int,PCWSTR,IUnknown*,IUnknown*);
using Resolve=FARPROC(WINAPI*)(HMODULE,LPCSTR);
struct State {bool installed=false,blocked=true,factoryBound=false,onlineFactorySeen=false;uint64_t denied=0;unsigned pendingEnvironment=0,pendingController=0,controllers=0;bool coverage=false;unsigned deferredController=0;};
inline Resolve original_resolve=nullptr;
inline std::atomic<Factory> original_factory{nullptr};
inline std::atomic<HMODULE> retained{nullptr};
inline std::atomic<bool> blocked{true},online_seen{false};
inline std::atomic<uint64_t> denied{0};inline bool installed=false;
struct Completion: IUnknown {virtual HRESULT STDMETHODCALLTYPE Invoke(HRESULT,IUnknown*)=0;};
inline constexpr GUID env_handler_iid={0x4e8a3389,0xc9d8,0x4bd2,{0xb6,0xb5,0x12,0x4f,0xee,0x6c,0xc1,0x4d}};
inline constexpr GUID controller_handler_iid={0x6c4819f3,0xc9b7,0x4260,{0x81,0x27,0xc9,0xf5,0xbd,0xe7,0xf6,0x8c}};
using ControllerCreate=HRESULT(WINAPI*)(IUnknown*,HWND,Completion*);
using ControllerClose=HRESULT(WINAPI*)(IUnknown*);
inline std::atomic<unsigned> pending_environment{0},pending_controller{0};
inline std::atomic<bool> coverage{true};inline DWORD owner=0;inline std::filesystem::path managed_root;inline std::wstring browser_image,configured_arguments;
inline std::mutex lifecycle_mutex;inline std::vector<IUnknown*> controllers;inline void *controller_target=nullptr,*close_target=nullptr;inline ControllerCreate original_controller_create=nullptr;inline ControllerClose original_controller_close=nullptr;
inline HRESULT WINAPI close_controller(IUnknown *controller){
 HRESULT result=original_controller_close(controller);if(SUCCEEDED(result)){
  IUnknown *release=nullptr;{std::lock_guard<std::mutex> lock(lifecycle_mutex);auto it=std::find(controllers.begin(),controllers.end(),controller);if(it!=controllers.end()){release=*it;controllers.erase(it);}}
  if(release)release->Release();
 }return result;
}
inline bool watch_controller(IUnknown *controller){
 auto table=*reinterpret_cast<void***>(controller);if(!table||!table[24])return false;
 std::lock_guard<std::mutex> lock(lifecycle_mutex);if(close_target)return close_target==table[24];
 void *original=nullptr;if(MH_CreateHook(table[24],reinterpret_cast<void*>(close_controller),&original)!=MH_OK)return false;
 original_controller_close=reinterpret_cast<ControllerClose>(original);
 if(MH_EnableHook(table[24])!=MH_OK){MH_RemoveHook(table[24]);original_controller_close=nullptr;return false;}close_target=table[24];return true;
}
inline HRESULT WINAPI create_controller(IUnknown *environment,HWND parent,Completion *handler);
inline bool watch_environment(IUnknown *environment){
 if(!environment)return false;auto table=*reinterpret_cast<void***>(environment);if(!table||!table[3])return false;
 std::lock_guard<std::mutex> lock(lifecycle_mutex);if(controller_target)return controller_target==table[3];
 void *original=nullptr;if(MH_CreateHook(table[3],reinterpret_cast<void*>(create_controller),&original)!=MH_OK)return false;
 original_controller_create=reinterpret_cast<ControllerCreate>(original);
 if(MH_EnableHook(table[3])!=MH_OK){MH_RemoveHook(table[3]);original_controller_create=nullptr;return false;}
 controller_target=table[3];return true;
}
struct Proxy final: Completion {
 std::atomic<ULONG> refs{1};Completion *next;bool environment;std::atomic<bool> completed{false};std::shared_ptr<std::atomic<bool>> claim;
 Proxy(Completion *target,bool env,std::shared_ptr<std::atomic<bool>> c={}):next(target),environment(env),claim(std::move(c)){next->AddRef();if(env)++pending_environment;else ++pending_controller;}
 ~Proxy(){if(!completed.exchange(true)){if(environment)--pending_environment;else --pending_controller;}next->Release();}
 HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid,void **result)override{if(!result)return E_POINTER;*result=nullptr;if(IsEqualIID(iid,IID_IUnknown)||IsEqualIID(iid,environment?env_handler_iid:controller_handler_iid)){*result=static_cast<Completion*>(this);AddRef();return S_OK;}return E_NOINTERFACE;}
 ULONG STDMETHODCALLTYPE AddRef()override{return ++refs;}
 ULONG STDMETHODCALLTYPE Release()override{auto value=--refs;if(!value)delete this;return value;}
 HRESULT STDMETHODCALLTYPE Invoke(HRESULT result,IUnknown *object)override{
  if(completed.exchange(true))return E_UNEXPECTED;
  if(claim&&claim->exchange(true)){if(environment)--pending_environment;else --pending_controller;return E_UNEXPECTED;}
  if(environment){
   if(SUCCEEDED(result)&&object&&(GetCurrentThreadId()!=owner||!watch_environment(object))){coverage.store(false);if(blocked.load()){result=HRESULT_FROM_WIN32(ERROR_ACCESS_DENIED);object=nullptr;}}
  }else if(SUCCEEDED(result)&&object){
   if(GetCurrentThreadId()!=owner){coverage.store(false);}
   else{
    if(!watch_controller(object))coverage.store(false);
    std::lock_guard<std::mutex> lock(lifecycle_mutex);try{object->AddRef();controllers.push_back(object);}catch(...){object->Release();coverage.store(false);}
   }
  }
  HRESULT forwarded=next->Invoke(result,object);if(environment)--pending_environment;else --pending_controller;return forwarded;
 }
};
inline HRESULT controller_impl(IUnknown *environment,HWND parent,Completion *handler,std::shared_ptr<std::atomic<bool>> claim={}){
 if(GetCurrentThreadId()!=owner||!handler){coverage.store(false);return original_controller_create(environment,parent,handler);}
 Proxy *proxy=new(std::nothrow) Proxy(handler,false,std::move(claim));if(!proxy){coverage.store(false);return E_OUTOFMEMORY;}
 HRESULT result=original_controller_create(environment,parent,proxy);proxy->Release();return result;
}
inline HRESULT WINAPI create_controller(IUnknown *environment,HWND parent,Completion *handler){return controller_impl(environment,parent,handler);}
inline bool conflicting_arguments(const std::wstring &arguments){
 std::wstring text=arguments;std::transform(text.begin(),text.end(),text.begin(),[](wchar_t c){return towlower(c);});
 return text.find(L"--proxy")!=std::wstring::npos||text.find(L"--no-proxy-server")!=std::wstring::npos||text.find(L"--edge-webview-switches")!=std::wstring::npos;
}
inline bool options_supported(IUnknown *options){
 if(!options)return true;const GUID iid={0x2fde08a8,0x1e9a,0x4766,{0x8c,0x05,0x95,0xa9,0xce,0xb9,0xd1,0xc5}};IUnknown *public_options=nullptr;
 if(FAILED(options->QueryInterface(iid,reinterpret_cast<void**>(&public_options)))||!public_options)return false;
 LPWSTR arguments=nullptr;auto table=*reinterpret_cast<void***>(public_options);HRESULT result=reinterpret_cast<HRESULT(WINAPI*)(IUnknown*,LPWSTR*)>(table[3])(public_options,&arguments);
 bool supported=SUCCEEDED(result)&&(!arguments||!conflicting_arguments(arguments));CoTaskMemFree(arguments);public_options->Release();return supported;
}
inline HRESULT WINAPI factory(bool check,int type,PCWSTR folder,IUnknown *options,IUnknown *handler){
 if(!handler)return E_INVALIDARG;
 if(!blocked.load())online_seen.store(true);auto original=original_factory.load();if(!original)return E_UNEXPECTED;
 if(!options_supported(options)){coverage.store(false);++denied;return E_INVALIDARG;}
 try{
  std::filesystem::path udf;
  if(folder&&*folder)udf=std::filesystem::absolute(folder).lexically_normal();
  else{wchar_t path[32768];DWORD n=GetModuleFileNameW(nullptr,path,32768);if(!n||n>=32768)return E_FAIL;udf=std::wstring(path)+L".WebView2";}
  auto relative=udf.lexically_relative(managed_root);
  if(managed_root.empty()||relative.empty()||relative.is_absolute()||*relative.begin()==L".."||!windows_webview_transport::prepare_profile(udf.c_str())){coverage.store(false);++denied;return HRESULT_FROM_WIN32(ERROR_ACCESS_DENIED);}
 }catch(...){coverage.store(false);++denied;return E_FAIL;}
 if(GetCurrentThreadId()!=owner||!handler){coverage.store(false);return original(check,type,folder,options,handler);}
 Proxy *proxy=new(std::nothrow) Proxy(reinterpret_cast<Completion*>(handler),true);if(!proxy){coverage.store(false);return E_OUTOFMEMORY;}
 HRESULT result=original(check,type,folder,options,proxy);proxy->Release();return result;
}
inline bool runtime_module(HMODULE module){wchar_t path[32768];DWORD n=GetModuleFileNameW(module,path,32768);if(!n||n>=32768)return false;const wchar_t *name=path;for(DWORD i=0;i<n;i++)if(path[i]==L'\\'||path[i]==L'/')name=path+i+1;return _wcsicmp(name,L"EmbeddedBrowserWebView.dll")==0;}
inline FARPROC WINAPI resolve(HMODULE module,LPCSTR name){
 FARPROC found=original_resolve(module,name);if(!found||reinterpret_cast<uintptr_t>(name)<=0xffff||strcmp(name,"CreateWebViewEnvironmentWithOptionsInternal"))return found;
 MEMORY_BASIC_INFORMATION info{};if(!VirtualQuery(__builtin_return_address(0),&info,sizeof info)||info.AllocationBase!=GetModuleHandleW(nullptr)||!runtime_module(module))return found;
 wchar_t path[32768];DWORD length=GetModuleFileNameW(module,path,32768);if(!length||length>=32768)return nullptr;
 auto browser=std::filesystem::path(path).parent_path().parent_path().parent_path()/L"msedgewebview2.exe";
 if(!windows_child_gate::register_helper(browser.wstring())){coverage.store(false);return nullptr;}browser_image=browser.wstring();
 HMODULE hold=nullptr;if(!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS,reinterpret_cast<LPCWSTR>(found),&hold))return nullptr;
 HMODULE expected=nullptr;if(retained.compare_exchange_strong(expected,hold)){original_factory.store(reinterpret_cast<Factory>(found));}
 else{FreeLibrary(hold);if(expected!=module||original_factory.load()!=reinterpret_cast<Factory>(found))return nullptr;}
 return reinterpret_cast<FARPROC>(factory);
}
inline bool install(const std::filesystem::path &profile){
 if(installed)return true;if(!windows_webview_transport::configuration_supported())return false;managed_root=std::filesystem::absolute(profile).lexically_normal();owner=GetCurrentThreadId();if(!windows_webview_transport::install())return false;
 wchar_t override_path[32768];if(GetEnvironmentVariableW(L"WEBVIEW2_USER_DATA_FOLDER",override_path,32768))return false;
 wchar_t previous[32768];DWORD count=GetEnvironmentVariableW(L"WEBVIEW2_ADDITIONAL_BROWSER_ARGUMENTS",previous,32768);if(count>=32768)return false;
 std::wstring arguments=count?std::wstring(previous,count):std::wstring();
 if(conflicting_arguments(arguments))return false;
 arguments+=L" "+windows_webview_transport::arguments();if(!SetEnvironmentVariableW(L"WEBVIEW2_ADDITIONAL_BROWSER_ARGUMENTS",arguments.c_str()))return false;
 configured_arguments=arguments;
 HMODULE kernel=GetModuleHandleW(L"kernel32.dll");if(!kernel)return false;
 auto address=reinterpret_cast<void*>(GetProcAddress(kernel,"GetProcAddress"));if(!address)return false;
 if(MH_CreateHook(address,reinterpret_cast<void*>(resolve),reinterpret_cast<void**>(&original_resolve))!=MH_OK)return false;
 if(MH_EnableHook(address)!=MH_OK){MH_RemoveHook(address);original_resolve=nullptr;return false;}installed=true;return true;
}
inline void setblocked(bool value){
 blocked.store(value);windows_webview_transport::setblocked(value);
}
inline bool retire(){
 if(!installed||!blocked.load()||GetCurrentThreadId()!=owner)return false;
 std::vector<IUnknown*> old;{std::lock_guard<std::mutex> lock(lifecycle_mutex);old.swap(controllers);}
 for(auto *controller:old){auto table=*reinterpret_cast<void***>(controller);if(!table||!table[24]||FAILED(reinterpret_cast<ControllerClose>(table[24])(controller)))coverage.store(false);controller->Release();}
 return coverage.load()&&pending_environment.load()==0&&pending_controller.load()==0;
}
inline State state(){std::lock_guard<std::mutex> lock(lifecycle_mutex);return {installed,blocked.load(),original_factory.load()!=nullptr,online_seen.load(),denied.load(),pending_environment.load(),pending_controller.load(),static_cast<unsigned>(controllers.size()),coverage.load(),0};}
inline bool covered(const State &value,bool earlyProof){
 auto transport=windows_webview_transport::state();
 wchar_t arguments[32768];DWORD size=GetEnvironmentVariableW(L"WEBVIEW2_ADDITIONAL_BROWSER_ARGUMENTS",arguments,32768);if(!size||size>=32768||configured_arguments!=std::wstring(arguments,size))return false;
 if(!value.installed||!value.blocked||!value.coverage||value.pendingEnvironment||value.pendingController||!transport.installed||!transport.blocked||transport.pending||transport.active||!transport.coverage)return false;
 return value.factoryBound||(earlyProof&&!value.onlineFactorySeen&&!value.denied);
}
inline bool process_covered(DWORD pid){
 if(pid==GetCurrentProcessId())return true;if(browser_image.empty())return false;
 HANDLE process=OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION,FALSE,pid);if(!process)return false;
 wchar_t image[32768];DWORD length=32768;bool ok=QueryFullProcessImageNameW(process,0,image,&length)&&CompareStringOrdinal(image,-1,browser_image.c_str(),-1,TRUE)==CSTR_EQUAL;CloseHandle(process);return ok;
}
}
