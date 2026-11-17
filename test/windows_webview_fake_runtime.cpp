#include <windows.h>
static int calls=0;
extern "C" __declspec(dllexport) HRESULT WINAPI CreateWebViewEnvironmentWithOptionsInternal(bool,int,PCWSTR,IUnknown*,IUnknown*){++calls;return S_OK;}
extern "C" __declspec(dllexport) int WINAPI fixture_calls(){return calls;}
BOOL WINAPI DllMain(HINSTANCE,DWORD,LPVOID){return TRUE;}
