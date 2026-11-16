#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shlobj.h>
#include <filesystem>
#include <fstream>
#include <string>
int wmain(int argc,wchar_t**argv){
 if(argc!=3)return 64;PWSTR folder=nullptr;SHGetKnownFolderPath(FOLDERID_LocalAppData,0,nullptr,&folder);if(folder)CoTaskMemFree(folder);HANDLE pipe=reinterpret_cast<HANDLE>(static_cast<uintptr_t>(_wcstoui64(argv[2],nullptr,10)));DWORD flags=0;bool valid=GetFileType(pipe)==FILE_TYPE_PIPE&&GetNamedPipeInfo(pipe,&flags,nullptr,nullptr,nullptr)&&(flags&PIPE_SERVER_END);
 {std::ofstream out{std::filesystem::path(argv[1])};out<<(valid?1:0);}if(!valid)return 65;BOOL connected=ConnectNamedPipe(pipe,nullptr);if(!connected&&GetLastError()!=ERROR_PIPE_CONNECTED)return 66;char bytes[5];DWORD count=0,wrote=0;if(!ReadFile(pipe,bytes,sizeof bytes,&count,nullptr)||!WriteFile(pipe,bytes,count,&wrote,nullptr))return 67;FlushFileBuffers(pipe);CloseHandle(pipe);return 0;
}
