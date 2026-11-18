#include <windows.h>
struct Completion: IUnknown {virtual HRESULT STDMETHODCALLTYPE Invoke(HRESULT,IUnknown*)=0;};
static int calls=0,closed=0;static wchar_t saved_folder[128]{};static IUnknown *saved_options=nullptr;static Completion *pending_env=nullptr,*pending_controller=nullptr;static bool delay_controller=false;
struct Object{void **table;ULONG refs=1;};
static HRESULT WINAPI query(Object*,REFIID,void **out){if(!out)return E_POINTER;*out=nullptr;return E_NOINTERFACE;}
static ULONG WINAPI addref(Object *o){return ++o->refs;}
static ULONG WINAPI release(Object *o){return --o->refs;}
static HRESULT WINAPI close_controller(Object*){++closed;return S_OK;}
static void *controller_table[25];static Object controller{controller_table};
static __attribute__((noinline)) HRESULT WINAPI create_controller(Object*,HWND,Completion *handler){volatile int status=1;if(!handler||!status)return E_INVALIDARG;if(delay_controller){pending_controller=handler;handler->AddRef();return S_OK;}return handler->Invoke(S_OK,reinterpret_cast<IUnknown*>(&controller));}
static void *environment_table[]={reinterpret_cast<void*>(query),reinterpret_cast<void*>(addref),reinterpret_cast<void*>(release),reinterpret_cast<void*>(create_controller)};
static Object environment{environment_table};
extern "C" __declspec(dllexport) HRESULT WINAPI CreateWebViewEnvironmentWithOptionsInternal(bool,int type,PCWSTR folder,IUnknown *options,IUnknown *handler){++calls;saved_options=options;if(folder)lstrcpynW(saved_folder,folder,128);else saved_folder[0]=0;delay_controller=type==2;if(type==3)return E_FAIL;if(handler&&type==1){pending_env=reinterpret_cast<Completion*>(handler);pending_env->AddRef();return S_OK;}if(handler)return reinterpret_cast<Completion*>(handler)->Invoke(S_OK,reinterpret_cast<IUnknown*>(&environment));return S_OK;}
extern "C" __declspec(dllexport) HRESULT WINAPI fixture_complete_env(){if(!pending_env)return E_UNEXPECTED;auto value=pending_env;pending_env=nullptr;auto result=value->Invoke(S_OK,reinterpret_cast<IUnknown*>(&environment));value->Release();return result;}
extern "C" __declspec(dllexport) HRESULT WINAPI fixture_complete_controller(){if(!pending_controller)return E_UNEXPECTED;auto value=pending_controller;pending_controller=nullptr;auto result=value->Invoke(S_OK,reinterpret_cast<IUnknown*>(&controller));value->Release();return result;}
extern "C" __declspec(dllexport) BOOL WINAPI fixture_arguments(PCWSTR folder,IUnknown *options){return saved_options==options&&lstrcmpW(saved_folder,folder)==0;}
extern "C" __declspec(dllexport) ULONG WINAPI fixture_environment_refs(){return environment.refs;}
extern "C" __declspec(dllexport) int WINAPI fixture_calls(){return calls;}
extern "C" __declspec(dllexport) int WINAPI fixture_closed(){return closed;}
BOOL WINAPI DllMain(HINSTANCE,DWORD reason,LPVOID){if(reason==DLL_PROCESS_ATTACH){controller_table[0]=reinterpret_cast<void*>(query);controller_table[1]=reinterpret_cast<void*>(addref);controller_table[2]=reinterpret_cast<void*>(release);controller_table[24]=reinterpret_cast<void*>(close_controller);}return TRUE;}
