#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shlobj.h>
#include <filesystem>
#include <fstream>
#include "../loader/windows/vendor/json.hpp"
#include "../sdk/include/loader_api.h"
using json=nlohmann::json;
int wmain(int argc,wchar_t **argv){
 if(argc!=3)return 64;json checks;
 auto core=GetModuleHandleW(L"ExtendCore.dll");auto get=core?(const loader_api_v1*(*)())GetProcAddress(core,"ExtendGetAPI"):nullptr;auto api=get?get():nullptr;
 checks["residentCoreBeforeMain"]=api&&api->host_pid==(int)GetCurrentProcessId();if(!api)return 65;
 PWSTR roaming=nullptr;HRESULT result=SHGetKnownFolderPath(FOLDERID_RoamingAppData,0,nullptr,&roaming);std::filesystem::path data=std::filesystem::u8path(api->host_data_directory());
 checks["roamingRedirected"]=SUCCEEDED(result)&&std::filesystem::path(roaming)==data/L"Roaming";if(roaming)CoTaskMemFree(roaming);
 auto dynamic=(decltype(&SHGetKnownFolderPath))GetProcAddress(GetModuleHandleW(L"shell32.dll"),"SHGetKnownFolderPath");roaming=nullptr;result=dynamic?dynamic(FOLDERID_RoamingAppData,0,nullptr,&roaming):E_FAIL;checks["dynamicFolderEntryRedirected"]=SUCCEEDED(result)&&std::filesystem::path(roaming)==data/L"Roaming";if(roaming)CoTaskMemFree(roaming);
 wchar_t legacy[MAX_PATH]{};auto dynamic_special=(decltype(&SHGetSpecialFolderPathW))GetProcAddress(GetModuleHandleW(L"shell32.dll"),"SHGetSpecialFolderPathW");checks["dynamicLegacyEntryRedirected"]=dynamic_special&&dynamic_special(nullptr,legacy,CSIDL_APPDATA,FALSE)&&std::filesystem::path(legacy)==data/L"Roaming";
 HANDLE denied=CreateFileW(argv[2],GENERIC_READ|GENERIC_WRITE,0,nullptr,OPEN_EXISTING,0,nullptr);checks["originalFileDenied"]=denied==INVALID_HANDLE_VALUE&&GetLastError()==ERROR_ACCESS_DENIED;if(denied!=INVALID_HANDLE_VALUE)CloseHandle(denied);
 auto target=data/L"Roaming"/L"fixture-write.txt";HANDLE allowed=CreateFileW(target.c_str(),GENERIC_WRITE,0,nullptr,CREATE_ALWAYS,0,nullptr);checks["profileWriteAllowed"]=allowed!=INVALID_HANDLE_VALUE;if(allowed!=INVALID_HANDLE_VALUE)CloseHandle(allowed);
 WNDCLASSW cls{};cls.lpfnWndProc=DefWindowProcW;cls.hInstance=GetModuleHandleW(nullptr);cls.lpszClassName=L"JUCE_ExtendNamespaceFixture";RegisterClassW(&cls);HWND window=CreateWindowW(cls.lpszClassName,L"Namespace fixture",WS_OVERLAPPEDWINDOW|WS_VISIBLE,0,0,800,600,nullptr,nullptr,cls.hInstance,nullptr);
 auto deadline=GetTickCount64()+1500;while(GetTickCount64()<deadline){MSG message;while(PeekMessageW(&message,nullptr,0,0,PM_REMOVE)){TranslateMessage(&message);DispatchMessageW(&message);}Sleep(10);}
 HWND button=FindWindowExW(window,nullptr,L"BUTTON",L"Ext");DWORD pid=0;if(button)GetWindowThreadProcessId(button,&pid);checks["nativePanelInHost"]=button&&pid==GetCurrentProcessId();
 bool passed=true;for(auto &value:checks)passed=passed&&value.get<bool>();std::ofstream output{std::filesystem::path(argv[1])};output<<json{{"passed",passed},{"checks",checks},{"hostPid",GetCurrentProcessId()}}.dump(2);output.close();DestroyWindow(window);return passed?0:1;
}
