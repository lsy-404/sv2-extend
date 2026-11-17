#include "windows_webview_live_probe.h"
#include <cassert>
int wmain(int argc,wchar_t **argv){
 if(argc!=4)return 2;assert(MH_Initialize()==MH_OK);assert(windows_socket_gate::install());assert(windows_child_gate::install());assert(windows_webview_gate::install(std::filesystem::path(argv[2]).parent_path()));auto runtime=LoadLibraryW(argv[1]);assert(runtime);assert(GetProcAddress(runtime,"CreateWebViewEnvironmentWithOptionsInternal"));
 windows_webview_live_probe::Probe probe(GetCurrentThreadId());assert(SUCCEEDED(probe.start(argv[2])));auto value=probe.state();assert(!value.pending&&value.environmentCompleted&&value.controllerCompleted&&value.controllerCreated&&value.windowExists&&value.error==0);assert(windows_webview_gate::state().controllers==1);
 windows_webview_live_probe::Probe other(GetCurrentThreadId());assert(SUCCEEDED(other.start(argv[3])));assert(windows_webview_gate::state().controllers==2);
 assert(probe.close());assert(!probe.state().windowExists);assert(other.state().windowExists&&windows_webview_gate::state().controllers==1);assert(other.close());assert(windows_webview_gate::state().controllers==0);FreeLibrary(runtime);return 0;
}
