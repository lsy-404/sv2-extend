#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <aclapi.h>
#include <iostream>
#include <string>
#include <vector>
#include "protocol.h"
bool current_user(HANDLE token,std::vector<unsigned char>& data){DWORD n=0;GetTokenInformation(token,TokenUser,nullptr,0,&n);if(!n)return false;data.resize(n);return GetTokenInformation(token,TokenUser,data.data(),n,&n)!=FALSE;}
bool endpoint(const wchar_t* file,std::wstring& pipe){
 HANDLE f=CreateFileW(file,GENERIC_READ|READ_CONTROL,FILE_SHARE_READ,nullptr,OPEN_EXISTING,FILE_FLAG_OPEN_REPARSE_POINT,nullptr);if(f==INVALID_HANDLE_VALUE)return false;
 BY_HANDLE_FILE_INFORMATION info{};PSID owner=nullptr;PACL dacl=nullptr;PSECURITY_DESCRIPTOR descriptor=nullptr;HANDLE self=nullptr;std::vector<unsigned char> own;bool valid=GetFileInformationByHandle(f,&info)&&!(info.dwFileAttributes&FILE_ATTRIBUTE_REPARSE_POINT)&&OpenProcessToken(GetCurrentProcess(),TOKEN_QUERY,&self)&&current_user(self,own)&&GetSecurityInfo(f,SE_FILE_OBJECT,OWNER_SECURITY_INFORMATION|DACL_SECURITY_INFORMATION,&owner,nullptr,&dacl,nullptr,&descriptor)==ERROR_SUCCESS&&owner&&dacl&&EqualSid(owner,reinterpret_cast<TOKEN_USER*>(own.data())->User.Sid);
 std::vector<std::vector<unsigned char>> allowed;if(valid){for(DWORD i=0;i<dacl->AceCount;i++){void* raw=nullptr;if(!GetAce(dacl,i,&raw)){valid=false;break;}auto header=static_cast<ACE_HEADER*>(raw);if(header->AceType==ACCESS_ALLOWED_ACE_TYPE){auto ace=static_cast<ACCESS_ALLOWED_ACE*>(raw);PSID sid=&ace->SidStart;if(!IsValidSid(sid)){valid=false;break;}DWORD size=GetLengthSid(sid);allowed.emplace_back(size);if(!CopySid(size,allowed.back().data(),sid)){valid=false;break;}}else if(header->AceType!=ACCESS_DENIED_ACE_TYPE){valid=false;break;}}}char buffer[8192];DWORD got=0;if(valid)valid=ReadFile(f,buffer,sizeof buffer,&got,nullptr)&&got>0&&got<sizeof buffer;CloseHandle(f);if(descriptor)LocalFree(descriptor);if(self)CloseHandle(self);if(!valid)return false;
 auto value=windows_mcp::json::parse(buffer,buffer+got,nullptr,false);if(!value.is_object()||value.value("schema",0)!=1||value.value("transport","")!="named-pipe"||!value.contains("pipeName")||!value["pipeName"].is_string()||!value.contains("hostPid")||!value["hostPid"].is_number_unsigned())return false;
 auto name=value["pipeName"].get<std::string>();int length=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,name.c_str(),-1,nullptr,0);if(length<2||length>512)return false;pipe.resize(length);if(!MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,name.c_str(),-1,pipe.data(),length))return false;pipe.pop_back();if(pipe.rfind(L"\\\\.\\pipe\\SV2Extend.MCP.",0)!=0)return false;
 auto id=value["hostPid"].get<unsigned long>();HANDLE process=OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION,FALSE,id),host=nullptr;std::vector<unsigned char> other;valid=process&&OpenProcessToken(process,TOKEN_QUERY,&host)&&current_user(host,other)&&EqualSid(reinterpret_cast<TOKEN_USER*>(own.data())->User.Sid,reinterpret_cast<TOKEN_USER*>(other.data())->User.Sid);if(valid){DWORD n=0;GetTokenInformation(host,TokenAppContainerSid,nullptr,0,&n);std::vector<unsigned char> app(n);valid=n&&GetTokenInformation(host,TokenAppContainerSid,app.data(),n,&n);BYTE system[SECURITY_MAX_SID_SIZE];DWORD systemSize=sizeof system;valid=valid&&CreateWellKnownSid(WinLocalSystemSid,nullptr,system,&systemSize);if(valid){PSID appSid=reinterpret_cast<TOKEN_APPCONTAINER_INFORMATION*>(app.data())->TokenAppContainer;for(auto& sid:allowed)if(!EqualSid(sid.data(),reinterpret_cast<TOKEN_USER*>(own.data())->User.Sid)&&!EqualSid(sid.data(),appSid)&&!EqualSid(sid.data(),system)){valid=false;break;}}}if(host)CloseHandle(host);if(process)CloseHandle(process);return valid;
}
int wmain(int argc,wchar_t**argv){
 if(argc!=3)return 64;std::wstring name;if(std::wstring(argv[1])==L"--endpoint"){if(!endpoint(argv[2],name)){std::cerr<<"Invalid or stale MCP endpoint\n";return 1;}}else if(std::wstring(argv[1])==L"--pipe")name=argv[2];else return 64;
 HANDLE pipe=CreateFileW(name.c_str(),GENERIC_READ|GENERIC_WRITE,0,nullptr,OPEN_EXISTING,SECURITY_SQOS_PRESENT|SECURITY_IDENTIFICATION,nullptr);if(pipe==INVALID_HANDLE_VALUE){std::cerr<<"Cannot connect to resident MCP endpoint\n";return 1;}
 std::string line;while(std::getline(std::cin,line)){
  if(line.size()>1048576){CloseHandle(pipe);return 2;}auto parsed=windows_mcp::json::parse(line,nullptr,false);bool response=!parsed.is_object()||parsed.contains("id");line+='\n';DWORD written=0;if(!WriteFile(pipe,line.data(),DWORD(line.size()),&written,nullptr)||written!=line.size()){CloseHandle(pipe);return 1;}
  if(response){std::string body;char byte;DWORD count=0;while(ReadFile(pipe,&byte,1,&count,nullptr)&&count&&byte!='\n'){body+=byte;if(body.size()>1048576){CloseHandle(pipe);return 2;}}if(!count){CloseHandle(pipe);return 1;}std::cout<<body<<std::endl;}
 }
 CloseHandle(pipe);return 0;
}
