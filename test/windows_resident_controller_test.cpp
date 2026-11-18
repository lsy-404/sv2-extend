#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <string>
int wmain(int argc,wchar_t** argv){
 if(argc==3){
  HMODULE module=LoadLibraryW(argv[2]);if(!module)return 10;
  auto start=reinterpret_cast<int(*)()>(GetProcAddress(module,"start_controller"));if(!start||!start())return 11;
  if(!FreeLibrary(module))return 12;
  HMODULE pinned=nullptr;if(!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,reinterpret_cast<LPCWSTR>(start),&pinned)||pinned!=module)return 13;
  auto alive=reinterpret_cast<int(*)()>(GetProcAddress(pinned,"controller_alive"));if(!alive||!alive())return 14;
  ExitProcess(0);
 }
 if(argc!=2)return 15;
 wchar_t self[32768];DWORD n=GetModuleFileNameW(nullptr,self,32768);if(!n||n>=32768)return 16;
 std::wstring cmd=L"\""+std::wstring(self)+L"\" child \""+argv[1]+L"\"";STARTUPINFOW si{sizeof si};PROCESS_INFORMATION pi{};
 if(!CreateProcessW(self,cmd.data(),nullptr,nullptr,FALSE,0,nullptr,nullptr,&si,&pi))return 17;
 CloseHandle(pi.hThread);DWORD wait=WaitForSingleObject(pi.hProcess,10000);DWORD code=99;bool ok=wait==WAIT_OBJECT_0&&GetExitCodeProcess(pi.hProcess,&code)&&code==0;
 if(wait==WAIT_TIMEOUT){TerminateProcess(pi.hProcess,98);WaitForSingleObject(pi.hProcess,1000);}CloseHandle(pi.hProcess);return ok?0:18;
}
