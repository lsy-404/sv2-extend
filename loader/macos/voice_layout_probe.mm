// Research-only observation of the host's existing getVoice callback.
// It preserves the original return value and only inspects a validated live
// NoteGroupReference. It is NOT part of the production build.
#import <Foundation/Foundation.h>
#include <mach/mach.h>
#include <mach/mach_vm.h>
#include <mach-o/dyld.h>
#include <mach-o/loader.h>
#include <pthread.h>
#include <stdint.h>
#include <string.h>
struct NativeValue {uint64_t words[3];};
struct NativeArguments {void *thisValue;const void *arguments;int32_t count;};
using VoiceFunction=NativeValue(*)(void*,const NativeArguments*);
static VoiceFunction original;
static intptr_t slide;
static bool readMemory(uintptr_t address,void *bytes,size_t length) {
    mach_vm_size_t actual=0;
    return address>4096 && mach_vm_read_overwrite(mach_task_self(),address,length,(mach_vm_address_t)bytes,&actual)==KERN_SUCCESS && actual==length;
}
static void *cast(void *value,uintptr_t source,uintptr_t target) {
    if(!value)return nullptr;
    return ((void*(*)(const void*,const void*,const void*,ptrdiff_t))(slide+0x10119772cULL))(value,(const void*)(slide+source),(const void*)(slide+target),0);
}
static NativeValue observe(void *closure,const NativeArguments *arguments) {
    NativeValue result=original(closure,arguments);
    if(!pthread_main_np() || !arguments || arguments->count!=0 || !arguments->thisValue)return result;
    uintptr_t type=0,getObject=0;
    if(!readMemory((uintptr_t)arguments->thisValue,&type,8) || !readMemory(type+0x38,&getObject,8))return result;
    void *object=((void*(*)(void*))getObject)((char*)arguments->thisValue+8);
    object=cast(object,0x1015529d0ULL,0x10156bda8ULL);
    if(!object)return result;
    void *nested=((void*(*)(void*))(slide+0x100c5ff00ULL))(object);
    void *reference=cast(nested,0x1015867e0ULL,0x1015a1df0ULL);
    if(!reference)return result;
    uint8_t memory[0x138];if(!readMemory((uintptr_t)reference,memory,sizeof memory))return result;
    fprintf(stderr,"VOICE_LAYOUT getVoice-preserved=1 model-valid=1\n");
    for(size_t offset=0;offset+8<=sizeof memory;offset+=8){
        uintptr_t candidate=0;memcpy(&candidate,memory+offset,8);char string[97]={0};
        if(!readMemory(candidate,string,96))continue;
        if(!memchr(string,0,96))continue;
        // Scope observations to the fixture's voice identity and public fields.
        if(!strncmp(string,"MEDIUM5",7) || !strcmp(string,"mandarin") || !strcmp(string,"SVR3") || !strcmp(string,"100"))fprintf(stderr,"VOICE_LAYOUT offset=0x%zx string=%s\n",offset,string);
    }
    fflush(stderr);return result;
}
__attribute__((constructor)) static void install(void) {
    if(!getenv("OPEN_SV2_VOICE_LAYOUT_PROBE"))return;
    dispatch_async(dispatch_get_main_queue(),^{
        const struct mach_header_64 *header=nullptr;uint32_t image=0;
        for(uint32_t i=0;i<_dyld_image_count();i++){const struct mach_header *h=_dyld_get_image_header(i);if(h && h->filetype==MH_EXECUTE){header=(const struct mach_header_64*)h;image=i;break;}}
        if(!header)return;
        const uint8_t uuid[16]={0xac,0xfe,0xfc,0x17,0x25,0xc2,0x34,0xd5,0xb2,0x8a,0xe8,0xbf,0x6f,0xfb,0x32,0xb4};bool matched=false;
        const struct load_command *command=(const struct load_command*)(header+1);
        for(uint32_t i=0;i<header->ncmds;i++){if(command->cmd==LC_UUID && command->cmdsize==sizeof(struct uuid_command))matched=!memcmp(((const struct uuid_command*)command)->uuid,uuid,16);command=(const struct load_command*)((const char*)command+command->cmdsize);}
        if(!matched)return;slide=_dyld_get_image_vmaddr_slide(image);
        VoiceFunction *slot=(VoiceFunction*)(slide+0x10159bb58ULL);VoiceFunction expected=(VoiceFunction)(slide+0x100ca7754ULL);
        const uint8_t bytes[16]={0xff,0x43,0x03,0xd1,0xfc,0x6f,0x07,0xa9,0xfa,0x67,0x08,0xa9,0xf8,0x5f,0x09,0xa9};
        if(*slot!=expected || memcmp((const void*)expected,bytes,16)){fprintf(stderr,"VOICE_LAYOUT signature-mismatch\n");return;}
        mach_vm_address_t region=(mach_vm_address_t)slot;mach_vm_size_t length=0;vm_region_basic_info_data_64_t info;mach_msg_type_number_t count=VM_REGION_BASIC_INFO_COUNT_64;mach_port_t object=MACH_PORT_NULL;
        kern_return_t rc=mach_vm_region(mach_task_self(),&region,&length,VM_REGION_BASIC_INFO_64,(vm_region_info_t)&info,&count,&object);if(object!=MACH_PORT_NULL)mach_port_deallocate(mach_task_self(),object);
        if(rc!=KERN_SUCCESS || (mach_vm_address_t)slot<region || (mach_vm_address_t)slot+8>region+length)return;
        mach_vm_address_t page=(mach_vm_address_t)slot & ~((mach_vm_address_t)vm_page_size-1);
        if(mach_vm_protect(mach_task_self(),page,vm_page_size,FALSE,info.protection|VM_PROT_WRITE|VM_PROT_COPY)!=KERN_SUCCESS)return;
        original=expected;*slot=observe;
        if(mach_vm_protect(mach_task_self(),page,vm_page_size,FALSE,info.protection)!=KERN_SUCCESS){*slot=expected;original=nullptr;return;}
        fprintf(stderr,"VOICE_LAYOUT observer-installed\n");
    });
}
