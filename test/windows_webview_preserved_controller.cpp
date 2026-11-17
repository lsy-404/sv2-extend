#include "../loader/windows/windows_webview_gate.h"
#include <cstdio>
#include <string>

using namespace windows_webview_gate;
static HWND window_handle;
static IUnknown *controller, *web;
static bool controller_done, script_done, capture_done;
static HRESULT controller_result=E_PENDING, script_result=E_PENDING;
static HRESULT capture_result=E_PENDING;
static std::wstring script_value;
struct ScriptHandler : IUnknown {
 ULONG refs=1;
 HRESULT STDMETHODCALLTYPE QueryInterface(REFIID,void **out) override {if(!out)return E_POINTER;*out=this;AddRef();return S_OK;}
 ULONG STDMETHODCALLTYPE AddRef() override{return ++refs;}
 ULONG STDMETHODCALLTYPE Release() override{auto n=--refs;if(!n)delete this;return n;}
 virtual HRESULT STDMETHODCALLTYPE Invoke(HRESULT r,PCWSTR value){script_result=r;script_value=value?value:L"";script_done=true;return S_OK;}
};
struct CaptureHandler : IUnknown {
 ULONG refs=1;
 HRESULT STDMETHODCALLTYPE QueryInterface(REFIID,void **out) override {if(!out)return E_POINTER;*out=this;AddRef();return S_OK;}
 ULONG STDMETHODCALLTYPE AddRef() override{return ++refs;}
 ULONG STDMETHODCALLTYPE Release() override{auto n=--refs;if(!n)delete this;return n;}
 virtual HRESULT STDMETHODCALLTYPE Invoke(HRESULT result){capture_result=result;capture_done=true;return S_OK;}
};
struct Handler : Completion {
 ULONG refs=1;bool environment;
 explicit Handler(bool value):environment(value){}
 HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid,void **out) override {if(!out)return E_POINTER;*out=nullptr;if(IsEqualIID(iid,IID_IUnknown)||IsEqualIID(iid,environment?env_handler_iid:controller_handler_iid)){*out=this;AddRef();return S_OK;}return E_NOINTERFACE;}
 ULONG STDMETHODCALLTYPE AddRef() override{return ++refs;}
 ULONG STDMETHODCALLTYPE Release() override{auto n=--refs;if(!n)delete this;return n;}
 HRESULT STDMETHODCALLTYPE Invoke(HRESULT result,IUnknown *object) override {
  if(environment){if(FAILED(result)||!object){controller_result=result;controller_done=true;return S_OK;}auto handler=new Handler(false);auto table=*reinterpret_cast<void***>(object);auto started=reinterpret_cast<ControllerCreate>(table[3])(object,window_handle,handler);handler->Release();if(FAILED(started)){controller_result=started;controller_done=true;}}
  else{controller_done=true;controller_result=result;if(SUCCEEDED(result)&&object){controller=object;controller->AddRef();auto table=*reinterpret_cast<void***>(controller);RECT bounds{};GetClientRect(window_handle,&bounds);controller_result=reinterpret_cast<HRESULT(WINAPI*)(IUnknown*,RECT)>(table[6])(controller,bounds);if(SUCCEEDED(controller_result))controller_result=reinterpret_cast<HRESULT(WINAPI*)(IUnknown*,IUnknown**)>(table[25])(controller,&web);}}
  return S_OK;
 }
};
static bool wait(bool &done){auto deadline=GetTickCount64()+15000;while(!done&&GetTickCount64()<deadline){MSG msg{};while(PeekMessageW(&msg,nullptr,0,0,PM_REMOVE)){TranslateMessage(&msg);DispatchMessageW(&msg);}Sleep(10);}return done;}
static bool evaluate(PCWSTR expression,const wchar_t *expected){script_done=false;auto callback=new ScriptHandler;auto table=*reinterpret_cast<void***>(web);HRESULT result=reinterpret_cast<HRESULT(WINAPI*)(IUnknown*,PCWSTR,IUnknown*)>(table[29])(web,expression,callback);callback->Release();return SUCCEEDED(result)&&wait(script_done)&&SUCCEEDED(script_result)&&script_value==expected;}
static bool capture(){IStream *stream=nullptr;if(FAILED(CreateStreamOnHGlobal(nullptr,TRUE,&stream)))return false;capture_done=false;auto handler=new CaptureHandler;auto table=*reinterpret_cast<void***>(web);HRESULT result=reinterpret_cast<HRESULT(WINAPI*)(IUnknown*,int,IStream*,IUnknown*)>(table[30])(web,0,stream,handler);handler->Release();STATSTG stats{};bool ok=SUCCEEDED(result)&&wait(capture_done)&&SUCCEEDED(capture_result)&&SUCCEEDED(stream->Stat(&stats,STATFLAG_NONAME))&&stats.cbSize.QuadPart>100;stream->Release();return ok;}
int wmain(int argc,wchar_t **argv){
 if(argc!=4)return 2;bool retire_old=!wcscmp(argv[3],L"retire");if(!retire_old&&wcscmp(argv[3],L"preserve"))return 2;
 if(FAILED(CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED))||MH_Initialize()!=MH_OK||!install())return 3;
 HMODULE runtime=LoadLibraryExW(argv[1],nullptr,LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR|LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);if(!runtime)return 4;
 auto create=reinterpret_cast<Factory>(GetProcAddress(runtime,"CreateWebViewEnvironmentWithOptionsInternal"));if(!create)return 5;
 window_handle=CreateWindowExW(0,L"STATIC",L"",WS_OVERLAPPEDWINDOW,0,0,400,300,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);if(!window_handle)return 6;ShowWindow(window_handle,SW_SHOW);
 setblocked(false);auto handler=new Handler(true);HRESULT started=create(false,0,argv[2],nullptr,handler);handler->Release();if(FAILED(started)||!wait(controller_done)||FAILED(controller_result)||!web)return 7;
 auto table=*reinterpret_cast<void***>(web);if(FAILED(reinterpret_cast<HRESULT(WINAPI*)(IUnknown*,PCWSTR)>(table[6])(web,L"<!doctype html><button id='b' onclick='this.textContent=Number(this.textContent)+1'>0</button>")))return 8;
 bool loaded=false;for(int i=0;i<100&&!loaded;++i){loaded=evaluate(L"document.getElementById('b') && document.getElementById('b').textContent",L"\"0\"");if(!loaded)Sleep(50);}if(!loaded||!evaluate(L"b.click();b.textContent",L"\"1\"")||!capture())return 9;
 auto original=controller;setblocked(true);if(retire_old&&!retire())return 10;
 bool offline=controller==original&&evaluate(L"b.click();b.textContent",L"\"2\"")&&capture();setblocked(false);
 bool online=controller==original&&evaluate(L"b.click();b.textContent",L"\"3\"")&&capture();
 std::printf("same_controller=%d offline_dom=%d online_dom=%d retired=%d\n",controller==original,offline,online,retire_old);std::fflush(stdout);
 if(!retire_old)reinterpret_cast<ControllerClose>((*reinterpret_cast<void***>(controller))[24])(controller);web->Release();controller->Release();DestroyWindow(window_handle);return offline&&online?0:11;
}
