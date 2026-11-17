#pragma once
#include <windows.h>
#include <string>
#include "vendor/minhook/include/MinHook.h"
namespace long_path_adapter {
inline std::wstring profile;
using Create=HANDLE(WINAPI*)(LPCWSTR,DWORD,DWORD,LPSECURITY_ATTRIBUTES,DWORD,DWORD,HANDLE);
using Attributes=DWORD(WINAPI*)(LPCWSTR);
using AttributesEx=BOOL(WINAPI*)(LPCWSTR,GET_FILEEX_INFO_LEVELS,LPVOID);
inline Create original_create=nullptr;inline Attributes original_attributes=nullptr;inline AttributesEx original_attributes_ex=nullptr;
inline bool reserved(LPCWSTR name,size_t length){
 size_t base=0;while(base<length&&name[base]!=L'.')++base;
 if(base==3&&(!_wcsnicmp(name,L"CON",3)||!_wcsnicmp(name,L"PRN",3)||!_wcsnicmp(name,L"AUX",3)||!_wcsnicmp(name,L"NUL",3)))return true;
 return base==4&&(!_wcsnicmp(name,L"COM",3)||!_wcsnicmp(name,L"LPT",3))&&name[3]>=L'1'&&name[3]<=L'9';
}
inline bool convert(LPCWSTR input,std::wstring &out){
 if(!input)return false;size_t n=wcsnlen(input,32768);if(n<MAX_PATH||n>=32768)return false;
 if(n>=4&&input[0]==L'\\'&&input[1]==L'\\'&&(input[2]==L'?'||input[2]==L'.')&&input[3]==L'\\')return false;
 bool unc=n>=3&&input[0]==L'\\'&&input[1]==L'\\'&&input[2]!=L'\\';bool drive=n>=3&&((input[0]>=L'A'&&input[0]<=L'Z')||(input[0]>=L'a'&&input[0]<=L'z'))&&input[1]==L':'&&input[2]==L'\\';if(!unc&&!drive)return false;
 wchar_t full[32768];DWORD length=GetFullPathNameW(input,32768,full,nullptr);if(!length||length>=32768||length!=n||CompareStringOrdinal(input,-1,full,-1,TRUE)!=CSTR_EQUAL)return false;
 if(profile.empty()||n<=profile.size()||CompareStringOrdinal(input,static_cast<int>(profile.size()),profile.c_str(),static_cast<int>(profile.size()),TRUE)!=CSTR_EQUAL||input[profile.size()]!=L'\\')return false;
 for(size_t i=0;i<n;i++){if(input[i]==L'/')return false;if(input[i]==L'\\'&&i&& (input[i-1]==L'.'||input[i-1]==L' '))return false;}if(input[n-1]==L'.'||input[n-1]==L' ')return false;
 size_t start=profile.size()+1;for(size_t i=start;i<=n;i++)if(i==n||input[i]==L'\\'){if(reserved(input+start,i-start))return false;start=i+1;}
 if(unc)out=L"\\\\?\\UNC\\"+std::wstring(input+2,n-2);else out=L"\\\\?\\"+std::wstring(input,n);return true;
}
inline HANDLE WINAPI create(LPCWSTR name,DWORD access,DWORD share,LPSECURITY_ATTRIBUTES security,DWORD disposition,DWORD flags,HANDLE templateFile){try{std::wstring path;if(convert(name,path))return original_create(path.c_str(),access,share,security,disposition,flags,templateFile);}catch(...){}return original_create(name,access,share,security,disposition,flags,templateFile);}
inline DWORD WINAPI attributes(LPCWSTR name){try{std::wstring path;if(convert(name,path))return original_attributes(path.c_str());}catch(...){}return original_attributes(name);}
inline BOOL WINAPI attributes_ex(LPCWSTR name,GET_FILEEX_INFO_LEVELS level,LPVOID output){try{std::wstring path;if(convert(name,path))return original_attributes_ex(path.c_str(),level,output);}catch(...){}return original_attributes_ex(name,level,output);}
inline bool install(const std::wstring &root){
 if(root.empty()||root.size()>=32767)return false;wchar_t full[32768];DWORD n=GetFullPathNameW(root.c_str(),32768,full,nullptr);if(!n||n>=32768||CompareStringOrdinal(root.c_str(),-1,full,-1,TRUE)!=CSTR_EQUAL)return false;
 profile.assign(full,n);while(profile.size()>3&&profile.back()==L'\\')profile.pop_back();
 HMODULE kernel=GetModuleHandleW(L"kernel32.dll");if(!kernel)return false;
 const char *names[]={"CreateFileW","GetFileAttributesW","GetFileAttributesExW"};void *targets[3]{},*hooks[]={reinterpret_cast<void*>(create),reinterpret_cast<void*>(attributes),reinterpret_cast<void*>(attributes_ex)};void **originals[]={reinterpret_cast<void**>(&original_create),reinterpret_cast<void**>(&original_attributes),reinterpret_cast<void**>(&original_attributes_ex)};
 size_t created=0;for(size_t i=0;i<3;i++){targets[i]=reinterpret_cast<void*>(GetProcAddress(kernel,names[i]));if(!targets[i]||MH_CreateHook(targets[i],hooks[i],originals[i])!=MH_OK)goto fail;++created;}
 for(size_t i=0;i<3;i++)if(MH_QueueEnableHook(targets[i])!=MH_OK)goto fail;
 if(MH_ApplyQueued()!=MH_OK)goto fail;return true;
fail:
 for(size_t i=0;i<created;i++)MH_RemoveHook(targets[i]);return false;
}
}
