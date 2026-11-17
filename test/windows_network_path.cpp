#include "../loader/windows/network_path.h"
#include <cassert>
int main(){
 wchar_t name[32768];DWORD n=GetModuleFileNameW(nullptr,name,32768);assert(n>0&&n<32768);
 std::wstring result;assert(windows_network::canonical_executable_path(name,result));assert(!result.empty()&&result.rfind(L"\\\\?\\",0)!=0);
 assert(!windows_network::canonical_executable_path(std::wstring(name)+L".absent",result));
 wchar_t directory[32768];assert(GetCurrentDirectoryW(32768,directory)>0);assert(!windows_network::canonical_executable_path(directory,result));
}
