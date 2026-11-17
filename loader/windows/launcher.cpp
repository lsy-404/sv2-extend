#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stddef.h>
#include <stdint.h>
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>
#include "bootstrap.h"
#include "early_entry_blob.h"
#include "managed_host.h"
#include "transient_layout.h"
#include "startup_policy.h"
#include "network_control.h"
namespace fs=std::filesystem;
struct Handle{HANDLE h=nullptr;~Handle(){if(h&&h!=INVALID_HANDLE_VALUE)CloseHandle(h);}};
static bool contains_path(const fs::path &parent,const fs::path &child){auto p=parent.begin(),c=child.begin();for(;p!=parent.end();++p,++c){if(c==child.end()||CompareStringOrdinal(p->c_str(),-1,c->c_str(),-1,TRUE)!=CSTR_EQUAL)return false;}return true;}
static std::wstring quoted(const std::wstring &s){std::wstring result=L"\"";unsigned slashes=0;for(wchar_t c:s){if(c==L'\\'){slashes++;continue;}if(c==L'\"')result.append(slashes*2+1,L'\\');else result.append(slashes,L'\\');slashes=0;result+=c;}result.append(slashes*2,L'\\');return result+L'\"';}
static bool full_trust(){Handle token;if(!OpenProcessToken(GetCurrentProcess(),TOKEN_QUERY,&token.h))return false;DWORD app=0,size=0;return GetTokenInformation(token.h,TokenIsAppContainer,&app,sizeof app,&size)&&size==sizeof app&&!app;}
static bool pe64(const fs::path &path){Handle file{CreateFileW(path.c_str(),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr)};if(file.h==INVALID_HANDLE_VALUE)return false;Handle map{CreateFileMappingW(file.h,nullptr,PAGE_READONLY,0,0,nullptr)};if(!map.h)return false;auto base=(const BYTE*)MapViewOfFile(map.h,FILE_MAP_READ,0,0,0);if(!base)return false;LARGE_INTEGER size{};GetFileSizeEx(file.h,&size);bool valid=false;if(size.QuadPart>0&&uint64_t(size.QuadPart)>sizeof(IMAGE_DOS_HEADER)){auto dos=(const IMAGE_DOS_HEADER*)base;if(dos->e_magic==IMAGE_DOS_SIGNATURE&&dos->e_lfanew>0&&uint64_t(dos->e_lfanew)+sizeof(IMAGE_NT_HEADERS64)<=uint64_t(size.QuadPart)){auto nt=(const IMAGE_NT_HEADERS64*)(base+dos->e_lfanew);valid=nt->Signature==IMAGE_NT_SIGNATURE&&nt->FileHeader.Machine==IMAGE_FILE_MACHINE_AMD64&&nt->OptionalHeader.Magic==IMAGE_NT_OPTIONAL_HDR64_MAGIC;}}UnmapViewOfFile(base);return valid;}
int wmain(int argc,wchar_t **argv){
 bool fixture=argc>=4&&std::wstring(argv[1])==L"--fixture";
 bool managed=argc>=4&&(std::wstring(argv[1])==L"--host"||std::wstring(argv[1])==L"--managed-fixture");
 if(managed&&!full_trust())return 77;
 if(!fixture&&!managed){std::wcerr<<L"ExtendLauncher --host <official-synthv-studio.exe> <profile-directory>\nExtendLauncher --fixture <test-host.exe> <profile-directory> [host arguments]\n";return 64;}
 try{
 fs::path host=fs::canonical(argv[2]),profile=fs::weakly_canonical(fs::absolute(argv[3]));if(managed&&(contains_path(host.parent_path(),profile)||contains_path(profile,host.parent_path())))return 78;fs::create_directories(profile);profile=fs::canonical(profile);
 wchar_t module[MAX_PATH*4];DWORD length=GetModuleFileNameW(nullptr,module,DWORD(std::size(module)));if(!length||length>=std::size(module))return 65;
 fs::path core=fs::canonical(fs::path(module).parent_path()/L"ExtendCore.dll");
 auto filename=host.filename().wstring();for(auto &c:filename)c=towlower(c);if(fixture&&(filename.find(L"synthv")!=std::wstring::npos||filename.find(L"synthesizer")!=std::wstring::npos))return 78;
 if(std::wstring(argv[1])==L"--host"&&filename!=L"synthv-studio.exe")return 78;
 managed_host::State host_state;
 if(managed){auto relative=profile.lexically_relative(host.parent_path());if(!relative.empty()&&*relative.begin()!=L"..")return 78;host_state.prepare(host,profile,core,std::wstring(argv[1])==L"--host");if(std::wstring(argv[1])==L"--host")transient_layout::retire(host_state.data/L"Roaming"/L"Dreamtonics"/L"Synthesizer V Studio 2"/L"settings"/L"settings.xml",profile);host=host_state.host;}
 if(!pe64(host)||!pe64(core)||core.wstring().size()>16000)return 65;
 windows_network::State startup_network_state;
 if(managed){
  if(!plugin_storage::install(profile))return 79;startup_policy::Service policy(GetCurrentThreadId());loader_startup_network_state_v1 state{LOADER_ABI_V1,sizeof state,0,0,0,0,0};
  if(policy.state(&state)!=LOADER_OK)return 79;
  windows_network::Controller network(host.wstring());
  bool needs_change=state.requested_blocked!=0;
  if(!needs_change&&!network.adopt(false))needs_change=network.adopt(true);
  if(needs_change){
   if(!network.request(state.requested_blocked!=0))return 79;
   const ULONGLONG end=GetTickCount64()+40000;windows_network::State value;
   do{value=network.state();if(!value.pending)break;Sleep(20);}while(GetTickCount64()<end);
   if(value.pending||value.error||!value.policy_applied||value.applied_blocked!=(state.requested_blocked!=0))return 79;
  }
  startup_network_state=network.state();
 }
 HMODULE image=LoadLibraryExW(core.c_str(),nullptr,DONT_RESOLVE_DLL_REFERENCES);if(!image)return 66;
 auto entry=GetProcAddress(image,"ExtendInitialize");uintptr_t rva=(uintptr_t)entry-(uintptr_t)image;FreeLibrary(image);if(!entry||rva>UINT32_MAX)return 66;
 // Environment inherited only by this child; launcher exits after initialization.
 SetEnvironmentVariableW(L"OPEN_SV2_LOADER_DATA_DIR",profile.c_str());SetEnvironmentVariableW(L"OPEN_SV2_WINDOWS_FIXTURE",fixture?L"1":nullptr);SetEnvironmentVariableW(L"OPEN_SV2_WINDOWS_MANAGED",managed?L"1":nullptr);
 SetEnvironmentVariableW(L"OPEN_SV2_NETWORK_BOOTSTRAP_HANDLE",managed?std::to_wstring((uintptr_t)host_state.network_handoff.read_handle()).c_str():nullptr);
 if(managed){SetEnvironmentVariableW(L"OPEN_SV2_HOST_DATA_DIR",host_state.data.c_str());SetEnvironmentVariableW(L"APPDATA",(host_state.data/L"Roaming").c_str());SetEnvironmentVariableW(L"LOCALAPPDATA",(host_state.data/L"Local").c_str());SetEnvironmentVariableW(L"TEMP",(host_state.data/L"Temp").c_str());SetEnvironmentVariableW(L"TMP",(host_state.data/L"Temp").c_str());SetEnvironmentVariableW(L"OPEN_SV2_HOST_JOB_HANDLE",std::to_wstring((uintptr_t)host_state.job_query).c_str());}
 std::wstring command=quoted(host.wstring());for(int i=4;i<argc;i++)command+=L" "+quoted(argv[i]);
 STARTUPINFOEXW startup{};startup.StartupInfo.cb=managed?sizeof startup:sizeof startup.StartupInfo;startup.lpAttributeList=managed?host_state.attributes:nullptr;PROCESS_INFORMATION process{};
 if(!CreateProcessW(host.c_str(),command.data(),nullptr,nullptr,managed?TRUE:FALSE,CREATE_SUSPENDED|(managed?EXTENDED_STARTUPINFO_PRESENT:0),nullptr,host.parent_path().c_str(),&startup.StartupInfo,&process))return 67;
 Handle child{process.hProcess},thread{process.hThread};bool resumed=false;
 auto fail=[&](int code){TerminateProcess(child.h,DWORD(code));WaitForSingleObject(child.h,5000);return code;};
 if(managed&&!AssignProcessToJobObject(host_state.job,child.h))return fail(76);
 if(managed){std::wstring physical;if(startup_network_state.policy_applied&&(!windows_network::canonical_executable_path(host.wstring(),physical)||!host_state.network_handoff.publish(child.h,physical,startup_network_state.generation,startup_network_state.requested_blocked,startup_network_state.applied_blocked,true)))return fail(79);if(!host_state.network_handoff.seal())return fail(79);}
 const auto path=core.wstring();const SIZE_T total=sizeof(extend_remote_context)+(path.size()+1)*sizeof(wchar_t);
 auto remote=(BYTE*)VirtualAllocEx(child.h,nullptr,total,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE);
 auto code=VirtualAllocEx(child.h,nullptr,sizeof extend_apc_blob,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE);if(!remote||!code)return fail(68);
 extend_remote_context context{};context.initialize_rva=DWORD(rva);context.path_length=WORD(path.size()*2);context.path_capacity=context.path_length+2;context.path_pointer=(uintptr_t)(remote+sizeof context);
 SIZE_T written=0;DWORD old=0;
 if(!WriteProcessMemory(child.h,remote,&context,sizeof context,&written)||written!=sizeof context||!WriteProcessMemory(child.h,remote+sizeof context,path.c_str(),(path.size()+1)*2,&written)||written!=(path.size()+1)*2||!WriteProcessMemory(child.h,code,extend_apc_blob,sizeof extend_apc_blob,&written)||written!=sizeof extend_apc_blob||!VirtualProtectEx(child.h,code,sizeof extend_apc_blob,PAGE_EXECUTE_READ,&old)||!FlushInstructionCache(child.h,code,sizeof extend_apc_blob))return fail(68);
 if(!QueueUserAPC((PAPCFUNC)code,thread.h,(ULONG_PTR)remote)||ResumeThread(thread.h)==DWORD(-1))return fail(69);resumed=true;(void)resumed;
 const ULONGLONG deadline=GetTickCount64()+15000;
 while(GetTickCount64()<deadline){
  if(WaitForSingleObject(child.h,0)==WAIT_OBJECT_0)return 70;
  SIZE_T read=0;if(!ReadProcessMemory(child.h,remote,&context,sizeof context,&read)||read!=sizeof context)return fail(71);
  if(context.complete){if(context.result||!context.module){std::cerr<<"Core initialization failed: "<<context.result<<'\n';return fail(72);}std::cout<<"{\"hostPid\":"<<process.dwProcessId<<",\"launcherPid\":"<<GetCurrentProcessId()<<",\"residentCore\":true,\"fixtureOnly\":"<<(fixture?"true":"false")<<",\"appContainer\":"<<"false"<<",\"dataCopied\":"<<(host_state.copied?"true":"false")<<"}\n";return 0;}
  Sleep(10);
 }
 // The remote APC storage stays allocated until process exit: completion is
 // published before the final ret, so freeing it here would race execution.
 return fail(74);
 }catch(const std::exception &e){std::cerr<<e.what()<<'\n';return 75;}
}
