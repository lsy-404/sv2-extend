#include "../loader/windows/voice_adapter.cpp"
#include "../loader/windows/voice_identity.h"
#include <cassert>
extern "C" MH_STATUS WINAPI MH_CreateHook(LPVOID,LPVOID,LPVOID*){return MH_ERROR_UNSUPPORTED_FUNCTION;}
extern "C" MH_STATUS WINAPI MH_QueueEnableHook(LPVOID){return MH_ERROR_UNSUPPORTED_FUNCTION;}
extern "C" MH_STATUS WINAPI MH_ApplyQueued(){return MH_ERROR_UNSUPPORTED_FUNCTION;}
extern "C" MH_STATUS WINAPI MH_RemoveHook(LPVOID){return MH_ERROR_UNSUPPORTED_FUNCTION;}
namespace editor_gate {bool version_matches(){return false;}}
static void callback(void*,void*){}
int main(){
 using namespace voice_adapter;thread=GetCurrentThreadId();observer=callback;deadline=GetTickCount64()+500;unsigned char reference[0xa8]{};const char *fields[]={"Voice","en","","backend","1"};for(size_t i=0;i<5;i++)memcpy(reference+0x80+i*8,&fields[i],8);
 voice_identity::json identity;assert(!voice_identity::read(reference,identity));observedReference=reference;assert(voice_identity::read(reference,identity));assert(identity["name"]=="Voice"&&identity["assigned"]==true&&identity["identityKey"].get<std::string>().size()==64&&identity["referenceKey"].get<std::string>().size()==64);
 auto first=identity["identityKey"];fields[0]="Other";memcpy(reference+0x80,&fields[0],8);assert(voice_identity::read(reference,identity)&&identity["identityKey"]!=first);
 char bad[]={char(0xff),0};fields[0]=bad;memcpy(reference+0x80,&fields[0],8);assert(!voice_identity::read(reference,identity));observedReference=nullptr;assert(!voice_identity::read(reference,identity));
}
