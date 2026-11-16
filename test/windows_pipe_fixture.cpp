#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <sddl.h>
#include <shlobj.h>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>
int wmain(int argc,wchar_t **argv){
 if(argc!=3)return 64;PWSTR folder=nullptr;SHGetKnownFolderPath(FOLDERID_LocalAppData,0,nullptr,&folder);if(folder)CoTaskMemFree(folder);
 HANDLE token=nullptr;if(!OpenProcessToken(GetCurrentProcess(),TOKEN_QUERY,&token))return 65;DWORD n=0;GetTokenInformation(token,TokenUser,nullptr,0,&n);std::vector<unsigned char> u(n);if(!GetTokenInformation(token,TokenUser,u.data(),n,&n))return 66;n=0;GetTokenInformation(token,TokenAppContainerSid,nullptr,0,&n);std::vector<unsigned char> a(n);if(!GetTokenInformation(token,TokenAppContainerSid,a.data(),n,&n))return 67;CloseHandle(token);
 LPWSTR user=nullptr,app=nullptr;if(!ConvertSidToStringSidW(reinterpret_cast<TOKEN_USER*>(u.data())->User.Sid,&user)||!ConvertSidToStringSidW(reinterpret_cast<TOKEN_APPCONTAINER_INFORMATION*>(a.data())->TokenAppContainer,&app))return 68;
 std::wstring sddl=L"D:(A;;GA;;;"+std::wstring(user)+L")(A;;GA;;;"+std::wstring(app)+L")S:(ML;;NW;;;LW)";LocalFree(user);LocalFree(app);PSECURITY_DESCRIPTOR sd=nullptr;if(!ConvertStringSecurityDescriptorToSecurityDescriptorW(sddl.c_str(),SDDL_REVISION_1,&sd,nullptr))return 69;SECURITY_ATTRIBUTES sa{sizeof sa,sd,FALSE};
 std::wstring name=L"\\\\.\\pipe\\LOCAL\\SV2Extend.MCP."+std::to_wstring(GetCurrentProcessId());HANDLE pipe=CreateNamedPipeW(name.c_str(),PIPE_ACCESS_DUPLEX|FILE_FLAG_FIRST_PIPE_INSTANCE,PIPE_TYPE_BYTE|PIPE_READMODE_BYTE|PIPE_WAIT|PIPE_REJECT_REMOTE_CLIENTS,1,4096,4096,0,&sa);DWORD error=GetLastError();LocalFree(sd);
 {std::ofstream out{std::filesystem::path(argv[1])};out<<GetCurrentProcessId()<<" "<<(pipe!=INVALID_HANDLE_VALUE)<<" "<<error;}
 if(pipe==INVALID_HANDLE_VALUE)return 70;wchar_t finalName[4096];DWORD finalLength=GetFinalPathNameByHandleW(pipe,finalName,4096,0);{std::wofstream out{std::filesystem::path(std::wstring(argv[1])+L".name")};out<<finalLength<<L" "<<(finalLength?finalName:L"");}BOOL ok=ConnectNamedPipe(pipe,nullptr);if(!ok&&GetLastError()!=ERROR_PIPE_CONNECTED)return 71;char buffer[32];DWORD len=0,written=0;if(!ReadFile(pipe,buffer,sizeof buffer,&len,nullptr))return 72;if(!WriteFile(pipe,buffer,len,&written,nullptr))return 73;FlushFileBuffers(pipe);DisconnectNamedPipe(pipe);CloseHandle(pipe);return 0;
}
