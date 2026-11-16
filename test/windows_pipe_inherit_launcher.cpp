#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>
#include "../loader/windows/host_sandbox.h"
int wmain(int argc,wchar_t**argv){
 if(argc!=5)return 64;try {
 std::filesystem::path host=std::filesystem::canonical(argv[1]),profile=std::filesystem::absolute(argv[2]),core=std::filesystem::canonical(argv[3]);std::filesystem::create_directories(profile);
 host_sandbox::State state;state.prepare(host,profile,core,false);
 HANDLE token=nullptr;DWORD size=0;if(!OpenProcessToken(GetCurrentProcess(),TOKEN_QUERY,&token))return 65;GetTokenInformation(token,TokenUser,nullptr,0,&size);std::vector<unsigned char> user(size);if(!GetTokenInformation(token,TokenUser,user.data(),size,&size))return 66;CloseHandle(token);
 LPWSTR userSid=nullptr,appSid=nullptr;if(!ConvertSidToStringSidW(reinterpret_cast<TOKEN_USER*>(user.data())->User.Sid,&userSid)||!ConvertSidToStringSidW(state.sid,&appSid))return 67;
 std::wstring sddl=L"D:(A;;GA;;;"+std::wstring(userSid)+L")(A;;GA;;;"+std::wstring(appSid)+L")S:(ML;;NW;;;LW)";LocalFree(userSid);LocalFree(appSid);PSECURITY_DESCRIPTOR sd=nullptr;if(!ConvertStringSecurityDescriptorToSecurityDescriptorW(sddl.c_str(),SDDL_REVISION_1,&sd,nullptr))return 68;
 SECURITY_ATTRIBUTES sa{sizeof sa,sd,TRUE};std::wstring name=L"\\\\.\\pipe\\SV2Extend.MCP.canary."+std::to_wstring(GetCurrentProcessId());HANDLE pipe=CreateNamedPipeW(name.c_str(),PIPE_ACCESS_DUPLEX|FILE_FLAG_FIRST_PIPE_INSTANCE,PIPE_TYPE_BYTE|PIPE_READMODE_BYTE|PIPE_WAIT|PIPE_REJECT_REMOTE_CLIENTS,1,4096,4096,0,&sa);LocalFree(sd);if(pipe==INVALID_HANDLE_VALUE)return 69;
 DeleteProcThreadAttributeList(state.attributes);HeapFree(GetProcessHeap(),0,state.attributes);state.attributes=nullptr;SIZE_T attributeBytes=0;InitializeProcThreadAttributeList(nullptr,2,0,&attributeBytes);state.attributes=(LPPROC_THREAD_ATTRIBUTE_LIST)HeapAlloc(GetProcessHeap(),0,attributeBytes);if(!state.attributes||!InitializeProcThreadAttributeList(state.attributes,2,0,&attributeBytes))return 73;HANDLE handles[2]={state.lock,pipe};SECURITY_CAPABILITIES caps{};caps.AppContainerSid=state.sid;
 if(!UpdateProcThreadAttribute(state.attributes,0,PROC_THREAD_ATTRIBUTE_HANDLE_LIST,handles,sizeof handles,nullptr,nullptr)||!UpdateProcThreadAttribute(state.attributes,0,PROC_THREAD_ATTRIBUTE_SECURITY_CAPABILITIES,&caps,sizeof caps,nullptr,nullptr))return 70;
 SetEnvironmentVariableW(L"OPEN_SV2_TEST_CORE",core.c_str());SetEnvironmentVariableW(L"OPEN_SV2_LOADER_DATA_DIR",profile.c_str());SetEnvironmentVariableW(L"OPEN_SV2_MCP_PIPE_HANDLE",std::to_wstring(reinterpret_cast<uintptr_t>(pipe)).c_str());SetEnvironmentVariableW(L"OPEN_SV2_MCP_PIPE_NAME",name.c_str());std::wstring command=L"\""+state.host.wstring()+L"\" \""+std::wstring(argv[4])+L"\" "+std::to_wstring(reinterpret_cast<uintptr_t>(pipe));STARTUPINFOEXW start{};start.StartupInfo.cb=sizeof start;start.lpAttributeList=state.attributes;PROCESS_INFORMATION child{};
 if(!CreateProcessW(state.host.c_str(),command.data(),nullptr,nullptr,TRUE,EXTENDED_STARTUPINFO_PRESENT|CREATE_NO_WINDOW,nullptr,state.host.parent_path().c_str(),&start.StartupInfo,&child))return 71;
 std::wcout<<name<<L"\n"<<child.dwProcessId<<L"\n";CloseHandle(child.hThread);CloseHandle(child.hProcess);CloseHandle(pipe);return 0;
 }catch(...){return 72;}
}
