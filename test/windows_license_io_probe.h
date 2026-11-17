#pragma once
#include <windows.h>
#include <algorithm>
#include <string>
#include <vector>
#include <map>
#include "../loader/windows/vendor/json.hpp"
#include "../loader/windows/vendor/minhook/include/MinHook.h"
namespace license_io_probe {
using json=nlohmann::json;
static decltype(&CreateFileW) create_original;
static decltype(&MoveFileW) move_original;
static decltype(&MoveFileExW) move_ex_original;
static decltype(&ReplaceFileW) replace_original;
static decltype(&DeleteFileW) delete_original;
static decltype(&GetFileAttributesW) attributes_original;
static decltype(&FindFirstFileW) find_original;
static decltype(&FindFirstFileExW) find_ex_original;
static decltype(&GetFileAttributesExW) attributes_ex_original;
static decltype(&FindClose) find_close_original;
static decltype(&CloseHandle) close_original;
static DWORD observed_thread=0;static std::wstring root;static bool enabled=false;static thread_local bool recording=false;static json events=json::array();static unsigned dropped=0;static std::map<uintptr_t,std::wstring> files,finds;
inline std::wstring absolute(const wchar_t *path){if(!path||!*path)return {};DWORD count=GetFullPathNameW(path,0,nullptr,nullptr);if(!count||count>32767)return {};std::wstring result(count,L'\0');DWORD written=GetFullPathNameW(path,count,result.data(),nullptr);if(!written||written>=count)return {};result.resize(written);return result;}
inline bool relative(const wchar_t *raw,std::wstring &result){
 auto value=absolute(raw);if(value.size()<root.size()||CompareStringOrdinal(value.c_str(),static_cast<int>(root.size()),root.c_str(),static_cast<int>(root.size()),TRUE)!=CSTR_EQUAL)return false;
 if(value.size()==root.size()){result=L".";return true;}if(value[root.size()]!=L'\\')return false;result=value.substr(root.size()+1);return true;
}
inline std::string utf8(const std::wstring &value){if(value.empty())return {};int size=WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,value.data(),static_cast<int>(value.size()),nullptr,0,nullptr,nullptr);if(size<=0)return {};std::string result(size,'\0');if(!WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,value.data(),static_cast<int>(value.size()),result.data(),size,nullptr,nullptr))return {};return result;}
inline void note(const char *op,const wchar_t *first,const wchar_t *second,DWORD flags,bool success,DWORD error,DWORD disposition=0,DWORD file_flags=0){
 if(!enabled||recording||GetCurrentThreadId()!=observed_thread)return;recording=true;
 try{std::wstring a,b;bool insideA=relative(first,a),insideB=second&&relative(second,b);if(insideA||insideB){if(events.size()>=256)++dropped;else{json value={{"operation",op},{"success",success},{"flags",flags}};if(!success)value["error"]=error;if(disposition){value["disposition"]=disposition;value["fileFlags"]=file_flags;}if(insideA)value["path"]=utf8(a);if(second){value["secondInScope"]=insideB;if(insideB)value["secondPath"]=utf8(b);}events.push_back(std::move(value));}}}catch(...){++dropped;}recording=false;
}
inline void remember(HANDLE handle,const wchar_t *path,bool search){if(!enabled||GetCurrentThreadId()!=observed_thread||handle==INVALID_HANDLE_VALUE||!handle)return;try{std::wstring relativePath;if(relative(path,relativePath))(search?finds:files)[reinterpret_cast<uintptr_t>(handle)]=std::move(relativePath);}catch(...){++dropped;}}
inline void closed(HANDLE handle,bool search,bool success,DWORD error){if(!enabled||GetCurrentThreadId()!=observed_thread)return;auto &map=search?finds:files;auto it=map.find(reinterpret_cast<uintptr_t>(handle));if(it==map.end())return;try{if(events.size()>=256)++dropped;else{json value={{"operation",search?"FindClose":"CloseHandle"},{"path",utf8(it->second)},{"success",success}};if(!success)value["error"]=error;events.push_back(std::move(value));}if(success)map.erase(it);}catch(...){++dropped;}}
static HANDLE WINAPI create(const wchar_t *path,DWORD access,DWORD share,LPSECURITY_ATTRIBUTES attributes,DWORD disposition,DWORD flags,HANDLE templateFile){auto result=create_original(path,access,share,attributes,disposition,flags,templateFile);auto error=GetLastError();note("CreateFileW",path,nullptr,access,result!=INVALID_HANDLE_VALUE,error,disposition,flags);remember(result,path,false);SetLastError(error);return result;}
static BOOL WINAPI move(const wchar_t *a,const wchar_t *b){auto result=move_original(a,b);auto error=GetLastError();note("MoveFileW",a,b,0,result!=FALSE,error);SetLastError(error);return result;}
static BOOL WINAPI move_ex(const wchar_t *a,const wchar_t *b,DWORD flags){auto result=move_ex_original(a,b,flags);auto error=GetLastError();note("MoveFileExW",a,b,flags,result!=FALSE,error);SetLastError(error);return result;}
static BOOL WINAPI replace(const wchar_t *a,const wchar_t *b,const wchar_t *backup,DWORD flags,LPVOID excluded,LPVOID reserved){auto result=replace_original(a,b,backup,flags,excluded,reserved);auto error=GetLastError();note("ReplaceFileW",a,b,flags,result!=FALSE,error);if(backup)note("ReplaceFileW.backup",backup,nullptr,flags,result!=FALSE,error);SetLastError(error);return result;}
static BOOL WINAPI remove(const wchar_t *path){auto result=delete_original(path);auto error=GetLastError();note("DeleteFileW",path,nullptr,0,result!=FALSE,error);SetLastError(error);return result;}
static DWORD WINAPI attributes(const wchar_t *path){auto result=attributes_original(path);auto error=GetLastError();note("GetFileAttributesW",path,nullptr,0,result!=INVALID_FILE_ATTRIBUTES,error);SetLastError(error);return result;}
static HANDLE WINAPI find(const wchar_t *path,LPWIN32_FIND_DATAW data){auto result=find_original(path,data);auto error=GetLastError();note("FindFirstFileW",path,nullptr,0,result!=INVALID_HANDLE_VALUE,error);remember(result,path,true);SetLastError(error);return result;}
static HANDLE WINAPI find_ex(const wchar_t *path,FINDEX_INFO_LEVELS info,LPVOID data,FINDEX_SEARCH_OPS search,LPVOID filter,DWORD flags){auto result=find_ex_original(path,info,data,search,filter,flags);auto error=GetLastError();note("FindFirstFileExW",path,nullptr,flags,result!=INVALID_HANDLE_VALUE,error);remember(result,path,true);SetLastError(error);return result;}
static BOOL WINAPI attributes_ex(const wchar_t *path,GET_FILEEX_INFO_LEVELS level,LPVOID data){auto result=attributes_ex_original(path,level,data);auto error=GetLastError();note("GetFileAttributesExW",path,nullptr,0,result!=FALSE,error);SetLastError(error);return result;}
static BOOL WINAPI find_close(HANDLE handle){auto result=find_close_original(handle);auto error=GetLastError();closed(handle,true,result!=FALSE,error);SetLastError(error);return result;}
static BOOL WINAPI close_handle(HANDLE handle){auto result=close_original(handle);auto error=GetLastError();closed(handle,false,result!=FALSE,error);SetLastError(error);return result;}
inline bool install(){
 auto module=GetModuleHandleW(L"kernel32.dll");if(!module)return false;
 struct Hook{const char *name;void *detour;void **original;};Hook hooks[]={
  {"CreateFileW",reinterpret_cast<void*>(create),reinterpret_cast<void**>(&create_original)}, {"MoveFileW",reinterpret_cast<void*>(move),reinterpret_cast<void**>(&move_original)},
  {"MoveFileExW",reinterpret_cast<void*>(move_ex),reinterpret_cast<void**>(&move_ex_original)}, {"ReplaceFileW",reinterpret_cast<void*>(replace),reinterpret_cast<void**>(&replace_original)},
  {"DeleteFileW",reinterpret_cast<void*>(remove),reinterpret_cast<void**>(&delete_original)}, {"GetFileAttributesW",reinterpret_cast<void*>(attributes),reinterpret_cast<void**>(&attributes_original)},
  {"FindFirstFileW",reinterpret_cast<void*>(find),reinterpret_cast<void**>(&find_original)}, {"FindFirstFileExW",reinterpret_cast<void*>(find_ex),reinterpret_cast<void**>(&find_ex_original)},
  {"GetFileAttributesExW",reinterpret_cast<void*>(attributes_ex),reinterpret_cast<void**>(&attributes_ex_original)}, {"FindClose",reinterpret_cast<void*>(find_close),reinterpret_cast<void**>(&find_close_original)},
  {"CloseHandle",reinterpret_cast<void*>(close_handle),reinterpret_cast<void**>(&close_original)}};
 std::vector<void*> installed;
 for(const auto &hook:hooks){auto target=reinterpret_cast<void*>(GetProcAddress(module,hook.name));if(!target||MH_CreateHook(target,hook.detour,hook.original)!=MH_OK){for(auto item:installed)MH_RemoveHook(item);return false;}installed.push_back(target);}
 for(auto target:installed)if(MH_QueueEnableHook(target)!=MH_OK){for(auto item:installed)MH_RemoveHook(item);return false;}
 if(MH_ApplyQueued()!=MH_OK){for(auto item:installed)MH_RemoveHook(item);return false;}return true;
}
inline bool arm(const std::wstring &host_data,DWORD thread){
 if(!thread||GetCurrentThreadId()!=thread||enabled)return false;auto path=absolute((host_data+L"\\Roaming\\Dreamtonics\\Synthesizer V Studio 2\\license").c_str());if(path.empty())return false;
 while(path.size()>3&&path.back()==L'\\')path.pop_back();root=std::move(path);observed_thread=thread;events=json::array();files.clear();finds.clear();dropped=0;enabled=true;return true;
}
inline json finish(){if(GetCurrentThreadId()!=observed_thread||!enabled)return {{"ok",false}};enabled=false;return {{"ok",true},{"events",events},{"dropped",dropped},{"openFileHandles",files.size()},{"openFindHandles",finds.size()}};}
}
