#pragma once
#include "../loader/windows/voice_identity.h"
#include <map>
namespace voice_catalog_probe {
using json=nlohmann::json;
struct Pair {void *object;void *control;};
inline json snapshot(DWORD thread){
 if(!thread||GetCurrentThreadId()!=thread)return {{"ok",false},{"reason","thread"}};
 auto image=reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr));void *manager=nullptr;
 if(!image||!voice_identity::read_memory(reinterpret_cast<void*>(image+0x17f6758),&manager,sizeof manager)||!manager)return {{"ok",false},{"reason","manager"}};
 auto base=reinterpret_cast<uintptr_t>(manager);auto lock=reinterpret_cast<CRITICAL_SECTION*>(base+0x30);CRITICAL_SECTION copy{};
 if(!voice_identity::read_memory(lock,&copy,sizeof copy))return {{"ok",false},{"reason","lock"}};
 EnterCriticalSection(lock);json out={{"ok",true},{"records",0},{"variants",0},{"kinds",json::object()},{"availableFlags",json::object()},{"variantVtables",json::object()}};
 try{
  Pair *records=nullptr;int count=0;
  if(!voice_identity::read_memory(reinterpret_cast<void*>(base+0x10),&records,sizeof records)||!voice_identity::read_memory(reinterpret_cast<void*>(base+0x1c),&count,sizeof count)||count<0||count>4096||(count&&!records))throw 1;
  out["records"]=count;
  for(int i=0;i<count;i++){
   Pair record{};if(!voice_identity::read_memory(records+i,&record,sizeof record)||!record.object||!record.control)throw 1;
   auto address=reinterpret_cast<uintptr_t>(record.object);uint8_t kind=0;Pair *begin=nullptr,*end=nullptr;
   if(!voice_identity::read_memory(reinterpret_cast<void*>(address+0x140),&kind,1)||!voice_identity::read_memory(reinterpret_cast<void*>(address+0x148),&begin,sizeof begin)||!voice_identity::read_memory(reinterpret_cast<void*>(address+0x150),&end,sizeof end))throw 1;
   auto first=reinterpret_cast<uintptr_t>(begin),last=reinterpret_cast<uintptr_t>(end);if(last<first||(last-first)%sizeof(Pair)||(last-first)/sizeof(Pair)>64)throw 1;
   auto key=std::to_string(kind);out["kinds"][key]=out["kinds"].value(key,0)+1;
   for(auto at=first;at<last;at+=sizeof(Pair)){
    Pair variant{};uintptr_t table=0;uint8_t available=0;
    if(!voice_identity::read_memory(reinterpret_cast<void*>(at),&variant,sizeof variant)||!variant.object||!variant.control||!voice_identity::read_memory(variant.object,&table,sizeof table)||!voice_identity::read_memory(static_cast<char*>(variant.object)+0x34,&available,1)||table<image||table-image>0x2000000)throw 1;
    auto vtable=std::to_string(table-image),flag=std::to_string(available);out["variantVtables"][vtable]=out["variantVtables"].value(vtable,0)+1;out["availableFlags"][flag]=out["availableFlags"].value(flag,0)+1;out["variants"]=out["variants"].get<int>()+1;
   }
  }
 }catch(...){out={{"ok",false},{"reason","layout"}};}
 LeaveCriticalSection(lock);return out;
}
}
