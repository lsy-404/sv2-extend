#pragma once
#include <windows.h>
#include <atomic>
#include <memory>
#include <string>
#include "../host_navigation_registry.h"
namespace windows_host_navigation {
inline constexpr UINT message=WM_APP+3;
inline navigation_registry registry{};
inline DWORD thread=0;
inline HWND dispatcher=nullptr;
inline std::atomic<unsigned> count{0};
struct Request {
 std::string url;
 HANDLE event=CreateEventW(nullptr,TRUE,FALSE,nullptr);
 std::atomic<int> phase{0};
 int result=-1;
 ~Request(){if(event)CloseHandle(event);}
};
inline void install(HWND window,DWORD owner){dispatcher=window;thread=owner;}
inline int add(const char *owner,loader_host_navigation_handler handler,void *context){
 if(GetCurrentThreadId()!=thread)return LOADER_ERR_INVALID;
 int result=navigation_register(&registry,owner,handler,context);if(result==LOADER_OK)++count;return result;
}
inline int remove(const char *owner,loader_host_navigation_handler handler,void *context){
 if(GetCurrentThreadId()!=thread)return LOADER_ERR_INVALID;
 int result=navigation_unregister(&registry,owner,handler,context);if(result==LOADER_OK)--count;return result;
}
inline int dispatch(const char *url){
 static thread_local unsigned depth=0;if(depth>=8)return -1;
 ++depth;int result=-1;try{result=navigation_dispatch(&registry,url);}catch(...){result=-1;}--depth;return result;
}
inline LRESULT receive(LPARAM value){
 std::unique_ptr<std::shared_ptr<Request>> holder(reinterpret_cast<std::shared_ptr<Request>*>(value));
 auto request=*holder;int expected=0;
 if(request->phase.compare_exchange_strong(expected,1)){request->result=dispatch(request->url.c_str());request->phase.store(2);SetEvent(request->event);}return 0;
}
inline int route(LPCWSTR url){
 if(!count.load())return 0;
 size_t length=0;while(url[length]&&length<32768)++length;if(!length||length==32768)return -1;
 int size=WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,url,static_cast<int>(length),nullptr,0,nullptr,nullptr);if(size<=0)return -1;
 std::string text(size,'\0');if(!WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,url,static_cast<int>(length),text.data(),size,nullptr,nullptr))return -1;
 if(GetCurrentThreadId()==thread)return dispatch(text.c_str());
 try{
  auto request=std::make_shared<Request>();if(!request->event)return -1;request->url=std::move(text);
  auto queued=new std::shared_ptr<Request>(request);if(!PostMessageW(dispatcher,message,0,reinterpret_cast<LPARAM>(queued))){delete queued;return -1;}
  if(WaitForSingleObject(request->event,2000)==WAIT_OBJECT_0)return request->result;
  int expected=0;request->phase.compare_exchange_strong(expected,3);return -1;
 }catch(...){return -1;}
}
}
