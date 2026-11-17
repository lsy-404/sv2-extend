#pragma once
#include <windows.h>
#include <shellapi.h>
#include <mutex>
#include <set>
#include <string>
#include "network_path.h"
#include "vendor/minhook/include/MinHook.h"
namespace windows_child_gate {
struct State {bool installed=false,blocked=true;size_t helpers=0;unsigned long long denied=0,generation=0;};
inline std::recursive_mutex mutex;
inline State current;
struct insensitive {bool operator()(const std::wstring &a,const std::wstring &b)const{return CompareStringOrdinal(a.c_str(),-1,b.c_str(),-1,TRUE)==CSTR_LESS_THAN;}};
inline std::set<std::wstring,insensitive> helpers;
using Create=BOOL(WINAPI*)(LPCWSTR,LPWSTR,LPSECURITY_ATTRIBUTES,LPSECURITY_ATTRIBUTES,BOOL,DWORD,LPVOID,LPCWSTR,LPSTARTUPINFOW,LPPROCESS_INFORMATION);
using Shell=HINSTANCE(WINAPI*)(HWND,LPCWSTR,LPCWSTR,LPCWSTR,LPCWSTR,INT);
inline Create original_create=nullptr;inline Shell original_shell=nullptr;
inline bool physical(const wchar_t *path,std::wstring &resolved){
 if(!path||!*path)return false;std::wstring value(path);wchar_t full[32768];DWORD n=GetFullPathNameW(path,32768,full,nullptr);
 if(!n||n>=32768||CompareStringOrdinal(path,-1,full,-1,TRUE)!=CSTR_EQUAL)return false;
 if(!windows_network::canonical_executable_path(value,resolved))return false;
 return CompareStringOrdinal(path,-1,resolved.c_str(),-1,TRUE)==CSTR_EQUAL;
}
inline BOOL WINAPI on_create(LPCWSTR application,LPWSTR command,LPSECURITY_ATTRIBUTES processSecurity,LPSECURITY_ATTRIBUTES threadSecurity,BOOL inherit,DWORD flags,LPVOID environment,LPCWSTR directory,LPSTARTUPINFOW startup,LPPROCESS_INFORMATION process){
 std::lock_guard<std::recursive_mutex> lock(mutex);
 if(!current.blocked){BOOL result=original_create(application,command,processSecurity,threadSecurity,inherit,flags,environment,directory,startup,process);if(result)++current.generation;return result;}
 std::wstring resolved;if(!physical(application,resolved)||helpers.find(resolved)==helpers.end()){++current.denied;SetLastError(ERROR_ACCESS_DENIED);return FALSE;}
 BOOL result=original_create(resolved.c_str(),command,processSecurity,threadSecurity,inherit,flags,environment,directory,startup,process);if(result)++current.generation;return result;
}
inline HINSTANCE WINAPI on_shell(HWND window,LPCWSTR operation,LPCWSTR file,LPCWSTR parameters,LPCWSTR directory,INT show){
 std::lock_guard<std::recursive_mutex> lock(mutex);
 if(!current.blocked)return original_shell(window,operation,file,parameters,directory,show);
 ++current.denied;SetLastError(ERROR_ACCESS_DENIED);return reinterpret_cast<HINSTANCE>(static_cast<INT_PTR>(SE_ERR_ACCESSDENIED));
}
inline bool register_helper(const std::wstring &path){std::wstring resolved;if(!physical(path.c_str(),resolved))return false;std::lock_guard<std::recursive_mutex> lock(mutex);try{if(helpers.insert(resolved).second)++current.generation;}catch(...){return false;}return true;}
inline void setblocked(bool value){std::lock_guard<std::recursive_mutex> lock(mutex);if(current.blocked!=value){current.blocked=value;++current.generation;}}
inline State state(){std::lock_guard<std::recursive_mutex> lock(mutex);State out=current;out.helpers=helpers.size();return out;}
inline bool install(){
 std::lock_guard<std::recursive_mutex> lock(mutex);if(current.installed)return true;
 HMODULE kernel=GetModuleHandleW(L"kernel32.dll"),shell=GetModuleHandleW(L"shell32.dll");if(!shell)shell=LoadLibraryExW(L"shell32.dll",nullptr,LOAD_LIBRARY_SEARCH_SYSTEM32);if(!kernel||!shell)return false;
 void *create=reinterpret_cast<void*>(GetProcAddress(kernel,"CreateProcessW")),*execute=reinterpret_cast<void*>(GetProcAddress(shell,"ShellExecuteW"));if(!create||!execute)return false;
 if(MH_CreateHook(create,reinterpret_cast<void*>(on_create),reinterpret_cast<void**>(&original_create))!=MH_OK)return false;
 if(MH_CreateHook(execute,reinterpret_cast<void*>(on_shell),reinterpret_cast<void**>(&original_shell))!=MH_OK){MH_RemoveHook(create);return false;}
 if(MH_EnableHook(create)!=MH_OK){MH_RemoveHook(execute);MH_RemoveHook(create);return false;}
 if(MH_EnableHook(execute)!=MH_OK){MH_DisableHook(create);MH_RemoveHook(execute);MH_RemoveHook(create);return false;}
 current.installed=true;return true;
}
}
