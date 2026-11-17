#include "../loader/windows/windows_child_gate.h"
#include <cassert>
int wmain(int argc,wchar_t**){
 if(argc>1)return 0;
 assert(MH_Initialize()==MH_OK);assert(windows_child_gate::install());assert(windows_child_gate::state().blocked);
 wchar_t module[32768];assert(GetModuleFileNameW(nullptr,module,32768));std::wstring physical;assert(windows_network::canonical_executable_path(module,physical));
 STARTUPINFOW si{};si.cb=sizeof si;PROCESS_INFORMATION pi{};std::wstring cmd=L"\""+physical+L"\" child";
 assert(!CreateProcessW(physical.c_str(),cmd.data(),nullptr,nullptr,FALSE,0,nullptr,nullptr,&si,&pi));assert(GetLastError()==ERROR_ACCESS_DENIED);
 assert(!windows_child_gate::register_helper(L"relative.exe"));assert(windows_child_gate::register_helper(physical));
 cmd=L"\""+physical+L"\" child";assert(!CreateProcessW(nullptr,cmd.data(),nullptr,nullptr,FALSE,0,nullptr,nullptr,&si,&pi));assert(GetLastError()==ERROR_ACCESS_DENIED);
 auto shell=ShellExecuteW(nullptr,L"open",L"unregistered",nullptr,nullptr,SW_HIDE);assert(reinterpret_cast<INT_PTR>(shell)==SE_ERR_ACCESSDENIED);
 cmd=L"\""+physical+L"\" child";assert(CreateProcessW(physical.c_str(),cmd.data(),nullptr,nullptr,FALSE,0,nullptr,nullptr,&si,&pi));
 assert(WaitForSingleObject(pi.hProcess,10000)==WAIT_OBJECT_0);DWORD exit=1;assert(GetExitCodeProcess(pi.hProcess,&exit)&&exit==0);CloseHandle(pi.hThread);CloseHandle(pi.hProcess);
 auto state=windows_child_gate::state();assert(state.helpers==1&&state.denied==3);windows_child_gate::setblocked(false);assert(!windows_child_gate::state().blocked);return 0;
}
