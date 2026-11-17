#include "javascript_gate.h"
#include "../../sdk/include/loader_api.h"
namespace editor_gate { bool version_matches(); }
#include "vendor/minhook/include/MinHook.h"
#include <windows.h>
#include <cstring>
#include <string>
#include <vector>

namespace javascript_gate {
using Encoder=int (*)(void *);
using PushString=const char *(*)(void *,const char *,size_t);
static Encoder original;
static PushString push;
static Router route;
static const char *module_source;
static DWORD thread;
static std::string ascii(const char *source){
 size_t length=strnlen(source,1048577);if(length>1048576)return "{\"ok\":false,\"error\":{\"code\":\"INVALID_NATIVE_RESPONSE\"}}";
 int count=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,source,(int)length,nullptr,0);
 if(!count && length)return "{\"ok\":false,\"error\":{\"code\":\"INVALID_NATIVE_RESPONSE\"}}";
 std::vector<wchar_t> wide(count);if(count&&!MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,source,(int)length,wide.data(),count))return "{\"ok\":false,\"error\":{\"code\":\"INVALID_NATIVE_RESPONSE\"}}";
 static constexpr char hex[]="0123456789abcdef";std::string result;result.reserve(length);
 for(auto c:wide){if(c<128)result.push_back((char)c);else{result.append("\\u");result.push_back(hex[(c>>12)&15]);result.push_back(hex[(c>>8)&15]);result.push_back(hex[(c>>4)&15]);result.push_back(hex[c&15]);}}
 return result;
}
static int intercept(void *context){
 uintptr_t bottom=0,top=0;memcpy(&bottom,(char*)context+0x68,8);memcpy(&top,(char*)context+0x70,8);
 if(!bottom||top<bottom||top-bottom<32||top-bottom>1048576||(top-bottom)%16)return original(context);
 uint32_t tag=0;uintptr_t string=0;memcpy(&tag,(void*)bottom,4);memcpy(&string,(void*)(bottom+8),8);
 if(tag!=8||!string)return original(context);
 uint32_t length=0;memcpy(&length,(void*)(string+0x18),4);
 constexpr char call[]="opensv2.v1",module[]="opensv2.module.v1";
 bool is_call=length==sizeof(call)-1&&!memcmp((void*)(string+0x20),call,sizeof(call)-1);
 bool is_module=length==sizeof(module)-1&&!memcmp((void*)(string+0x20),module,sizeof(module)-1);
 if(!is_call&&!is_module)return original(context);
 const char *result="{\"ok\":false,\"error\":{\"code\":\"INVALID_NATIVE_CALL\"}}";
 if(GetCurrentThreadId()!=thread)result="{\"ok\":false,\"error\":{\"code\":\"MAIN_THREAD_REQUIRED\"}}";
 else if(is_module)result=module_source;
 else {memcpy(&tag,(void*)(bottom+16),4);memcpy(&string,(void*)(bottom+24),8);if(tag==8&&string){memcpy(&length,(void*)(string+0x18),4);const char *response=route((const char*)(string+0x20),length,true);if(response)result=response;}}
 auto encoded=ascii(result);push(context,encoded.data(),encoded.size());return 1;
}
static bool matches(unsigned char *address,const unsigned char *bytes,size_t length){MEMORY_BASIC_INFORMATION page{};return VirtualQuery(address,&page,sizeof page)&&page.State==MEM_COMMIT&&page.Type==MEM_IMAGE&&(page.Protect&0xff)==PAGE_EXECUTE_READ&&address+length<=static_cast<unsigned char*>(page.BaseAddress)+page.RegionSize&&!memcmp(address,bytes,length);}
int install(Router router,const char *module){
 if(!router||!module||original||!editor_gate::version_matches())return LOADER_ERR_NOT_SUPPORTED;
 size_t module_length=strnlen(module,1048577);if(module_length>1048576)return LOADER_ERR_NOT_SUPPORTED;
 for(size_t i=0;i<module_length;i++)if((unsigned char)module[i]>=128)return LOADER_ERR_NOT_SUPPORTED;
 auto base=reinterpret_cast<unsigned char*>(GetModuleHandleW(nullptr));if(!base)return LOADER_ERR_NOT_SUPPORTED;
 auto dos=reinterpret_cast<IMAGE_DOS_HEADER*>(base);if(dos->e_magic!=IMAGE_DOS_SIGNATURE)return LOADER_ERR_NOT_SUPPORTED;
 auto nt=reinterpret_cast<IMAGE_NT_HEADERS64*>(base+dos->e_lfanew);constexpr size_t encoder=0xfac990,push_string=0xf9c3e0;
 static constexpr unsigned char encoder_bytes[]={0x40,0x53,0x48,0x83,0xec,0x30,0x48,0x8b,0x41,0x68,0x48,0x8b,0xd9,0x48,0x8b,0x49,0x70,0x33,0xd2,0x48,0x2b,0xc8,0x48,0xc1,0xf9,0x04,0x85,0xc9,0x48,0x0f,0x44,0xc2};
 static constexpr unsigned char push_bytes[]={0x40,0x53,0x48,0x83,0xec,0x20,0x48,0x8b,0x41,0x58,0x4d,0x8b,0xc8,0x48,0x8b,0xd9,0x48,0x39,0x41,0x70,0x73,0x47,0x45,0x33,0xc0,0x48,0x85,0xd2,0x4d,0x0f,0x45,0xc1};
 if(nt->Signature!=IMAGE_NT_SIGNATURE||nt->OptionalHeader.SizeOfImage<encoder+sizeof encoder_bytes||!matches(base+encoder,encoder_bytes,sizeof encoder_bytes)||!matches(base+push_string,push_bytes,sizeof push_bytes))return LOADER_ERR_NOT_SUPPORTED;
 void *trampoline=nullptr;if(MH_CreateHook(base+encoder,reinterpret_cast<void*>(intercept),&trampoline)!=MH_OK)return LOADER_ERR_NOT_SUPPORTED;
 original=reinterpret_cast<Encoder>(trampoline);push=reinterpret_cast<PushString>(base+push_string);route=router;module_source=module;thread=GetCurrentThreadId();
 if(MH_QueueEnableHook(base+encoder)!=MH_OK||MH_ApplyQueued()!=MH_OK){MH_RemoveHook(base+encoder);original=nullptr;push=nullptr;route=nullptr;module_source=nullptr;return LOADER_ERR_NOT_SUPPORTED;}
 return LOADER_OK;
}
bool ready(){return original!=nullptr;}
}
