#pragma once
#include "voice_identity.h"
#include <algorithm>
#include <set>
namespace editor_gate {bool version_matches();}
namespace voice_catalog_native {
using json=nlohmann::json;
struct Pair {void *object;void *control;};
inline bool signature(uintptr_t image,size_t offset,const unsigned char *expected,size_t length){MEMORY_BASIC_INFORMATION page{};auto ptr=reinterpret_cast<void*>(image+offset);return VirtualQuery(ptr,&page,sizeof page)&&page.State==MEM_COMMIT&&page.Type==MEM_IMAGE&&page.AllocationBase==reinterpret_cast<void*>(image)&&(page.Protect&0xff)==PAGE_EXECUTE_READ&&!memcmp(ptr,expected,length);}
inline bool supported(uintptr_t image){
 static const unsigned char convert[]={0x48,0x89,0x5c,0x24,0x10,0x48,0x89,0x6c,0x24,0x18,0x48,0x89,0x74,0x24,0x20,0x57};
 static const unsigned char destroy[]={0x48,0x89,0x5c,0x24,0x08,0x48,0x89,0x74,0x24,0x10,0x57,0x48,0x83,0xec,0x20,0x48};
 static const unsigned char available[]={0x0f,0xb6,0x41,0x34,0xc3};
 static const unsigned char language[]={0x48,0x8b,0x41,0x50,0x48,0x8d,0x0d,0x85,0x65,0xed,0x00,0x48,0x89,0x02,0x48,0x83};
 static const unsigned char backend[]={0x40,0x53,0x48,0x83,0xec,0x30,0x48,0x8b,0xda,0x48,0x8d,0x15,0x14,0x4f,0xc0,0x00};
 static const unsigned char languages[]={0x40,0x53,0x48,0x83,0xec,0x20,0x33,0xc0,0x48,0x8b,0xda,0x48,0x89,0x02,0x48,0x89};
 return signature(image,0x8c1910,convert,sizeof convert)&&signature(image,0x865d60,destroy,sizeof destroy)&&signature(image,0x86bb20,available,sizeof available)&&signature(image,0x8b9e00,language,sizeof language)&&signature(image,0x8b9e20,backend,sizeof backend)&&signature(image,0x8b9e50,languages,sizeof languages);
}
inline bool known_variant(uintptr_t image,void *object){
 uintptr_t table=0;if(!voice_identity::read_memory(object,&table,sizeof table))return false;
 if(table!=image+0x14bf4d0&&table!=image+0x14bf5b0&&table!=image+0x14bf690)return false;
 uintptr_t slots[13]{};if(!voice_identity::read_memory(reinterpret_cast<void*>(table),slots,sizeof slots))return false;
 return slots[1]==image+0x86bb20&&slots[2]==image+0x8b9e00&&slots[5]==image+0x8b9e20&&slots[12]==image+0x8b9e50;
}
struct Database {
 uintptr_t fields[5];uintptr_t image;
 explicit Database(uintptr_t base):image(base){for(auto &field:fields)field=base+0x17903a0;}
 ~Database(){reinterpret_cast<void(*)(void*)>(image+0x865d60)(fields);}
 Database(const Database&)=delete;Database&operator=(const Database&)=delete;
 bool identity(json &value){
  static constexpr const char *names[]={"name","language","languageOverride","backendType","version"};value=json::object();
  for(size_t i=0;i<5;i++){std::string text;if(!voice_identity::string_at(reinterpret_cast<const char*>(fields[i]),text))return false;value[names[i]]=text;}
  value["assigned"]=!value["name"].get<std::string>().empty();value["source"]="native-reference";std::string key;auto body=value.dump();if(!voice_identity::digest(body.data(),body.size(),key))return false;value["identityKey"]=key;value["voicebankId"]=key;return true;
 }
};
inline bool probe(DWORD thread,json &result){
 if(!thread||GetCurrentThreadId()!=thread||!editor_gate::version_matches())return false;
 auto image=reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr));if(!image||!supported(image))return false;void *manager=nullptr;
 if(!voice_identity::read_memory(reinterpret_cast<void*>(image+0x17f6758),&manager,sizeof manager)||!manager)return false;
 auto base=reinterpret_cast<uintptr_t>(manager);auto lock=reinterpret_cast<CRITICAL_SECTION*>(base+0x30);CRITICAL_SECTION copy{};if(!voice_identity::read_memory(lock,&copy,sizeof copy))return false;
 EnterCriticalSection(lock);bool ok=false;
 try{
  Pair *records=nullptr;int count=0;json values=json::array();std::set<std::string> seen;
  if(!voice_identity::read_memory(reinterpret_cast<void*>(base+0x10),&records,sizeof records)||!voice_identity::read_memory(reinterpret_cast<void*>(base+0x1c),&count,sizeof count)||count<0||count>4096||(count&&!records))throw 1;
  for(int i=0;i<count;i++){
   Pair record{};if(!voice_identity::read_memory(records+i,&record,sizeof record)||!record.object||!record.control)throw 1;auto address=reinterpret_cast<uintptr_t>(record.object);uint8_t kind=0;Pair *begin=nullptr,*end=nullptr;
   if(!voice_identity::read_memory(reinterpret_cast<void*>(address+0x140),&kind,1)||!voice_identity::read_memory(reinterpret_cast<void*>(address+0x148),&begin,sizeof begin)||!voice_identity::read_memory(reinterpret_cast<void*>(address+0x150),&end,sizeof end))throw 1;
   auto first=reinterpret_cast<uintptr_t>(begin),last=reinterpret_cast<uintptr_t>(end);if(last<first||(last-first)%sizeof(Pair)||(last-first)/sizeof(Pair)>64)throw 1;
   if(kind){if(first!=last)throw 1;continue;}
   for(auto at=first;at<last;at+=sizeof(Pair)){
    Pair variant{};uint8_t available=0;if(!voice_identity::read_memory(reinterpret_cast<void*>(at),&variant,sizeof variant)||!variant.object||!variant.control||!known_variant(image,variant.object)||!voice_identity::read_memory(static_cast<char*>(variant.object)+0x34,&available,1)||available>1)throw 1;
    if(!available)continue;Database database(image);reinterpret_cast<void(*)(void*,void*)>(image+0x8c1910)(variant.object,database.fields);json identity;if(!database.identity(identity)||!identity["assigned"].get<bool>())throw 1;
    auto id=identity["identityKey"].get<std::string>();if(seen.insert(id).second)values.push_back(std::move(identity));if(values.size()>1024)throw 1;
   }
  }
  std::sort(values.begin(),values.end(),[](const json &a,const json &b){return a["identityKey"].get<std::string>()<b["identityKey"].get<std::string>();});result={{"voicebanks",values},{"source","native-host-catalog"},{"availableOnly",true}};ok=true;
 }catch(...){ok=false;}
 LeaveCriticalSection(lock);return ok;
}
}
