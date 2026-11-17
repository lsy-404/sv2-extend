#pragma once
#include <windows.h>
#include <atomic>
#include <cstring>
#include <cstdint>
#include "vendor/minhook/include/MinHook.h"
namespace windows_webview_gate {
using Factory=HRESULT(WINAPI*)(bool,int,PCWSTR,IUnknown*,IUnknown*);
using Resolve=FARPROC(WINAPI*)(HMODULE,LPCSTR);
struct State {bool installed=false,blocked=true,onlineFactorySeen=false;uint64_t denied=0;};
inline Resolve original_resolve=nullptr;
inline std::atomic<Factory> original_factory{nullptr};
inline std::atomic<HMODULE> retained{nullptr};
inline std::atomic<bool> blocked{true},online_seen{false};
inline std::atomic<uint64_t> denied{0};inline bool installed=false;
inline HRESULT WINAPI factory(bool check,int type,PCWSTR folder,IUnknown *options,IUnknown *handler){
 if(blocked.load()){++denied;return HRESULT_FROM_WIN32(ERROR_ACCESS_DENIED);}
 online_seen.store(true);auto original=original_factory.load();return original?original(check,type,folder,options,handler):E_UNEXPECTED;
}
inline bool runtime_module(HMODULE module){wchar_t path[32768];DWORD n=GetModuleFileNameW(module,path,32768);if(!n||n>=32768)return false;const wchar_t *name=path;for(DWORD i=0;i<n;i++)if(path[i]==L'\\'||path[i]==L'/')name=path+i+1;return _wcsicmp(name,L"EmbeddedBrowserWebView.dll")==0;}
inline FARPROC WINAPI resolve(HMODULE module,LPCSTR name){
 FARPROC found=original_resolve(module,name);if(!found||reinterpret_cast<uintptr_t>(name)<=0xffff||strcmp(name,"CreateWebViewEnvironmentWithOptionsInternal"))return found;
 MEMORY_BASIC_INFORMATION info{};if(!VirtualQuery(__builtin_return_address(0),&info,sizeof info)||info.AllocationBase!=GetModuleHandleW(nullptr)||!runtime_module(module))return found;
 HMODULE hold=nullptr;if(!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS,reinterpret_cast<LPCWSTR>(found),&hold))return nullptr;
 HMODULE expected=nullptr;if(retained.compare_exchange_strong(expected,hold)){original_factory.store(reinterpret_cast<Factory>(found));}
 else{FreeLibrary(hold);if(expected!=module||original_factory.load()!=reinterpret_cast<Factory>(found))return nullptr;}
 return reinterpret_cast<FARPROC>(factory);
}
inline bool install(){
 if(installed)return true;HMODULE kernel=GetModuleHandleW(L"kernel32.dll");if(!kernel)return false;
 auto address=reinterpret_cast<void*>(GetProcAddress(kernel,"GetProcAddress"));if(!address)return false;
 if(MH_CreateHook(address,reinterpret_cast<void*>(resolve),reinterpret_cast<void**>(&original_resolve))!=MH_OK)return false;
 if(MH_EnableHook(address)!=MH_OK){MH_RemoveHook(address);original_resolve=nullptr;return false;}installed=true;return true;
}
inline void setblocked(bool value){blocked.store(value);}
inline State state(){return {installed,blocked.load(),online_seen.load(),denied.load()};}
}
