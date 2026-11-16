#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shlobj.h>
int wmain(int argc,wchar_t**argv){
 if(argc!=2||!argv[1])return 64;
 wchar_t raw[64];if(!GetEnvironmentVariableW(L"SV2_TEST_MAIN_HANDLE",raw,64))return 65;HANDLE marker=reinterpret_cast<HANDLE>(static_cast<uintptr_t>(_wcstoui64(raw,nullptr,10)));
 DWORD written=0;const char text[]="main entered";BOOL ok=WriteFile(marker,text,sizeof(text)-1,&written,nullptr);FlushFileBuffers(marker);CloseHandle(marker);
 PWSTR folder=nullptr;SHGetKnownFolderPath(FOLDERID_LocalAppData,0,nullptr,&folder);if(folder)CoTaskMemFree(folder);Sleep(1000);return ok&&written==sizeof(text)-1?0:66;
}
