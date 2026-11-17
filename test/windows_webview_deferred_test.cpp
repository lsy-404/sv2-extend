#include "../loader/windows/windows_webview_gate.h"
#include <cassert>
using namespace windows_webview_gate;
struct Object final:Completion {ULONG refs=1;int calls=0;bool reblock=false;HRESULT result=S_OK,reply=S_OK;
 HRESULT STDMETHODCALLTYPE QueryInterface(REFIID,void **out)override{if(!out)return E_POINTER;*out=this;AddRef();return S_OK;}
 ULONG STDMETHODCALLTYPE AddRef()override{return ++refs;}
 ULONG STDMETHODCALLTYPE Release()override{return --refs;}
 HRESULT STDMETHODCALLTYPE Invoke(HRESULT r,IUnknown*)override{++calls;result=r;if(reblock)setblocked(true);return reply;}
};
int wmain(int argc,wchar_t**argv){if(argc!=2)return 2;assert(MH_Initialize()==MH_OK&&install());auto module=LoadLibraryW(argv[1]);assert(module);auto f=reinterpret_cast<Factory>(GetProcAddress(module,"CreateWebViewEnvironmentWithOptionsInternal"));auto calls=reinterpret_cast<int(WINAPI*)()>(GetProcAddress(module,"fixture_calls"));auto args=reinterpret_cast<BOOL(WINAPI*)(PCWSTR,IUnknown*)>(GetProcAddress(module,"fixture_arguments"));assert(f&&calls&&args);Object handler,options;wchar_t folder[]=L"original";
 assert(SUCCEEDED(f(false,0,folder,&options,&handler)));folder[0]=L'X';assert(SUCCEEDED(f(false,0,L"second",&options,&handler)));auto s=state();assert(s.deferred==2&&s.pendingEnvironment==0&&calls()==0&&handler.calls==0&&handler.refs==3&&options.refs==3&&covered(s,false));
 handler.reblock=true;setblocked(false);s=state();assert(s.blocked&&s.deferred==1&&calls()==1&&handler.calls==1&&handler.refs==2&&options.refs==2&&args(L"original",&options));assert(covered(s,false));
 handler.reblock=false;setblocked(false);s=state();assert(!s.blocked&&s.deferred==0&&calls()==2&&handler.calls==2&&handler.refs==1&&options.refs==1&&args(L"second",&options));setblocked(true);assert(retire());assert(SUCCEEDED(f(false,3,L"error",&options,&handler)));assert(handler.calls==2&&state().deferred==1);setblocked(false);assert(handler.calls==3&&handler.result==E_FAIL&&handler.refs==1&&options.refs==1&&state().deferred==0);setblocked(true);handler.reply=E_FAIL;assert(SUCCEEDED(f(false,0,L"callback-error",&options,&handler)));setblocked(false);assert(handler.calls==4&&handler.refs==1&&options.refs==1);FreeLibrary(module);return 0;}
