#include "../loader/windows/network_bootstrap.h"
#include <cassert>
#include <string>
using namespace windows_network_bootstrap;
int wmain(int argc,wchar_t **argv){
 if(argc==3){auto raw=_wcstoui64(argv[2],nullptr,10);HANDLE handle=reinterpret_cast<HANDLE>(static_cast<uintptr_t>(raw));Record result{};if(!read(handle,result)||result.generation!=47||result.requested!=1||result.applied!=1||result.verified!=1)return 1;void *write=MapViewOfFile(handle,FILE_MAP_WRITE,0,0,sizeof(Record));if(write){UnmapViewOfFile(write);return 2;}return 0;}
 HANDLE scratch=CreateFileMappingW(INVALID_HANDLE_VALUE,nullptr,PAGE_READWRITE,0,sizeof(Record),nullptr);assert(scratch);auto view=static_cast<Record*>(MapViewOfFile(scratch,FILE_MAP_WRITE,0,0,sizeof(Record)));assert(view);
 uint32_t self=0;uint64_t creation=0;std::wstring selfImage;assert(process_identity(GetCurrentProcess(),self,creation,selfImage));Record sample{};sample.magic=record_magic;sample.version=1;sample.size=sizeof sample;sample.pid=self;sample.creation=creation;sample.requested=sample.applied=sample.verified=1;std::memcpy(sample.image,selfImage.c_str(),(selfImage.size()+1)*sizeof(wchar_t));std::memcpy(view,&sample,sizeof sample);
 Record checked{};assert(read(scratch,checked));view->creation++;assert(!read(scratch,checked));view->creation=creation;view->image[0]=L'X';assert(!read(scratch,checked));UnmapViewOfFile(view);CloseHandle(scratch);
 Handoff handoff;assert(handoff.create());assert(!handoff.create());HANDLE passed=handoff.read_handle();assert(passed);
 SIZE_T size=0;InitializeProcThreadAttributeList(nullptr,1,0,&size);auto attrs=static_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(HeapAlloc(GetProcessHeap(),0,size));assert(attrs&&InitializeProcThreadAttributeList(attrs,1,0,&size));assert(UpdateProcThreadAttribute(attrs,0,PROC_THREAD_ATTRIBUTE_HANDLE_LIST,&passed,sizeof passed,nullptr,nullptr));
 wchar_t module[32768];assert(GetModuleFileNameW(nullptr,module,32768));std::wstring physical;assert(windows_network::canonical_executable_path(module,physical));std::wstring command=L"\""+physical+L"\" child "+std::to_wstring(reinterpret_cast<uintptr_t>(passed));
 STARTUPINFOEXW si{};si.StartupInfo.cb=sizeof si;si.lpAttributeList=attrs;PROCESS_INFORMATION pi{};assert(CreateProcessW(physical.c_str(),command.data(),nullptr,nullptr,TRUE,CREATE_SUSPENDED|EXTENDED_STARTUPINFO_PRESENT,nullptr,nullptr,&si.StartupInfo,&pi));
 assert(!handoff.publish(pi.hProcess,physical+L".wrong",47,true,true,true));assert(!handoff.publish(pi.hProcess,physical,47,true,true,false));assert(!handoff.publish(pi.hProcess,physical,47,true,false,true));assert(handoff.publish(pi.hProcess,physical,47,true,true,true));
 Record parent{};assert(!read(passed,parent));assert(handoff.seal());assert(!handoff.seal());assert(!handoff.publish(pi.hProcess,physical,47,true,true,true));
 assert(ResumeThread(pi.hThread)!=DWORD(-1));assert(WaitForSingleObject(pi.hProcess,10000)==WAIT_OBJECT_0);DWORD exit=1;assert(GetExitCodeProcess(pi.hProcess,&exit)&&exit==0);
 CloseHandle(pi.hThread);CloseHandle(pi.hProcess);DeleteProcThreadAttributeList(attrs);HeapFree(GetProcessHeap(),0,attrs);return 0;
}
