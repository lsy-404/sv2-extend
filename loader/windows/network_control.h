#pragma once
#include <windows.h>
#include <taskschd.h>
#include <netfw.h>
#include "network_firewall.h"
#include <string>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <chrono>
#include <cstdint>

namespace windows_network {
inline constexpr GUID task_class={0x0f87369f,0xa4e5,0x4cfc,{0xbd,0x3e,0x73,0xe6,0x15,0x45,0x72,0xdd}};
inline constexpr GUID task_interface={0x2faba4c7,0x4da9,0x4013,{0x96,0x97,0x20,0xcc,0x3f,0xd4,0x0f,0x85}};
inline constexpr GUID firewall_class={0xe2b3c97f,0x6ae1,0x41ac,{0x81,0x7a,0xf6,0xf9,0x21,0x66,0xd7,0xdd}};
inline constexpr GUID firewall_interface={0x98325047,0xc671,0x4174,{0x8d,0x81,0xde,0xfc,0xd3,0xf0,0x31,0x86}};
template<class T> struct ComPtr { T* p=nullptr; ~ComPtr(){if(p)p->Release();} T** out(){return &p;} T* operator->()const{return p;} };
struct Bstr { BSTR p; explicit Bstr(const wchar_t *s):p(SysAllocString(s)){} ~Bstr(){SysFreeString(p);} };
struct State {
 uint64_t generation=0;
 bool requested_blocked=false;
 bool applied_blocked=false;
 bool policy_applied=false;
 bool verified=false;
 bool pending=false;
 bool host_connections_drained=false;
 bool child_scope=false;
 int32_t error=0;
};
class Controller {
 std::mutex mutex_;
 State value_;
 std::thread worker_;
 std::condition_variable wake_;
 bool stopping_=false,monitor_ready_=false;
 HRESULT monitor_error_=S_OK;
 std::wstring target_;
 std::thread monitor_;
 static HRESULT run(bool block) {
  ComPtr<ITaskService> service;
  HRESULT hr=CoCreateInstance(task_class,nullptr,CLSCTX_INPROC_SERVER,task_interface,(void**)service.out()); if(FAILED(hr))return hr;
  VARIANT empty; VariantInit(&empty); hr=service->Connect(empty,empty,empty,empty); if(FAILED(hr))return hr;
  ComPtr<ITaskFolder> folder; Bstr root(L"\\"); hr=service->GetFolder(root.p,folder.out());if(FAILED(hr))return hr;
  Bstr name(block?L"SV2 Extend Network Block":L"SV2 Extend Network Allow"); ComPtr<IRegisteredTask> task;
  hr=folder->GetTask(name.p,task.out());if(FAILED(hr))return hr;
  ComPtr<IRunningTask> running;hr=task->Run(empty,running.out());if(FAILED(hr))return hr;
  DWORD start=GetTickCount();
  for(;;){ TASK_STATE state;hr=running->get_State(&state);if(FAILED(hr))return hr;
   if(state==TASK_STATE_READY || state==TASK_STATE_DISABLED)break;
   if(GetTickCount()-start>35000)return HRESULT_FROM_WIN32(ERROR_TIMEOUT);
   Sleep(100);
  }
  LONG result=0;hr=task->get_LastTaskResult(&result);if(FAILED(hr))return hr;
  return result==0?S_OK:HRESULT_FROM_WIN32(result);
 }
 static HRESULT readback(bool block,const std::wstring& target){
  ComPtr<INetFwPolicy2> policy;HRESULT hr=CoCreateInstance(firewall_class,nullptr,CLSCTX_INPROC_SERVER,firewall_interface,(void**)policy.out());if(FAILED(hr))return hr;
  NET_FW_MODIFY_STATE modify;hr=policy->get_LocalPolicyModifyState(&modify);if(FAILED(hr))return hr;
  if(modify!=NET_FW_MODIFY_STATE_OK)return HRESULT_FROM_WIN32(ERROR_ACCESS_DISABLED_BY_POLICY);
  long active=0;hr=policy->get_CurrentProfileTypes(&active);if(FAILED(hr))return hr;if(!active)return HRESULT_FROM_WIN32(ERROR_NOT_READY);
  for(long profile : {long(NET_FW_PROFILE2_DOMAIN),long(NET_FW_PROFILE2_PRIVATE),long(NET_FW_PROFILE2_PUBLIC)})if(active&profile){VARIANT_BOOL enabled=VARIANT_FALSE;hr=policy->get_FirewallEnabled((NET_FW_PROFILE_TYPE2)profile,&enabled);if(FAILED(hr))return hr;if(enabled!=VARIANT_TRUE)return HRESULT_FROM_WIN32(ERROR_NOT_READY);}
  ComPtr<INetFwRule> rule;hr=windows_firewall::find(policy.p,target,rule.out());if(FAILED(hr))return hr;
  VARIANT_BOOL enabled=VARIANT_FALSE;hr=rule->get_Enabled(&enabled);if(FAILED(hr))return hr;
  return ((enabled==VARIANT_TRUE)==block)?S_OK:HRESULT_FROM_WIN32(ERROR_INVALID_DATA);
 }
 void monitor(){
  HRESULT initialized=CoInitializeEx(nullptr,COINIT_MULTITHREADED);
  std::unique_lock<std::mutex> lock(mutex_);monitor_ready_=true;monitor_error_=initialized;wake_.notify_all();if(FAILED(initialized))return;
  while(!stopping_){
   if(wake_.wait_for(lock,std::chrono::seconds(2),[this]{return stopping_;}))break;
   if(value_.pending||!value_.policy_applied)continue;
   auto generation=value_.generation;bool blocked=value_.applied_blocked;lock.unlock();HRESULT hr=readback(blocked,target_);lock.lock();
   if(FAILED(hr)&&!value_.pending&&value_.generation==generation){value_.policy_applied=false;value_.verified=false;value_.error=hr;}
  }
  lock.unlock();CoUninitialize();
 }
 void execute(uint64_t generation,bool block){
  HRESULT initialized=CoInitializeEx(nullptr,COINIT_MULTITHREADED);HRESULT result=initialized;
  if(SUCCEEDED(initialized)){result=run(block);if(SUCCEEDED(result))result=readback(block,target_);CoUninitialize();}
  std::lock_guard<std::mutex> lock(mutex_);
  if(value_.generation!=generation)return;
  value_.pending=false;value_.error=result;
  if(SUCCEEDED(result)){value_.policy_applied=true;value_.applied_blocked=block;}
 }
public:
 explicit Controller(std::wstring target):target_(std::move(target)),monitor_([this]{monitor();}){std::unique_lock<std::mutex> lock(mutex_);wake_.wait(lock,[this]{return monitor_ready_;});}
 Controller(const Controller&)=delete;Controller& operator=(const Controller&)=delete;
 ~Controller(){{std::lock_guard<std::mutex> lock(mutex_);stopping_=true;}wake_.notify_all();if(worker_.joinable())worker_.join();if(monitor_.joinable())monitor_.join();}
 bool adopt(bool blocked){
  std::unique_lock<std::mutex> lock(mutex_);if(value_.pending)return false;if(FAILED(monitor_error_)){value_.error=monitor_error_;return false;}if(worker_.joinable())worker_.join();
  auto generation=++value_.generation;value_.requested_blocked=blocked;value_.pending=true;value_.policy_applied=false;value_.verified=false;lock.unlock();
  HRESULT initialized=CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);bool changed=initialized==RPC_E_CHANGED_MODE;
  HRESULT result=SUCCEEDED(initialized)||changed?readback(blocked,target_):initialized;if(SUCCEEDED(initialized))CoUninitialize();
  lock.lock();if(value_.generation!=generation)return false;value_.pending=false;value_.error=result;
  if(SUCCEEDED(result)){value_.policy_applied=true;value_.applied_blocked=blocked;}return SUCCEEDED(result);
 }
 bool request(bool blocked){
  std::unique_lock<std::mutex> lock(mutex_);if(value_.pending)return false;if(FAILED(monitor_error_)){value_.error=monitor_error_;return false;}
  if(worker_.joinable())worker_.join();
  value_.generation++;value_.requested_blocked=blocked;value_.pending=true;value_.policy_applied=false;
  value_.host_connections_drained=false;value_.child_scope=false;value_.error=0;
  auto generation=value_.generation;
  try {worker_=std::thread([this,generation,blocked]{execute(generation,blocked);});}
  catch(...){value_.pending=false;value_.error=E_OUTOFMEMORY;return false;}
  return true;
 }
 State state(){std::lock_guard<std::mutex> lock(mutex_);return value_;}
};
}
