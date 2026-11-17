#include "../loader/windows/windows_webview_gate.h"
#include <cassert>
using namespace windows_webview_gate;
static bool close_immediately=false;
struct Handler final:Completion {
 bool environment;int calls=0;explicit Handler(bool e):environment(e){}
 HRESULT STDMETHODCALLTYPE QueryInterface(REFIID,void **out)override{if(!out)return E_POINTER;*out=this;return S_OK;}
 ULONG STDMETHODCALLTYPE AddRef()override{return 2;}
 ULONG STDMETHODCALLTYPE Release()override{return 1;}
 HRESULT STDMETHODCALLTYPE Invoke(HRESULT result,IUnknown *value)override{
  ++calls;if(FAILED(result))return S_OK;if(environment){auto table=*reinterpret_cast<void***>(value);static Handler controller(false);HRESULT r=reinterpret_cast<ControllerCreate>(table[3])(value,nullptr,&controller);assert(SUCCEEDED(r));}else if(close_immediately){auto table=*reinterpret_cast<void***>(value);assert(SUCCEEDED(reinterpret_cast<ControllerClose>(table[24])(value)));}return S_OK;
 }
};
int wmain(int argc,wchar_t **argv){if(argc!=2)return 2;assert(MH_Initialize()==MH_OK);assert(install());auto module=LoadLibraryW(argv[1]);assert(module);auto function=reinterpret_cast<Factory>(GetProcAddress(module,"CreateWebViewEnvironmentWithOptionsInternal"));auto closed=reinterpret_cast<int(WINAPI*)()>(GetProcAddress(module,"fixture_closed"));assert(function&&closed);
 Handler handler(true);setblocked(false);assert(SUCCEEDED(function(false,0,nullptr,nullptr,&handler)));auto observed=state();assert(handler.calls==1&&observed.coverage&&observed.controllers==1&&!observed.pendingEnvironment&&!observed.pendingController);
 setblocked(true);assert(retire());assert(closed()==1&&state().controllers==0);
 auto completeEnv=reinterpret_cast<HRESULT(WINAPI*)()>(GetProcAddress(module,"fixture_complete_env"));auto completeController=reinterpret_cast<HRESULT(WINAPI*)()>(GetProcAddress(module,"fixture_complete_controller"));assert(completeEnv&&completeController);
 setblocked(false);assert(SUCCEEDED(function(false,1,nullptr,nullptr,&handler)));assert(state().pendingEnvironment==1);setblocked(true);assert(!retire());assert(SUCCEEDED(completeEnv()));assert(state().pendingEnvironment==0&&retire());
 setblocked(false);assert(SUCCEEDED(function(false,2,nullptr,nullptr,&handler)));assert(state().pendingController==1);setblocked(true);assert(!retire());assert(SUCCEEDED(completeController()));assert(state().pendingController==0&&closed()==2&&retire());
 setblocked(false);close_immediately=true;assert(SUCCEEDED(function(false,0,nullptr,nullptr,&handler)));assert(closed()==3&&state().controllers==0);setblocked(true);assert(retire());
 FreeLibrary(module);return 0;}
