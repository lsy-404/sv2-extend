#include "../loader/windows/editor_autostart.cpp"
#include <cassert>
#include <thread>
extern "C" MH_STATUS WINAPI MH_CreateHook(LPVOID,LPVOID,LPVOID*){return MH_ERROR_UNSUPPORTED_FUNCTION;}
extern "C" MH_STATUS WINAPI MH_QueueEnableHook(LPVOID){return MH_ERROR_UNSUPPORTED_FUNCTION;}
extern "C" MH_STATUS WINAPI MH_ApplyQueued(){return MH_ERROR_UNSUPPORTED_FUNCTION;}
extern "C" MH_STATUS WINAPI MH_RemoveHook(LPVOID){return MH_ERROR_UNSUPPORTED_FUNCTION;}
namespace editor_gate {bool version_matches(){return false;}}
static int runs;static void execute(void*){++runs;}
static int scans,cleanups;static void fake_init(void*,void*){}static void fake_cleanup(void*){++cleanups;}static void fake_scan(void*){++scans;}
static int unregisters,replies;static int unreg(const char*,void*){++unregisters;return 0;}static void reply(const char*,size_t,void*){++replies;}
int main(){
 using namespace editor_autostart;unsigned char fakeManager[0x60]{},object[0x20]{};auto base=reinterpret_cast<unsigned char*>(GetModuleHandleW(nullptr));void *vtable=base+0x14c9980;memcpy(object,&vtable,sizeof vtable);
 char path[]="C:\\controlled\\opensv2-loader-runtime.lua";const char *name=path;memcpy(object+8,&name,sizeof name);void *entries[]={object};void **array=entries;int count=1;memcpy(fakeManager+0x48,&array,sizeof array);memcpy(fakeManager+0x54,&count,sizeof count);
 controlled=L"C:\\controlled\\opensv2-loader-runtime.lua";manager=fakeManager;void *found=nullptr;assert(find(nullptr,false,&found)&&found==object);
 loader_api_v1 api{};api.unregister_capability_provider=unreg;editor_lua::Service service(&api);resident=&service;run=execute;candidate=object;generation=pendingGeneration=1;liveRoot.store(fakeManager);
 count=0;memcpy(fakeManager+0x54,&count,sizeof count);start(nullptr,0,0,0);assert(runs==0);
 count=1;memcpy(fakeManager+0x54,&count,sizeof count);start(nullptr,0,0,0);assert(runs==1);
 path[3]='x';start(nullptr,0,0,0);assert(runs==1);
 unsigned char root[0x2af0]{},project[0x40]{},scriptManager[0x58]{};void *p=project;memcpy(root+0x2ae8,&p,sizeof p);p=root+0x58;memcpy(project+0x38,&p,sizeof p);p=scriptManager;memcpy(root+0x6f8,&p,sizeof p);originalInit=fake_init;originalCleanup=fake_cleanup;scanEntry=fake_scan;init_root(root,nullptr);
 std::thread wrong([]{assert(!request_initial_scan());});wrong.join();assert(scans==0);
 p=nullptr;memcpy(project+0x38,&p,sizeof p);assert(!request_initial_scan());assert(std::string(status().initial_scan_reason)=="project_owner_mismatch");assert(scans==0);
 p=root+0x58;memcpy(project+0x38,&p,sizeof p);service.epoch="live";service.jobs.emplace("1",editor_lua::Job{{},reply,nullptr,0,false});cleanup_root(root);assert(cleanups==1&&unregisters==1&&replies==1&&service.epoch.empty());assert(!request_initial_scan());assert(std::string(status().initial_scan_reason)=="root_unavailable");assert(scans==0);
 init_root(root,nullptr);assert(request_initial_scan());assert(scans==1);assert(!request_initial_scan());cleanup_root(root);
}
