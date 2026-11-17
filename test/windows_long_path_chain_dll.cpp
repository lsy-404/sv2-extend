#include <windows.h>
#include "../loader/windows/vendor/minhook/include/MinHook.h"
static decltype(&CreateFileW) original=nullptr;static int calls=0;
static HANDLE WINAPI interception(LPCWSTR name,DWORD access,DWORD share,LPSECURITY_ATTRIBUTES security,DWORD disposition,DWORD flags,HANDLE file){++calls;return original(name,access,share,security,disposition,flags,file);}
extern "C" __declspec(dllexport) int WINAPI install_fixture(){if(MH_Initialize()!=MH_OK)return 1;auto kernel=GetModuleHandleW(L"kernel32.dll");auto target=reinterpret_cast<void*>(GetProcAddress(kernel,"CreateFileW"));if(MH_CreateHook(target,reinterpret_cast<void*>(interception),reinterpret_cast<void**>(&original))!=MH_OK)return 2;if(MH_EnableHook(target)!=MH_OK)return 3;return 0;}
extern "C" __declspec(dllexport) int WINAPI fixture_calls(){return calls;}
BOOL WINAPI DllMain(HINSTANCE,DWORD,LPVOID){return TRUE;}
