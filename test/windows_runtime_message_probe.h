#pragma once
#include <windows.h>
#include <algorithm>
#include <atomic>
#include <map>
#include <mutex>
#include <string>
#include <vector>
#include "../loader/windows/vendor/json.hpp"
namespace runtime_message_probe {
using json=nlohmann::json;
struct Key {UINT id;unsigned target;bool operator<(const Key &other)const{return id<other.id||(id==other.id&&target<other.target);}};
inline std::mutex mutex;
inline std::map<Key,uint64_t> counts;
inline std::vector<json> samples;
inline std::atomic<uint64_t> denials{0};
inline uint64_t overflow=0;
inline DWORD owner=0;inline HWND dispatcher=nullptr;inline HHOOK hook=nullptr;
inline std::string utf8(const std::wstring &value){if(value.empty())return {};int n=WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,value.data(),static_cast<int>(value.size()),nullptr,0,nullptr,nullptr);if(n<=0)return {};std::string output(size_t(n),'\0');return WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,value.data(),static_cast<int>(value.size()),output.data(),n,nullptr,nullptr)==n?output:std::string();}
inline std::wstring limited(const wchar_t *value){if(!value)return {};size_t n=0;while(n<2048&&value[n])++n;if(n==2048)return {};return std::wstring(value,n);}
inline std::string basename(const wchar_t *path){std::wstring value=limited(path);auto pos=value.find_last_of(L"\\/");if(pos!=std::wstring::npos)value=value.substr(pos+1);if(value.size()>255)return {};return utf8(value);}
inline bool scheme_char(wchar_t c,bool first){return (c>=L'a'&&c<=L'z')||(c>=L'A'&&c<=L'Z')||(!first&&((c>=L'0'&&c<=L'9')||c==L'+'||c==L'-'||c==L'.'));}
inline json shell_target(const wchar_t *input){
 std::wstring value=limited(input);auto colon=value.find(L':');if(colon==std::wstring::npos||colon==0||colon>32)return json::object();
 for(size_t i=0;i<colon;i++)if(!scheme_char(value[i],i==0))return json::object();
 std::wstring scheme=value.substr(0,colon);for(auto &ch:scheme)if(ch>=L'A'&&ch<=L'Z')ch+=L'a'-L'A';json result={{"scheme",utf8(scheme)}};
 if(value.compare(colon+1,2,L"//")!=0)return result;size_t begin=colon+3,end=value.find_first_of(L"/?#",begin);if(end==std::wstring::npos)end=value.size();if(end<=begin||end-begin>255)return result;
 std::wstring authority=value.substr(begin,end-begin);if(authority.find(L'@')!=std::wstring::npos)return result;
 std::wstring host;if(authority[0]==L'['){auto close=authority.find(L']');if(close==std::wstring::npos)return result;host=authority.substr(0,close+1);}else{auto port=authority.find(L':');host=authority.substr(0,port);}
 if(host.empty())return result;for(auto ch:host)if(!((ch>=L'a'&&ch<=L'z')||(ch>=L'A'&&ch<=L'Z')||(ch>=L'0'&&ch<=L'9')||ch==L'.'||ch==L'-'||ch==L'['||ch==L']'||ch==L':'))return result;
 result["host"]=utf8(host);return result;
}
inline std::string caller_module(const void *address){HMODULE module=nullptr;if(!address||!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,reinterpret_cast<LPCWSTR>(address),&module))return {};wchar_t path[32768];DWORD n=GetModuleFileNameW(module,path,32768);if(!n||n>=32768)return {};return basename(path);}
inline void record_denial(const char *kind,const wchar_t *target,const void *caller){
 if(denials.fetch_add(1)>=8)return;std::lock_guard<std::mutex> lock(mutex);if(samples.size()>=8)return;
 json sample;std::string category=kind?kind:"";
 if(category=="create"){sample["kind"]="create";auto name=basename(target);if(name.find_first_of(" ?#\"'\t\r\n")!=std::string::npos)name.clear();sample["basename"]=name;}
 else if(category=="shell"){sample["kind"]="shell";sample["target"]=shell_target(target);}
 else return;
 sample["callerModule"]=caller_module(caller);samples.push_back(std::move(sample));
}
inline LRESULT CALLBACK observe(int code,WPARAM w,LPARAM l){if(code>=0&&l){auto message=reinterpret_cast<const MSG*>(l);Key key{message->message,message->hwnd==nullptr?0u:message->hwnd==dispatcher?1u:2u};std::lock_guard<std::mutex> lock(mutex);auto it=counts.find(key);if(it!=counts.end())++it->second;else if(counts.size()<128)counts.emplace(key,1);else ++overflow;}return CallNextHookEx(hook,code,w,l);}
inline bool install(DWORD thread,HWND window){if(!thread||GetCurrentThreadId()!=thread||!window)return false;if(hook)return owner==thread&&dispatcher==window;owner=thread;dispatcher=window;hook=SetWindowsHookExW(WH_GETMESSAGE,observe,nullptr,thread);return hook!=nullptr;}
inline json snapshot(){
 std::lock_guard<std::mutex> lock(mutex);std::vector<std::pair<Key,uint64_t>> ordered(counts.begin(),counts.end());std::sort(ordered.begin(),ordered.end(),[](const auto&a,const auto&b){return a.second>b.second;});json messages=json::array();for(size_t i=0;i<ordered.size()&&i<20;i++){auto &entry=ordered[i];messages.push_back({{"id",entry.first.id},{"target",entry.first.target==0?"null":entry.first.target==1?"dispatcher":"other"},{"count",entry.second}});}return {{"installed",hook!=nullptr},{"denials",denials.load()},{"samples",samples},{"messages",messages},{"otherMessages",overflow}};
}
}
