#include "voice_adapter.h"
#include "editor_gate.h"
#include "vendor/minhook/include/MinHook.h"
#include <cstring>

namespace voice_adapter {
struct Arguments {void *self;const void *values;int count;};
using Getter=void*(*)(void*,void*,const Arguments*);
using Cast=void*(*)(void*,long,const void*,const void*,int);
using ObjectGetter=void*(*)(void*);
static Getter original;static DWORD thread;static unsigned char *image;static Observer observer;static void *owner,*observedReference;static ULONGLONG deadline;static unsigned count;
static bool readable(const void *value,size_t length){
 auto address=reinterpret_cast<uintptr_t>(value);if(address<4096||address+length<address)return false;auto end=address+length;
 while(address<end){MEMORY_BASIC_INFORMATION page{};if(!VirtualQuery(reinterpret_cast<void*>(address),&page,sizeof page))return false;DWORD p=page.Protect&0xff;
  if(page.State!=MEM_COMMIT||(page.Protect&PAGE_GUARD)||(p!=PAGE_READONLY&&p!=PAGE_READWRITE&&p!=PAGE_WRITECOPY&&p!=PAGE_EXECUTE_READ&&p!=PAGE_EXECUTE_READWRITE&&p!=PAGE_EXECUTE_WRITECOPY))return false;
  auto start=reinterpret_cast<uintptr_t>(page.BaseAddress);if(start>address||page.RegionSize>UINTPTR_MAX-start)return false;auto next=start+page.RegionSize;if(next<=address)return false;address=next<end?next:end;
 }return true;
}
template<class T>static bool read(const void *address,T &value){if(!readable(address,sizeof value))return false;memcpy(&value,address,sizeof value);return true;}
static void *hook(void *closure,void *result,const Arguments *arguments){
 void *returned=original(closure,result,arguments);
 if(GetCurrentThreadId()!=thread||!observer||GetTickCount64()>deadline||!readable(arguments,sizeof *arguments)||arguments->count!=0||!arguments->self)return returned;
 try {
  void **table=nullptr;void *method=nullptr;if(!read(arguments->self,table)||!table||!read(table+7,method))return returned;
  MEMORY_BASIC_INFORMATION page{};if(!VirtualQuery(method,&page,sizeof page)||page.Type!=MEM_IMAGE||page.AllocationBase!=image||(page.Protect&0xff)!=PAGE_EXECUTE_READ)return returned;
  auto get=reinterpret_cast<ObjectGetter>(method);auto cast=reinterpret_cast<Cast>(image+0x10bd5cc);
  void *object=get(static_cast<char*>(arguments->self)+8);if(!object)return returned;
  object=cast(object,0,image+0x17a54c8,image+0x179ae18,0);if(!object)return returned;
  object=reinterpret_cast<ObjectGetter>(image+0x9f7560)(object);if(!object)return returned;
  void *reference=cast(object,0,image+0x17916d0,image+0x17916a0,0);if(!reference)return returned;
  ++count;if(count==1){observedReference=reference;observer(reference,owner);observedReference=nullptr;}
 }catch(...){observedReference=nullptr;count=2;}
 return returned;
}
int install(DWORD mainThread){
 if(original||GetCurrentThreadId()!=mainThread||!editor_gate::version_matches())return -1;
 auto base=reinterpret_cast<unsigned char*>(GetModuleHandleW(nullptr));constexpr size_t offset=0xa2c500;constexpr unsigned char expected[]={0x40,0x53,0x48,0x83,0xec,0x30,0x48,0x83,0xc1,0x08};
 MEMORY_BASIC_INFORMATION page{};if(!VirtualQuery(base+offset,&page,sizeof page)||page.Type!=MEM_IMAGE||page.State!=MEM_COMMIT||(page.Protect&0xff)!=PAGE_EXECUTE_READ||memcmp(base+offset,expected,sizeof expected))return -1;
 struct Check{size_t offset;unsigned char bytes[10];};const Check checks[]={{0xa0da00,{0x48,0x89,0x5c,0x24,0x08,0x48,0x89,0x74,0x24,0x10}},{0x9f7560,{0x48,0x89,0x5c,0x24,0x20,0x55,0x56,0x57,0x48,0x83}},{0x10bd5cc,{0x48,0x89,0x5c,0x24,0x10,0x48,0x89,0x74,0x24,0x18}}};
 for(const auto &check:checks){MEMORY_BASIC_INFORMATION region{};if(!VirtualQuery(base+check.offset,&region,sizeof region)||region.Type!=MEM_IMAGE||region.State!=MEM_COMMIT||(region.Protect&0xff)!=PAGE_EXECUTE_READ||memcmp(base+check.offset,check.bytes,sizeof check.bytes))return -1;}
 void *trampoline=nullptr;if(MH_CreateHook(base+offset,reinterpret_cast<void*>(hook),&trampoline)!=MH_OK)return -1;
 original=reinterpret_cast<Getter>(trampoline);thread=mainThread;image=base;
 if(MH_QueueEnableHook(base+offset)!=MH_OK||MH_ApplyQueued()!=MH_OK){MH_RemoveHook(base+offset);original=nullptr;return -1;}return 0;
}
bool begin(Observer callback,void *context){if(!original||!callback||GetCurrentThreadId()!=thread||(observer&&GetTickCount64()<=deadline))return false;observer=callback;owner=context;deadline=GetTickCount64()+500;count=0;return true;}
bool finish(){if(GetCurrentThreadId()!=thread||!observer)return false;bool valid=count==1&&GetTickCount64()<=deadline;observer=nullptr;owner=nullptr;return valid;}
bool is_observing(void *reference){return GetCurrentThreadId()==thread&&observer&&reference&&reference==observedReference&&GetTickCount64()<=deadline;}
}
