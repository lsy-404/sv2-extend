#include "windows_runtime_message_probe.h"
#include <cassert>
int main(){
 WNDCLASSW wc{};wc.lpfnWndProc=DefWindowProcW;wc.hInstance=GetModuleHandleW(nullptr);wc.lpszClassName=L"NetworkMessageFixture";assert(RegisterClassW(&wc));HWND window=CreateWindowW(wc.lpszClassName,L"",0,0,0,0,0,HWND_MESSAGE,nullptr,wc.hInstance,nullptr);assert(window);assert(runtime_message_probe::install(GetCurrentThreadId(),window));
 assert(PostThreadMessageW(GetCurrentThreadId(),WM_APP+11,9,7));assert(PostMessageW(window,WM_APP+12,8,6));MSG message{};while(PeekMessageW(&message,nullptr,0,0,PM_REMOVE)){}
 runtime_message_probe::record_denial("create",L"C:\\private\\sample.exe",GetModuleHandleW(nullptr));
 runtime_message_probe::record_denial("shell",L"demo://example.test/private?token=value",GetModuleHandleW(nullptr));
 runtime_message_probe::record_denial("shell",L"demo://user:password@example.test/private",GetModuleHandleW(nullptr));
 auto result=runtime_message_probe::snapshot();assert(result["installed"]==true&&result["denials"]==3);assert(result["samples"][0]["basename"]=="sample.exe");assert(result["samples"][1]["target"]["scheme"]=="demo");assert(result["samples"][1]["target"]["host"]=="example.test");assert(!result["samples"][2]["target"].contains("host"));
 bool thread=false,dispatch=false;for(const auto &entry:result["messages"]){if(entry["id"]==WM_APP+11&&entry["target"]=="null")thread=true;if(entry["id"]==WM_APP+12&&entry["target"]=="dispatcher")dispatch=true;}assert(thread&&dispatch);
 auto body=result.dump();assert(body.find("private")==std::string::npos&&body.find("token")==std::string::npos&&body.find("password")==std::string::npos);DestroyWindow(window);return 0;
}
