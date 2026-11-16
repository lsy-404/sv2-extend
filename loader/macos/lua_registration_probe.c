// Research-only supported-build probe. Wraps Lua's registration table entry,
// preserving the original implementation and never inspecting lua_State.
// Not included in production builds. Restricted to the managed test executable.
#include <mach/mach.h>
#include <mach/mach_vm.h>
#include <mach-o/dyld.h>
#include <mach-o/loader.h>
#include <libproc.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
typedef int (*lua_entry)(void *);
static lua_entry original;
static int observed_loadlib(void *state) {
    fprintf(stderr,"LUA_REGISTRATION direct_entry pid=%d main=%d state_present=%d\n",getpid(),pthread_main_np(),state!=NULL);fflush(stderr);
    return original(state);
}
__attribute__((constructor)) static void install(void) {
#if defined(__arm64__)
    if(!getenv("OPEN_SV2_LUA_REGISTRATION_PROBE"))return;
    char executable[PROC_PIDPATHINFO_MAXSIZE]={0};proc_pidpath(getpid(),executable,sizeof executable);
    if(!strstr(executable,"/SV2 Loader Probe.app/Contents/MacOS/synthv-studio-original")){fprintf(stderr,"LUA_REGISTRATION refused_unmanaged_host\n");return;}
    uint32_t image=0;const struct mach_header_64 *header=NULL;
    for(uint32_t i=0;i<_dyld_image_count();i++){const struct mach_header *candidate=_dyld_get_image_header(i);if(candidate && candidate->filetype==MH_EXECUTE){image=i;header=(const void*)candidate;break;}}
    if(!header){fprintf(stderr,"LUA_REGISTRATION executable_image_missing\n");return;}
    const uint8_t expectedUUID[16]={0xac,0xfe,0xfc,0x17,0x25,0xc2,0x34,0xd5,0xb2,0x8a,0xe8,0xbf,0x6f,0xfb,0x32,0xb4};
    const struct load_command *command=(const void*)(header+1);int matched=0;
    for(uint32_t i=0;i<header->ncmds;i++){
        if(command->cmd==LC_UUID && command->cmdsize==sizeof(struct uuid_command)){
            const unsigned char *uuid=((const struct uuid_command*)command)->uuid;
            matched=memcmp(uuid,expectedUUID,16)==0;
            fprintf(stderr,"LUA_REGISTRATION image=%s uuid=",_dyld_get_image_name(image));for(unsigned j=0;j<16;j++)fprintf(stderr,"%02x",uuid[j]);fprintf(stderr,"\n");
        }
        command=(const void*)((const char*)command+command->cmdsize);
    }
    if(!matched){fprintf(stderr,"LUA_REGISTRATION unsupported_image\n");return;}
    intptr_t slide=_dyld_get_image_vmaddr_slide(image);
    lua_entry *slot=(void*)(slide+0x1015c6b28ULL);
    lua_entry expected=(lua_entry)(slide+0x10111a970ULL);
    const uint8_t prologue[16]={0xf4,0x4f,0xbe,0xa9,0xfd,0x7b,0x01,0xa9,0xfd,0x43,0x00,0x91,0xf3,0x03,0x00,0xaa};
    if(*slot!=expected || memcmp((const void*)expected,prologue,sizeof prologue)){
        fprintf(stderr,"LUA_REGISTRATION signature_mismatch\n");return;
    }
    mach_vm_address_t region=(mach_vm_address_t)slot;mach_vm_size_t regionSize=0;
    vm_region_basic_info_data_64_t info;mach_msg_type_number_t count=VM_REGION_BASIC_INFO_COUNT_64;mach_port_t object=MACH_PORT_NULL;
    kern_return_t rc=mach_vm_region(mach_task_self(),&region,&regionSize,VM_REGION_BASIC_INFO_64,(vm_region_info_t)&info,&count,&object);
    if(object!=MACH_PORT_NULL)mach_port_deallocate(mach_task_self(),object);
    if(rc!=KERN_SUCCESS || (mach_vm_address_t)slot<region || (mach_vm_address_t)slot+sizeof *slot>region+regionSize){fprintf(stderr,"LUA_REGISTRATION region_unavailable\n");return;}
    mach_vm_address_t page=(mach_vm_address_t)slot & ~((mach_vm_address_t)vm_page_size-1);
    rc=mach_vm_protect(mach_task_self(),page,vm_page_size,FALSE,info.protection|VM_PROT_WRITE|VM_PROT_COPY);
    if(rc!=KERN_SUCCESS){fprintf(stderr,"LUA_REGISTRATION protection_failed=%d\n",rc);return;}
    original=expected;*slot=observed_loadlib;
    rc=mach_vm_protect(mach_task_self(),page,vm_page_size,FALSE,info.protection);
    if(rc!=KERN_SUCCESS){*slot=original;fprintf(stderr,"LUA_REGISTRATION restore_failed=%d\n",rc);return;}
    fprintf(stderr,"LUA_REGISTRATION installed pid=%d same_process=1 original_semantics=1\n",getpid());fflush(stderr);
#else
    fprintf(stderr,"LUA_REGISTRATION unsupported_architecture\n");
#endif
}
