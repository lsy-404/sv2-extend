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
using Navigation=int(*)(LPCWSTR);
inline Navigation navigation=nullptr;
inline bool navigation_url(LPCWSTR operation,LPCWSTR file,LPCWSTR parameters){
 if(!file||!*file||(operation&&CompareStringOrdinal(operation,-1,L"open",-1,TRUE)!=CSTR_EQUAL)||(parameters&&*parameters))return false;
 auto alpha=[](wchar_t c){return (c>=L'a'&&c<=L'z')||(c>=L'A'&&c<=L'Z');};
 if(!alpha(file[0]))return false;size_t i=1;for(;file[i]&&i<32768;++i){wchar_t c=file[i];if(c==L':')return i>1;if(!alpha(c)&&!(c>=L'0'&&c<=L'9')&&c!=L'+'&&c!=L'-'&&c!=L'.')return false;}return false;
}
inline bool physical(const wchar_t *path,std::wstring &resolved){
 if(!path||!*path)return false;std::wstring value(path);wchar_t full[32768];DWORD n=GetFullPathNameW(path,32768,full,nullptr);
 if(!n||n>=32768||CompareStringOrdinal(path,-1,full,-1,TRUE)!=CSTR_EQUAL)return false;
 if(!windows_network::canonical_executable_path(value,resolved))return false;
 return CompareStringOrdinal(path,-1,resolved.c_str(),-1,TRUE)==CSTR_EQUAL;
}
inline BOOL WINAPI on_create(LPCWSTR application,LPWSTR command,LPSECURITY_ATTRIBUTES processSecurity,LPSECURITY_ATTRIBUTES threadSecurity,BOOL inherit,DWORD flags,LPVOID environment,LPCWSTR directory,LPSTARTUPINFOW startup,LPPROCESS_INFORMATION process){
 std::lock_guard<std::recursive_mutex> lock(mutex);
 if(!current.blocked){BOOL result=original_create(application,command,processSecurity,threadSecurity,inherit,flags,environment,directory,startup,process);if(result)++current.generation;return result;}
 std::wstring resolved;bool valid=false;
 if(application&&*application)valid=physical(application,resolved);
 else if(command&&*command){int argc=0;LPWSTR *argv=CommandLineToArgvW(command,&argc);if(argv){if(argc>0)valid=physical(argv[0],resolved);LocalFree(argv);}}
 if(!valid||helpers.find(resolved)==helpers.end()){++current.denied;SetLastError(ERROR_ACCESS_DENIED);return FALSE;}
 BOOL result=original_create(resolved.c_str(),command,processSecurity,threadSecurity,inherit,flags,environment,directory,startup,process);if(result)++current.generation;return result;
}
inline HINSTANCE WINAPI on_shell(HWND window,LPCWSTR operation,LPCWSTR file,LPCWSTR parameters,LPCWSTR directory,INT show){
 auto route=navigation;if(route&&navigation_url(operation,file,parameters)){int decision=route(file);if(decision==1)return reinterpret_cast<HINSTANCE>(static_cast<INT_PTR>(33));if(decision<0){std::lock_guard<std::recursive_mutex> lock(mutex);++current.denied;SetLastError(ERROR_ACCESS_DENIED);return reinterpret_cast<HINSTANCE>(static_cast<INT_PTR>(SE_ERR_ACCESSDENIED));}}
 std::lock_guard<std::recursive_mutex> lock(mutex);
 if(!current.blocked)return original_shell(window,operation,file,parameters,directory,show);
 ++current.denied;SetLastError(ERROR_ACCESS_DENIED);return reinterpret_cast<HINSTANCE>(static_cast<INT_PTR>(SE_ERR_ACCESSDENIED));
}
inline bool register_helper(const std::wstring &path){std::wstring resolved;if(!physical(path.c_str(),resolved))return false;std::lock_guard<std::recursive_mutex> lock(mutex);try{if(helpers.insert(resolved).second)++current.generation;}catch(...){return false;}return true;}
inline void unregister_helper(const std::wstring &path){std::lock_guard<std::recursive_mutex> lock(mutex);if(helpers.erase(path))++current.generation;}
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
