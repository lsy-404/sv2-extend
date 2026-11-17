#include "windows_webview_live_probe.h"
#include <cassert>
int wmain(int argc,wchar_t **argv){
 if(argc!=3)return 2;assert(MH_Initialize()==MH_OK);assert(windows_webview_gate::install());auto runtime=LoadLibraryW(argv[1]);assert(runtime);assert(GetProcAddress(runtime,"CreateWebViewEnvironmentWithOptionsInternal"));
 windows_webview_live_probe::Probe probe(GetCurrentThreadId());assert(probe.start(argv[2])==HRESULT_FROM_WIN32(ERROR_INVALID_STATE));windows_webview_gate::setblocked(false);
 assert(SUCCEEDED(probe.start(argv[2])));auto value=probe.state();assert(!value.pending&&value.environmentCompleted&&value.controllerCompleted&&value.controllerCreated&&value.windowExists&&value.error==0);assert(windows_webview_gate::state().controllers==1);assert(!probe.close());
 windows_webview_gate::setblocked(true);assert(windows_webview_gate::retire());assert(probe.close());assert(!probe.state().windowExists);FreeLibrary(runtime);return 0;
}
