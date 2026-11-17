#include <cassert>
#include "../loader/windows/voice_catalog_native.h"
namespace editor_gate {bool version_matches(){return false;}}
namespace voice_adapter {bool is_observing(void*){return false;}}
int main(){
 auto image=reinterpret_cast<uintptr_t>(VirtualAlloc(nullptr,0x14c0000,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE));assert(image);
 auto table=reinterpret_cast<uintptr_t*>(image+0x14bf4d0);table[1]=image+0x86bb20;table[2]=image+0x8b9e00;table[5]=image+0x8b9e20;table[12]=image+0x8b9e50;
 uintptr_t object=reinterpret_cast<uintptr_t>(table);assert(voice_catalog_native::known_variant(image,&object));table[5]++;assert(!voice_catalog_native::known_variant(image,&object));table[5]--;
 object=image+0x14bf810;assert(!voice_catalog_native::known_variant(image,&object));nlohmann::json result;assert(!voice_catalog_native::probe(GetCurrentThreadId(),result));
 VirtualFree(reinterpret_cast<void*>(image),0,MEM_RELEASE);
}
