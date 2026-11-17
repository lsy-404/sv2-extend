#define wmain network_helper_entry
#include "../loader/windows/network_helper.cpp"
#undef wmain
#include <cassert>
int main(){
 assert(valid_target(L"C:\\profiles\\owned\\runtime\\Synthesizer V Studio 2 Pro\\synthv-studio.exe"));
 assert(!valid_target(L"runtime\\Synthesizer V Studio 2 Pro\\synthv-studio.exe"));
 assert(!valid_target(L"C:\\profiles\\runtime\\Synthesizer V Studio 2 Pro\\other.exe"));
 assert(!valid_target(L"C:\\profiles\\other\\Synthesizer V Studio 2 Pro\\synthv-studio.exe"));
 assert(!valid_target(L"C:\\profiles\\runtime\\other\\..\\Synthesizer V Studio 2 Pro\\synthv-studio.exe"));
 wchar_t a[]=L"helper",b[]=L"--other",c[]=L"--allow";wchar_t* args[]={a,b,c};
 assert(network_helper_entry(1,args)==2);assert(network_helper_entry(2,args)==2);assert(network_helper_entry(3,args)==2);
}
