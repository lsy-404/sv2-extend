#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shlobj.h>
#include <filesystem>
#include <fstream>
#include "../loader/windows/vendor/json.hpp"
#include "../sdk/include/loader_api.h"
using json=nlohmann::json;
int wmain(int argc,wchar_t **argv){
 if(argc==2&&std::wstring(argv[1])==L"--job-child")return 0;
 if(argc!=3)return 64;json checks;
 auto core=GetModuleHandleW(L"ExtendCore.dll");auto get=core?(const loader_api_v1*(*)())GetProcAddress(core,"ExtendGetAPI"):nullptr;auto api=get?get():nullptr;
 checks["residentCoreBeforeMain"]=api&&api->host_pid==(int)GetCurrentProcessId();if(!api)return 65;
 HANDLE token=nullptr;DWORD isContainer=1,tokenSize=0;checks["fullTrustToken"]=OpenProcessToken(GetCurrentProcess(),TOKEN_QUERY,&token)&&GetTokenInformation(token,TokenIsAppContainer,&isContainer,sizeof isContainer,&tokenSize)&&tokenSize==sizeof isContainer&&!isContainer;if(token)CloseHandle(token);
 wchar_t handleText[32]{};DWORD handleLength=GetEnvironmentVariableW(L"OPEN_SV2_MCP_PIPE_HANDLE",handleText,32);wchar_t *end=nullptr;auto handle=handleLength&&handleLength<32?reinterpret_cast<HANDLE>(static_cast<uintptr_t>(wcstoull(handleText,&end,10))):INVALID_HANDLE_VALUE;DWORD pipeFlags=0;
 checks["inheritedPipeServer"]=end&&!*end&&GetNamedPipeInfo(handle,&pipeFlags,nullptr,nullptr,nullptr)&&(pipeFlags&PIPE_SERVER_END);
 wchar_t jobText[32]{};DWORD jobLength=GetEnvironmentVariableW(L"OPEN_SV2_HOST_JOB_HANDLE",jobText,32);end=nullptr;auto job=jobLength&&jobLength<32?reinterpret_cast<HANDLE>(static_cast<uintptr_t>(wcstoull(jobText,&end,10))):nullptr;BOOL inJob=FALSE;JOBOBJECT_BASIC_ACCOUNTING_INFORMATION accounting{};
 checks["managedJobQueryable"]=end&&!*end&&IsProcessInJob(GetCurrentProcess(),job,&inJob)&&inJob&&QueryInformationJobObject(job,JobObjectBasicAccountingInformation,&accounting,sizeof accounting,nullptr)&&accounting.ActiveProcesses==1;
 JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits{};checks["managedJobNoBreakawayOrKill"]=QueryInformationJobObject(job,JobObjectExtendedLimitInformation,&limits,sizeof limits,nullptr)&&!(limits.BasicLimitInformation.LimitFlags&(JOB_OBJECT_LIMIT_BREAKAWAY_OK|JOB_OBJECT_LIMIT_SILENT_BREAKAWAY_OK|JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE));
 wchar_t executable[32768]{};DWORD executableLength=GetModuleFileNameW(nullptr,executable,32768);std::wstring command=L"\""+std::wstring(executable)+L"\" --job-child";STARTUPINFOW startup{};startup.cb=sizeof startup;PROCESS_INFORMATION child{};
 BOOL childCreated=executableLength&&executableLength<32768&&CreateProcessW(executable,command.data(),nullptr,nullptr,FALSE,CREATE_SUSPENDED|CREATE_NO_WINDOW,nullptr,nullptr,&startup,&child);BOOL childInJob=FALSE;
 checks["childInManagedJob"]=childCreated&&IsProcessInJob(child.hProcess,job,&childInJob)&&childInJob;
 if(childCreated){ResumeThread(child.hThread);WaitForSingleObject(child.hProcess,5000);CloseHandle(child.hThread);CloseHandle(child.hProcess);}
 PWSTR roaming=nullptr;HRESULT result=SHGetKnownFolderPath(FOLDERID_RoamingAppData,0,nullptr,&roaming);std::filesystem::path data=std::filesystem::u8path(api->host_data_directory());
 checks["roamingRedirected"]=SUCCEEDED(result)&&std::filesystem::path(roaming)==data/L"Roaming";if(roaming)CoTaskMemFree(roaming);
 auto dynamic=(decltype(&SHGetKnownFolderPath))GetProcAddress(GetModuleHandleW(L"shell32.dll"),"SHGetKnownFolderPath");roaming=nullptr;result=dynamic?dynamic(FOLDERID_RoamingAppData,0,nullptr,&roaming):E_FAIL;checks["dynamicFolderEntryRedirected"]=SUCCEEDED(result)&&std::filesystem::path(roaming)==data/L"Roaming";if(roaming)CoTaskMemFree(roaming);
 wchar_t legacy[MAX_PATH]{};auto dynamic_special=(decltype(&SHGetSpecialFolderPathW))GetProcAddress(GetModuleHandleW(L"shell32.dll"),"SHGetSpecialFolderPathW");checks["dynamicLegacyEntryRedirected"]=dynamic_special&&dynamic_special(nullptr,legacy,CSIDL_APPDATA,FALSE)&&std::filesystem::path(legacy)==data/L"Roaming";
 PWSTR common=nullptr;result=SHGetKnownFolderPath(FOLDERID_ProgramData,0,nullptr,&common);checks["programDataRedirected"]=SUCCEEDED(result)&&common&&std::filesystem::path(common)==data/L"ProgramData";if(common)CoTaskMemFree(common);
 common=nullptr;result=dynamic?dynamic(FOLDERID_ProgramData,0,nullptr,&common):E_FAIL;checks["dynamicProgramDataRedirected"]=SUCCEEDED(result)&&common&&std::filesystem::path(common)==data/L"ProgramData";if(common)CoTaskMemFree(common);
 wchar_t commonLegacy[MAX_PATH]{};checks["programDataLegacyRedirected"]=dynamic_special&&dynamic_special(nullptr,commonLegacy,CSIDL_COMMON_APPDATA,FALSE)&&std::filesystem::path(commonLegacy)==data/L"ProgramData";
 wchar_t commonFolder[MAX_PATH]{};auto dynamic_folder=(decltype(&SHGetFolderPathW))GetProcAddress(GetModuleHandleW(L"shell32.dll"),"SHGetFolderPathW");checks["programDataFolderRedirected"]=dynamic_folder&&SUCCEEDED(dynamic_folder(nullptr,CSIDL_COMMON_APPDATA,nullptr,0,commonFolder))&&std::filesystem::path(commonFolder)==data/L"ProgramData";
 HANDLE outside=CreateFileW(argv[2],GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,0,nullptr);checks["outsideFixtureReadable"]=outside!=INVALID_HANDLE_VALUE;if(outside!=INVALID_HANDLE_VALUE)CloseHandle(outside);
 auto target=data/L"Roaming"/L"fixture-write.txt";HANDLE allowed=CreateFileW(target.c_str(),GENERIC_WRITE,0,nullptr,CREATE_ALWAYS,0,nullptr);checks["profileWriteAllowed"]=allowed!=INVALID_HANDLE_VALUE;if(allowed!=INVALID_HANDLE_VALUE)CloseHandle(allowed);
 bool passed=true;for(auto &value:checks)passed=passed&&value.get<bool>();std::ofstream output{std::filesystem::path(argv[1])};output<<json{{"passed",passed},{"checks",checks},{"hostPid",GetCurrentProcessId()}}.dump(2);output.close();Sleep(3000);return passed?0:1;
}
