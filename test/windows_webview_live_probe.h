#pragma once
#include <windows.h>
#include <atomic>
#include <memory>
#include <string>
#include <new>
#include "../loader/windows/windows_webview_gate.h"
namespace windows_webview_live_probe {
struct State {bool pending=false,environmentCompleted=false,controllerCompleted=false,controllerCreated=false,windowExists=false;int32_t factoryResult=0,error=0;};
struct Shared {
 DWORD thread=0;HWND window=nullptr;std::atomic<bool> pending{false},environmentCompleted{false},controllerCompleted{false},controllerCreated{false};std::atomic<int32_t> factoryResult{0},error{0};
};
struct Handler final:windows_webview_gate::Completion {
 std::atomic<ULONG> refs{1};std::shared_ptr<Shared> shared;bool environment;
 Handler(std::shared_ptr<Shared> value,bool env):shared(std::move(value)),environment(env){}
 HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid,void **out)override{
  if(!out)return E_POINTER;*out=nullptr;if(IsEqualIID(iid,IID_IUnknown)||IsEqualIID(iid,environment?windows_webview_gate::env_handler_iid:windows_webview_gate::controller_handler_iid)){*out=static_cast<windows_webview_gate::Completion*>(this);AddRef();return S_OK;}return E_NOINTERFACE;
 }
 ULONG STDMETHODCALLTYPE AddRef()override{return ++refs;}
 ULONG STDMETHODCALLTYPE Release()override{auto value=--refs;if(!value)delete this;return value;}
 HRESULT STDMETHODCALLTYPE Invoke(HRESULT result,IUnknown *object)override{
  if(GetCurrentThreadId()!=shared->thread){shared->error=RPC_E_WRONG_THREAD;shared->pending=false;return S_OK;}
  if(environment){
   shared->environmentCompleted=true;if(FAILED(result)||!object){shared->error=FAILED(result)?result:E_UNEXPECTED;shared->pending=false;return S_OK;}
   auto table=*reinterpret_cast<void***>(object);if(!table||!table[3]){shared->error=E_UNEXPECTED;shared->pending=false;return S_OK;}
   Handler *next=new(std::nothrow) Handler(shared,false);if(!next){shared->error=E_OUTOFMEMORY;shared->pending=false;return S_OK;}
   HRESULT create=reinterpret_cast<windows_webview_gate::ControllerCreate>(table[3])(object,shared->window,next);next->Release();
   if(FAILED(create)&&!shared->controllerCompleted.load()){shared->error=create;shared->pending=false;}return S_OK;
  }
  shared->controllerCompleted=true;shared->controllerCreated=SUCCEEDED(result)&&object;if(FAILED(result)||!object)shared->error=FAILED(result)?result:E_UNEXPECTED;shared->pending=false;return S_OK;
 }
};
inline bool empty_directory(const std::wstring &path){
 if(path.empty()||path.size()>=32700)return false;wchar_t full[32768];DWORD n=GetFullPathNameW(path.c_str(),32768,full,nullptr);if(!n||n>=32768||CompareStringOrdinal(path.c_str(),-1,full,-1,TRUE)!=CSTR_EQUAL)return false;
 DWORD attributes=GetFileAttributesW(path.c_str());if(attributes==INVALID_FILE_ATTRIBUTES||!(attributes&FILE_ATTRIBUTE_DIRECTORY)||(attributes&FILE_ATTRIBUTE_REPARSE_POINT))return false;
 WIN32_FIND_DATAW item{};HANDLE find=FindFirstFileW((path+L"\\*").c_str(),&item);if(find==INVALID_HANDLE_VALUE)return false;bool empty=true;do{if(wcscmp(item.cFileName,L".")&&wcscmp(item.cFileName,L"..")){empty=false;break;}}while(FindNextFileW(find,&item));FindClose(find);return empty;
}
class Probe {
 std::shared_ptr<Shared> shared_;
public:
 explicit Probe(DWORD owner):shared_(std::make_shared<Shared>()){shared_->thread=owner;}
 HRESULT start(const std::wstring &directory){
  if(GetCurrentThreadId()!=shared_->thread)return RPC_E_WRONG_THREAD;auto gate=windows_webview_gate::state();if(!gate.installed||!gate.factoryBound||gate.blocked)return HRESULT_FROM_WIN32(ERROR_INVALID_STATE);
  if(shared_->pending||shared_->window||!empty_directory(directory))return E_INVALIDARG;
  HWND window=CreateWindowExW(0,L"STATIC",L"",WS_OVERLAPPED,0,0,1,1,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);if(!window)return HRESULT_FROM_WIN32(GetLastError());shared_->window=window;
  shared_->pending=true;shared_->environmentCompleted=false;shared_->controllerCompleted=false;shared_->controllerCreated=false;shared_->factoryResult=0;shared_->error=0;
  Handler *handler=new(std::nothrow) Handler(shared_,true);if(!handler){shared_->pending=false;shared_->error=E_OUTOFMEMORY;return E_OUTOFMEMORY;}
  HRESULT result=windows_webview_gate::factory(false,0,directory.c_str(),nullptr,handler);handler->Release();shared_->factoryResult=result;
  if(FAILED(result)&&!shared_->environmentCompleted.load()){shared_->error=result;shared_->pending=false;}return result;
 }
 State state()const{return {shared_->pending.load(),shared_->environmentCompleted.load(),shared_->controllerCompleted.load(),shared_->controllerCreated.load(),shared_->window&&IsWindow(shared_->window),shared_->factoryResult.load(),shared_->error.load()};}
 bool close(){
  if(GetCurrentThreadId()!=shared_->thread||shared_->pending)return false;auto gate=windows_webview_gate::state();if(gate.controllers||gate.pendingEnvironment||gate.pendingController)return false;
  if(shared_->window&&!DestroyWindow(shared_->window))return false;shared_->window=nullptr;return true;
 }
};
}
