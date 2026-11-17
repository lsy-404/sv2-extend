#include "../loader/windows/windows_host_navigation.h"
#include "../loader/windows/windows_child_gate.h"
#include <cassert>
#include <thread>
static int calls=0;
static int handler(const char *url,void*){
 assert(GetCurrentThreadId()==windows_host_navigation::thread);++calls;
 return !strcmp(url,"example:handled")?1:!strcmp(url,"example:rejected")?-1:0;
}
static LRESULT CALLBACK procedure(HWND window,UINT message,WPARAM w,LPARAM l){
 if(message==windows_host_navigation::message)return windows_host_navigation::receive(l);
 return DefWindowProcW(window,message,w,l);
}
int main(){
 WNDCLASSW cls{};cls.lpfnWndProc=procedure;cls.hInstance=GetModuleHandleW(nullptr);cls.lpszClassName=L"NavigationFixture";assert(RegisterClassW(&cls));
 HWND window=CreateWindowW(cls.lpszClassName,L"",0,0,0,0,0,HWND_MESSAGE,nullptr,cls.hInstance,nullptr);assert(window);
 windows_host_navigation::install(window,GetCurrentThreadId());windows_child_gate::navigation=windows_host_navigation::route;
 assert(windows_host_navigation::add("fixture",handler,nullptr)==LOADER_OK);
 assert(MH_Initialize()==MH_OK&&windows_child_gate::install());
 assert(windows_child_gate::navigation_url(L"open",L"example:handled",nullptr));
 assert(!windows_child_gate::navigation_url(L"open",L"C:\\file",nullptr));
 assert(!windows_child_gate::navigation_url(L"runas",L"example:handled",nullptr));
 assert(!windows_child_gate::navigation_url(L"open",L"example:handled",L"argument"));
 assert(reinterpret_cast<INT_PTR>(ShellExecuteW(nullptr,L"open",L"example:handled",nullptr,nullptr,SW_HIDE))>32);
 assert(reinterpret_cast<INT_PTR>(ShellExecuteW(nullptr,L"open",L"example:rejected",nullptr,nullptr,SW_HIDE))==SE_ERR_ACCESSDENIED);
 assert(reinterpret_cast<INT_PTR>(ShellExecuteW(nullptr,L"open",L"example:unhandled",nullptr,nullptr,SW_HIDE))==SE_ERR_ACCESSDENIED);
 std::atomic<bool> complete{false};int result=0;
 std::thread worker([&]{assert(windows_host_navigation::add("other",handler,nullptr)==LOADER_ERR_INVALID);result=windows_host_navigation::route(L"example:handled");complete=true;});
 auto deadline=GetTickCount64()+4000;
 while(!complete&&GetTickCount64()<deadline){MSG message;while(PeekMessageW(&message,nullptr,0,0,PM_REMOVE))DispatchMessageW(&message);Sleep(1);}
 worker.join();assert(result==1&&calls==4);
 std::thread expired([&]{result=windows_host_navigation::route(L"example:handled");});expired.join();assert(result<0);
 MSG message;while(PeekMessageW(&message,nullptr,0,0,PM_REMOVE))DispatchMessageW(&message);assert(calls==4);
 assert(windows_host_navigation::remove("fixture",handler,nullptr)==LOADER_OK);assert(windows_host_navigation::route(L"example:handled")==0);
 DestroyWindow(window);return 0;
}
