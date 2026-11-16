#include "editor_autostart.h"
#include "editor_gate.h"
#include "vendor/minhook/include/MinHook.h"
#include <cstring>
#include <string>
#include <atomic>

namespace editor_autostart {
using Scan=void(*)(void*);using Run=void(*)(void*);using RootInit=void(*)(void*,void*);using RootCleanup=void(*)(void*);
static Scan original,scanEntry;static Run run;static editor_lua::Service *resident;static std::wstring controlled;
static RootInit originalInit;static RootCleanup originalCleanup;static std::atomic<void*> liveRoot;
static void *manager;static void *candidate;static unsigned long long generation,pendingGeneration;static UINT_PTR timer;
static bool scanSeen,runRequested,initialScanRequested;
static const char *initialScanReason="not_attempted";
Status status(){return {resident!=nullptr,scanSeen,runRequested,initialScanReason};}
static bool readable(const void *pointer,size_t length){
 if(!pointer||!length)return false;auto cursor=reinterpret_cast<uintptr_t>(pointer);if(cursor+length<cursor)return false;auto end=cursor+length;
 while(cursor<end){MEMORY_BASIC_INFORMATION page{};if(!VirtualQuery(reinterpret_cast<void*>(cursor),&page,sizeof page))return false;DWORD protection=page.Protect&0xff;
  if(page.State!=MEM_COMMIT||(page.Protect&PAGE_GUARD)||(protection!=PAGE_READONLY&&protection!=PAGE_READWRITE&&protection!=PAGE_WRITECOPY&&protection!=PAGE_EXECUTE_READ&&protection!=PAGE_EXECUTE_READWRITE&&protection!=PAGE_EXECUTE_WRITECOPY))return false;
  auto start=reinterpret_cast<uintptr_t>(page.BaseAddress);if(start>cursor||page.RegionSize>UINTPTR_MAX-start)return false;auto next=start+page.RegionSize;if(next<=cursor)return false;cursor=next<end?next:end;
 }return true;
}
template<class T>static bool read(const void *address,T &value){if(!readable(address,sizeof value))return false;memcpy(&value,address,sizeof value);return true;}
static bool same_path(void *object){
 auto base=reinterpret_cast<unsigned char*>(GetModuleHandleW(nullptr));auto bytes=static_cast<unsigned char*>(object);void *vtable=nullptr;const char *name=nullptr;
 if(!read(bytes,vtable)||vtable!=base+0x14c9980||!read(bytes+8,name)||!name)return false;
 std::string path;for(size_t i=0;i<32767;i++){char ch;if(!read(name+i,ch))return false;if(!ch)break;path.push_back(ch);if(i==32766)return false;}
 int count=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,path.c_str(),-1,nullptr,0);if(count<=0||count>32767)return false;
 std::wstring wide(static_cast<size_t>(count),L'\0');if(!MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,path.c_str(),-1,wide.data(),count))return false;
 wchar_t full[32768];DWORD size=GetFullPathNameW(wide.c_str(),32768,full,nullptr);return size&&size<32768&&CompareStringOrdinal(full,-1,controlled.c_str(),-1,TRUE)==CSTR_EQUAL;
}
static bool find(void *target,bool requireTarget,void **found){
 auto bytes=static_cast<unsigned char*>(manager);void **items=nullptr;int count=0;
 if(!manager||!read(bytes+0x48,items)||!read(bytes+0x54,count)||count<0||count>512||(!items&&count)||!readable(items,count?static_cast<size_t>(count)*sizeof(void*):1))return false;
 void *match=nullptr;for(int i=0;i<count;i++){void *item=nullptr;if(!read(items+i,item))return false;if(item&&same_path(item)){if(match)return false;match=item;}}
 if(!match||(requireTarget&&match!=target))return false;*found=match;return true;
}
static VOID CALLBACK start(HWND,UINT,UINT_PTR id,DWORD){
 KillTimer(nullptr,id);timer=0;if(!resident||!liveRoot.load()||!resident->epoch.empty()||pendingGeneration!=generation)return;void *item=nullptr;
 if(find(candidate,true,&item)){runRequested=true;run(item);}
}
static void scan(void *value){
 if(!resident||GetCurrentThreadId()!=resident->thread){original(value);return;}
 auto ticket=++generation;scanSeen=true;manager=value;candidate=nullptr;if(timer){KillTimer(nullptr,timer);timer=0;}
 original(value);if(ticket!=generation||!resident->epoch.empty())return;void *item=nullptr;
 if(find(nullptr,false,&item)){candidate=item;pendingGeneration=generation;timer=SetTimer(nullptr,0,1,start);}
}
static void init_root(void *root,void *context){
 originalInit(root,context);if(resident&&GetCurrentThreadId()==resident->thread)liveRoot.store(root);
}
static void cleanup_root(void *root){
 void *expected=root;bool owned=liveRoot.compare_exchange_strong(expected,nullptr);
 if(owned&&resident&&GetCurrentThreadId()==resident->thread){
  ++generation;if(timer){KillTimer(nullptr,timer);timer=0;}manager=nullptr;candidate=nullptr;scanSeen=false;runRequested=false;initialScanRequested=false;
  if(!resident->epoch.empty()){
   resident->api->unregister_capability_provider("editor.lua",resident);resident->epoch.clear();auto jobs=std::move(resident->jobs);resident->jobs.clear();
   for(auto &entry:jobs)editor_lua::Service::reply(entry.second,editor_lua::Service::fail("EDITOR_PROVIDER_OFFLINE"));
  }
 }
 originalCleanup(root);
}
bool request_initial_scan(){
 if(!resident){initialScanReason="not_installed";return false;}if(GetCurrentThreadId()!=resident->thread){initialScanReason="wrong_thread";return false;}if(scanSeen){initialScanReason="scan_already_seen";return false;}if(initialScanRequested){initialScanReason="already_requested";return false;}if(!resident->epoch.empty()){initialScanReason="editor_attached";return false;}
 auto root=static_cast<unsigned char*>(liveRoot.load());if(!root){initialScanReason="root_unavailable";return false;}
 void *project=nullptr,*projectOwner=nullptr,*scriptManager=nullptr,*items=nullptr;int capacity=0,count=0;
 if(!read(root+0x2ae8,project)||!project){initialScanReason="project_unavailable";return false;}
 if(!read(static_cast<unsigned char*>(project)+0x38,projectOwner)||projectOwner!=root+0x58){initialScanReason="project_owner_mismatch";return false;}
 if(!read(root+0x6f8,scriptManager)||!scriptManager||!readable(scriptManager,0x58)){initialScanReason="manager_unavailable";return false;}
 if(!read(static_cast<unsigned char*>(scriptManager)+0x48,items)||!read(static_cast<unsigned char*>(scriptManager)+0x50,capacity)||!read(static_cast<unsigned char*>(scriptManager)+0x54,count)||capacity<0||capacity>512||count<0||count>capacity||(!items&&count)||(count&&!readable(items,static_cast<size_t>(count)*sizeof(void*)))){initialScanReason="list_invalid";return false;}
 initialScanRequested=true;initialScanReason="requested";scanEntry(scriptManager);return true;
}
int install(editor_lua::Service *service,const wchar_t *scriptPath){
 if(!service||!scriptPath||resident||GetCurrentThreadId()!=service->thread||!editor_gate::version_matches())return LOADER_ERR_NOT_SUPPORTED;
 wchar_t full[32768];DWORD length=GetFullPathNameW(scriptPath,32768,full,nullptr);if(!length||length>=32768)return LOADER_ERR_INVALID;
 auto base=reinterpret_cast<unsigned char*>(GetModuleHandleW(nullptr));constexpr size_t scanOffset=0x9e9170,runOffset=0x9e8080,initOffset=0x8712f0,cleanupOffset=0x871b70;
 constexpr unsigned char scanSig[]={0x48,0x89,0x5c,0x24,0x10},runSig[]={0x48,0x89,0x5c,0x24,0x10,0x48,0x89,0x74,0x24,0x18};
 MEMORY_BASIC_INFORMATION a{},b{};if(!VirtualQuery(base+scanOffset,&a,sizeof a)||!VirtualQuery(base+runOffset,&b,sizeof b)||a.Type!=MEM_IMAGE||b.Type!=MEM_IMAGE||a.State!=MEM_COMMIT||b.State!=MEM_COMMIT||(a.Protect&0xff)!=PAGE_EXECUTE_READ||(b.Protect&0xff)!=PAGE_EXECUTE_READ||memcmp(base+scanOffset,scanSig,sizeof scanSig)||memcmp(base+runOffset,runSig,sizeof runSig))return LOADER_ERR_NOT_SUPPORTED;
 constexpr unsigned char lifecycleSig[]={0x48,0x89,0x5c,0x24,0x10,0x48,0x89,0x74,0x24,0x18};
 MEMORY_BASIC_INFORMATION c{},d{};if(!VirtualQuery(base+initOffset,&c,sizeof c)||!VirtualQuery(base+cleanupOffset,&d,sizeof d)||c.Type!=MEM_IMAGE||d.Type!=MEM_IMAGE||c.State!=MEM_COMMIT||d.State!=MEM_COMMIT||(c.Protect&0xff)!=PAGE_EXECUTE_READ||(d.Protect&0xff)!=PAGE_EXECUTE_READ||memcmp(base+initOffset,lifecycleSig,sizeof lifecycleSig)||memcmp(base+cleanupOffset,lifecycleSig,sizeof lifecycleSig))return LOADER_ERR_NOT_SUPPORTED;
 void *scanTrampoline=nullptr,*initTrampoline=nullptr,*cleanupTrampoline=nullptr;
 if(MH_CreateHook(base+scanOffset,reinterpret_cast<void*>(scan),&scanTrampoline)!=MH_OK)return LOADER_ERR_NOT_SUPPORTED;
 if(MH_CreateHook(base+initOffset,reinterpret_cast<void*>(init_root),&initTrampoline)!=MH_OK){MH_RemoveHook(base+scanOffset);return LOADER_ERR_NOT_SUPPORTED;}
 if(MH_CreateHook(base+cleanupOffset,reinterpret_cast<void*>(cleanup_root),&cleanupTrampoline)!=MH_OK){MH_RemoveHook(base+initOffset);MH_RemoveHook(base+scanOffset);return LOADER_ERR_NOT_SUPPORTED;}
 original=reinterpret_cast<Scan>(scanTrampoline);scanEntry=reinterpret_cast<Scan>(base+scanOffset);originalInit=reinterpret_cast<RootInit>(initTrampoline);originalCleanup=reinterpret_cast<RootCleanup>(cleanupTrampoline);run=reinterpret_cast<Run>(base+runOffset);controlled=full;resident=service;
 if(MH_QueueEnableHook(base+scanOffset)!=MH_OK||MH_QueueEnableHook(base+initOffset)!=MH_OK||MH_QueueEnableHook(base+cleanupOffset)!=MH_OK||MH_ApplyQueued()!=MH_OK){resident=nullptr;MH_RemoveHook(base+cleanupOffset);MH_RemoveHook(base+initOffset);MH_RemoveHook(base+scanOffset);return LOADER_ERR_NOT_SUPPORTED;}return LOADER_OK;
}
}
