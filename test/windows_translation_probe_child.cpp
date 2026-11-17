#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shlobj.h>
#include <cstdio>
#include <cwchar>
#include <string>
#include "../loader/windows/data_namespace.h"
int wmain(){
 wchar_t root[32768],common[MAX_PATH]{};DWORD n=GetEnvironmentVariableW(L"OPEN_SV2_LOADER_DATA_DIR",root,32768);if(!n||n>32000)return 2;std::wstring data=std::wstring(root)+L"\\host-data";if(!data_namespace::install(data))return 4;PWSTR known=nullptr;bool knownOK=SUCCEEDED(SHGetKnownFolderPath(FOLDERID_ProgramData,0,nullptr,&known))&&std::wstring(known)==data+L"\\ProgramData";if(known)CoTaskMemFree(known);wchar_t folder[MAX_PATH]{};bool folderOK=SUCCEEDED(SHGetFolderPathW(nullptr,CSIDL_COMMON_APPDATA,nullptr,0,folder))&&std::wstring(folder)==data+L"\\ProgramData";
 bool resolved=SHGetSpecialFolderPathW(nullptr,common,CSIDL_COMMON_APPDATA,FALSE)!=FALSE;DWORD resolveError=resolved?0:GetLastError();if(!resolved)wcscpy(common,L"C:\\ProgramData");wchar_t target[MAX_PATH];swprintf(target,MAX_PATH,L"%ls\\Dreamtonics\\Synthesizer V Studio 2\\translations\\*.txt",common);WIN32_FIND_DATAW entry{};HANDLE find=FindFirstFileW(target,&entry);DWORD err=find==INVALID_HANDLE_VALUE?GetLastError():0;int count=0;if(find!=INVALID_HANDLE_VALUE){do{if(!(entry.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY))++count;}while(FindNextFileW(find,&entry));FindClose(find);}swprintf(target,MAX_PATH,L"%ls\\Dreamtonics\\Synthesizer V Studio 2\\translations\\zh-cn.txt",common);HANDLE f=CreateFileW(target,GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,0,nullptr);bool open=f!=INVALID_HANDLE_VALUE;DWORD openerr=open?0:GetLastError();bool chineseContent=false;if(open){char bytes[256]{};DWORD got=0;if(ReadFile(f,bytes,sizeof bytes,&got,nullptr)&&got>10){for(DWORD i=0;i<got;i++)if(static_cast<unsigned char>(bytes[i])>=0x80)chineseContent=true;}CloseHandle(f);}wcscat(root,L"\\outputs\\translation-probe.txt");FILE *out=_wfopen(root,L"w");if(!out)return 3;fprintf(out,"known=%d folder=%d resolved=%d resolveError=%lu directCount=%d findError=%lu chineseReadable=%d chineseContent=%d openError=%lu\n",knownOK,folderOK,resolved,resolveError,count,err,open,chineseContent,openerr);fclose(out);return 0;
}
