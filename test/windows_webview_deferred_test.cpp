#include "../loader/windows/windows_webview_gate.h"
#include <cassert>
using namespace windows_webview_gate;
struct Handler final:Completion {ULONG refs=1;int calls=0;HRESULT result=S_OK;bool reblock=false;
 HRESULT STDMETHODCALLTYPE QueryInterface(REFIID,void **out)override{if(!out)return E_POINTER;*out=this;AddRef();return S_OK;}
 ULONG STDMETHODCALLTYPE AddRef()override{return ++refs;}ULONG STDMETHODCALLTYPE Release()override{return --refs;}
 HRESULT STDMETHODCALLTYPE Invoke(HRESULT r,IUnknown*)override{++calls;result=r;if(reblock)setblocked(true);return S_OK;}
};
struct EnvironmentHandler final:Completion {ULONG refs=1;HWND window;Handler *next;IUnknown *env=nullptr;
 EnvironmentHandler(HWND w,Handler *h):window(w),next(h){}
 HRESULT STDMETHODCALLTYPE QueryInterface(REFIID,void **out)override{if(!out)return E_POINTER;*out=this;AddRef();return S_OK;}
 ULONG STDMETHODCALLTYPE AddRef()override{return ++refs;}ULONG STDMETHODCALLTYPE Release()override{return --refs;}
 HRESULT STDMETHODCALLTYPE Invoke(HRESULT r,IUnknown *value)override{assert(SUCCEEDED(r)&&value);env=value;auto table=*reinterpret_cast<void***>(value);return reinterpret_cast<ControllerCreate>(table[3])(value,window,next);}
};
int wmain(int argc,wchar_t**argv){if(argc!=2)return 2;assert(MH_Initialize()==MH_OK&&install());auto module=LoadLibraryW(argv[1]);assert(module);auto f=reinterpret_cast<Factory>(GetProcAddress(module,"CreateWebViewEnvironmentWithOptionsInternal"));auto refs=reinterpret_cast<ULONG(WINAPI*)()>(GetProcAddress(module,"fixture_environment_refs"));auto closed=reinterpret_cast<int(WINAPI*)()>(GetProcAddress(module,"fixture_closed"));assert(f&&refs&&closed);HWND first=CreateWindowExW(0,L"STATIC",L"",WS_OVERLAPPED,0,0,1,1,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr),second=CreateWindowExW(0,L"STATIC",L"",WS_OVERLAPPED,0,0,1,1,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);assert(first&&second);Handler a,b;EnvironmentHandler ea(first,&a),eb(second,&b);assert(SUCCEEDED(f(false,0,nullptr,nullptr,&ea)));assert(SUCCEEDED(f(false,0,nullptr,nullptr,&eb)));auto s=state();assert(ea.env&&eb.env&&s.deferredController==2&&!s.pendingEnvironment&&!s.pendingController&&!s.controllers&&covered(s,false)&&refs()==3&&a.refs==2&&b.refs==2&&!a.calls&&!b.calls);
 assert(DestroyWindow(first));a.reblock=true;setblocked(false);s=state();assert(s.blocked&&s.deferredController==1&&a.calls==1&&a.result==HRESULT_FROM_WIN32(ERROR_INVALID_WINDOW_HANDLE)&&refs()==2&&a.refs==1&&b.refs==2);a.reblock=false;setblocked(false);s=state();assert(!s.deferredController&&b.calls==1&&SUCCEEDED(b.result)&&s.controllers==1&&refs()==1&&b.refs==1);setblocked(true);assert(retire()&&closed()==1);assert(DestroyWindow(second));FreeLibrary(module);return 0;}
