#include "../loader/windows/windows_webview_gate.h"
#include <cassert>
int wmain(int argc,wchar_t **argv){
 if(argc!=3)return 2;assert(MH_Initialize()==MH_OK);assert(windows_socket_gate::install());assert(windows_child_gate::install());assert(windows_webview_gate::install(argv[2]));auto module=LoadLibraryW(argv[1]);assert(module);
 auto function=reinterpret_cast<windows_webview_gate::Factory>(GetProcAddress(module,"CreateWebViewEnvironmentWithOptionsInternal"));auto calls=reinterpret_cast<int(WINAPI*)()>(GetProcAddress(module,"fixture_calls"));assert(function&&calls);
 assert(!windows_webview_transport::state().installed);assert(function(false,0,nullptr,nullptr,nullptr)==E_INVALIDARG);assert(calls()==0&&!windows_webview_gate::state().onlineFactorySeen&&!windows_webview_transport::state().installed);
 windows_webview_gate::setblocked(false);assert(function(false,0,nullptr,nullptr,nullptr)==E_INVALIDARG);assert(calls()==0);FreeLibrary(module);return 0;
}
