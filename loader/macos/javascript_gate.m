// Supported-build JS adapter. Changes only the managed process's private builtin
// registration-table page. VM allocations/conversion use the HOST's own helpers.
// The tiny read-only context/string layout below is pinned to a full code profile;
// no alternate Duktape VM is linked and no raw VM state is written.
#import <Foundation/Foundation.h>
#include <CommonCrypto/CommonDigest.h>
#include <mach/mach.h>
#include <mach/mach_vm.h>
#include <mach-o/dyld.h>
#include <mach-o/loader.h>
#include <libproc.h>
#include <pthread.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include "build/javascript_bundle.h"
extern const char *loader_script_call_json_for_runtime(const char*,size_t,int);
#if defined(__arm64__)
typedef int (*Encoder)(void*);
typedef void *(*PushFixedBuffer)(void*,size_t);
typedef const char *(*BufferToStringTop)(void*);
static Encoder original;
static PushFixedBuffer pushBuffer;
static BufferToStringTop bufferToString;
static int supported_encoder(void *context) {
    uintptr_t bottom=0,top=0;memcpy(&bottom,(char*)context+0x68,8);memcpy(&top,(char*)context+0x70,8);
    if(!bottom || top<bottom || top-bottom<32 || top-bottom>1048576 || (top-bottom)%16)return original(context);
    uint32_t tag=0;uintptr_t string=0;memcpy(&tag,(void*)bottom,4);memcpy(&string,(void*)(bottom+8),8);
    if(tag!=8 || !string)return original(context);
    uint32_t size=0;memcpy(&size,(void*)(string+0x18),4);
    const char format[]="opensv2.v1",moduleFormat[]="opensv2.module.v1";
    BOOL isCall=size==sizeof format-1 && !memcmp((void*)(string+0x20),format,sizeof format-1);
    BOOL isModule=size==sizeof moduleFormat-1 && !memcmp((void*)(string+0x20),moduleFormat,sizeof moduleFormat-1);
    if(!isCall && !isModule)return original(context);
    const char *response=isModule?loaderJavaScriptModule:NULL;
    memcpy(&tag,(void*)(bottom+16),4);memcpy(&string,(void*)(bottom+24),8);
    if(!isModule && tag==8 && string){memcpy(&size,(void*)(string+0x18),4);response=loader_script_call_json_for_runtime((const char*)(string+0x20),size,1);}
    if(!response)response="{\"ok\":false,\"error\":{\"code\":\"INVALID_NATIVE_CALL\"}}";
    @autoreleasepool {
        NSString *text=[[NSString alloc] initWithBytes:response length:strnlen(response,1048577) encoding:NSUTF8StringEncoding];
        if(!text)text=@"{\"ok\":false,\"error\":{\"code\":\"INVALID_NATIVE_RESPONSE\"}}";
        // JSON passed to Duktape is ASCII: preserves surrogate-pair semantics
        // across its CESU-8 strings and the native API's UTF-8 JSON boundary.
        NSMutableData *ascii=[NSMutableData dataWithCapacity:text.length];
        for(NSUInteger i=0;i<text.length;i++){
            unichar c=[text characterAtIndex:i];
            if(c<128){uint8_t byte=(uint8_t)c;[ascii appendBytes:&byte length:1];}
            else {char escaped[7];snprintf(escaped,sizeof escaped,"\\u%04x",c);[ascii appendBytes:escaped length:6];}
        }
        void *destination=pushBuffer(context,ascii.length);memcpy(destination,ascii.bytes,ascii.length);bufferToString(context);
    }
    return 1;
}
static BOOL digestMatches(uintptr_t address,size_t size,const char *expected) {
    unsigned char digest[CC_SHA256_DIGEST_LENGTH];CC_SHA256((const void*)address,(CC_LONG)size,digest);
    char encoded[65];for(unsigned i=0;i<32;i++)snprintf(encoded+2*i,3,"%02x",digest[i]);return !strcmp(encoded,expected);
}
#endif
int loader_javascript_gate_install(void) {
#if defined(__arm64__)
    if(original)return 0;
    if(!pthread_main_np())return -1;
    char executable[PROC_PIDPATHINFO_MAXSIZE]={0};proc_pidpath(getpid(),executable,sizeof executable);
    if(!strstr(executable,"/SV2 Extended.app/Contents/MacOS/synthv-studio-original") && !strstr(executable,"/SV2 Loader Probe.app/Contents/MacOS/synthv-studio-original"))return -1;
    uint32_t image=0;const struct mach_header_64 *header=NULL;
    for(uint32_t i=0;i<_dyld_image_count();i++){const struct mach_header *candidate=_dyld_get_image_header(i);if(candidate && candidate->filetype==MH_EXECUTE){image=i;header=(const void*)candidate;break;}}
    if(!header)return -1;
    const uint8_t expectedUUID[16]={0xac,0xfe,0xfc,0x17,0x25,0xc2,0x34,0xd5,0xb2,0x8a,0xe8,0xbf,0x6f,0xfb,0x32,0xb4};
    const struct load_command *command=(const void*)(header+1);BOOL uuidMatched=NO;
    for(uint32_t i=0;i<header->ncmds;i++){if(command->cmd==LC_UUID && command->cmdsize==sizeof(struct uuid_command))uuidMatched=!memcmp(((const struct uuid_command*)command)->uuid,expectedUUID,16);command=(const void*)((const char*)command+command->cmdsize);}
    if(!uuidMatched)return -1;
    intptr_t slide=_dyld_get_image_vmaddr_slide(image);
    Encoder *slot=(void*)(slide+0x1015c7300ULL);Encoder entry=(Encoder)(slide+0x101155e50ULL);
    if(*slot!=entry || !digestMatches((uintptr_t)entry,1680,"a4f786af084fa37f64baac32ec7de45782f5daf2b85d285c484b0b7e724046d8") || !digestMatches(slide+0x1010ed80cULL,268,"862e696fc7760f8b87ed39c19b5dcff0a0fc8a9ac57148402e9596f1e3b1bed8") || !digestMatches(slide+0x1010ed918ULL,508,"702710315b4feaf3824b0a1750f70c588d4d48fc25354e1dc5c6c8338765cf3c"))return -1;
    mach_vm_address_t region=(mach_vm_address_t)slot;mach_vm_size_t length=0;vm_region_basic_info_data_64_t info;mach_msg_type_number_t count=VM_REGION_BASIC_INFO_COUNT_64;mach_port_t object=MACH_PORT_NULL;
    kern_return_t result=mach_vm_region(mach_task_self(),&region,&length,VM_REGION_BASIC_INFO_64,(vm_region_info_t)&info,&count,&object);if(object!=MACH_PORT_NULL)mach_port_deallocate(mach_task_self(),object);
    if(result!=KERN_SUCCESS || (mach_vm_address_t)slot<region || (mach_vm_address_t)slot+sizeof *slot>region+length)return -1;
    mach_vm_address_t page=(mach_vm_address_t)slot & ~((mach_vm_address_t)vm_page_size-1);
    result=mach_vm_protect(mach_task_self(),page,vm_page_size,FALSE,info.protection|VM_PROT_WRITE|VM_PROT_COPY);if(result!=KERN_SUCCESS)return -1;
    original=entry;pushBuffer=(PushFixedBuffer)(slide+0x1010ed80cULL);bufferToString=(BufferToStringTop)(slide+0x1010ed918ULL);*slot=supported_encoder;
    result=mach_vm_protect(mach_task_self(),page,vm_page_size,FALSE,info.protection);
    if(result!=KERN_SUCCESS){*slot=entry;original=NULL;return -1;}
    fprintf(stderr,"JS_GATE installed pid=%d profile=sv2-2.3.0-arm64-duk-2.5.0\n",getpid());return 0;
#else
    return -1;
#endif
}

int loader_javascript_gate_ready(void) {
#if defined(__arm64__)
    return original!=NULL;
#else
    return 0;
#endif
}
