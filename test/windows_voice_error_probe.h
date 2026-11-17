#pragma once
#include "../loader/windows/voice_identity.h"
#include "../loader/windows/vendor/minhook/include/MinHook.h"
#include <cstring>
namespace editor_gate {bool version_matches();}
namespace voice_error_probe {
using json=nlohmann::json;
using Handler=void(*)(void*);
struct Entry {int code;int padding;const char *voice;const char *detail;const char *folder;};
static Handler original=nullptr;static DWORD thread=0;static json latest=json::object();
inline std::string text(const char *source){
 if(!source)return {};std::string value;
 for(size_t i=0;i<2048;i++){char c=0;if(!voice_identity::read_memory(source+i,&c,1))return "[unreadable]";if(!c){if(!value.empty()&&!MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,value.c_str(),-1,nullptr,0))return "[invalid utf8]";if(value.find('\\')!=std::string::npos||value.find('/')!=std::string::npos)return "[path redacted]";return value;}value.push_back(c);}
 return "[too long]";
}
inline json read(void *event){
 auto address=reinterpret_cast<uintptr_t>(event);if(address<4096||address>UINTPTR_MAX-0x12)return json();Entry *entries=nullptr;int count=0;unsigned char disabled=0;
 if(!voice_identity::read_memory(event,&entries,sizeof entries)||!voice_identity::read_memory(reinterpret_cast<void*>(address+0xc),&count,sizeof count)||!voice_identity::read_memory(reinterpret_cast<void*>(address+0x11),&disabled,1)||disabled||count<=0||count>64||!entries)return json();
 json rows=json::array();for(int i=0;i<count;i++){Entry entry{};if(!voice_identity::read_memory(entries+i,&entry,sizeof entry))return json();rows.push_back({{"code",entry.code},{"voice",text(entry.voice)},{"detail",text(entry.detail)},{"folderPresent",entry.folder&&*text(entry.folder).c_str()!=0}});}
 return {{"entries",rows}};
}
static void intercept(void *event){if(GetCurrentThreadId()==thread){try{auto value=read(event);if(value.is_object())latest=std::move(value);}catch(...){}}original(event);}
inline bool install(DWORD main_thread){
 if(original||!main_thread||GetCurrentThreadId()!=main_thread||!editor_gate::version_matches())return false;
 auto image=reinterpret_cast<unsigned char*>(GetModuleHandleW(nullptr));if(!image)return false;auto address=image+0x8bacd0;
 static constexpr unsigned char sig[]={0x48,0x89,0x5c,0x24,0x10,0x48,0x89,0x74,0x24,0x18,0x48,0x89,0x7c,0x24,0x20};MEMORY_BASIC_INFORMATION page{};
 if(!VirtualQuery(address,&page,sizeof page)||page.State!=MEM_COMMIT||page.Type!=MEM_IMAGE||page.AllocationBase!=image||(page.Protect&0xff)!=PAGE_EXECUTE_READ||memcmp(address,sig,sizeof sig))return false;
 void *trampoline=nullptr;if(MH_CreateHook(address,reinterpret_cast<void*>(intercept),&trampoline)!=MH_OK)return false;original=reinterpret_cast<Handler>(trampoline);thread=main_thread;
 if(MH_QueueEnableHook(address)!=MH_OK||MH_ApplyQueued()!=MH_OK){MH_RemoveHook(address);original=nullptr;return false;}return true;
}
}
