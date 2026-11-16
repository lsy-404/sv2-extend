// Supported-build voice identity adapter. The original scripting getter and
// return ownership are preserved. Snapshot values only during a live call;
// never retain a model pointer or write the object's layout.
#import <Foundation/Foundation.h>
#import "voice_native_internal.h"
#include <mach/mach.h>
#include <mach/mach_vm.h>
#include <mach-o/dyld.h>
#include <mach-o/loader.h>
#include <libproc.h>
#include <unistd.h>
#include <pthread.h>
#include <CommonCrypto/CommonDigest.h>
#include <stdint.h>
#include <string.h>
#if defined(__arm64__)
struct NativeArguments {void *thisValue;const void *arguments;int32_t count;};
using VoiceFunction=void(*)(); // Opaque native entry; calls occur only in assembly.
extern "C" void loader_voice_return_trampoline(void);
extern "C" {__attribute__((visibility("hidden"))) VoiceFunction loader_voice_original_target=nullptr;}
static VoiceFunction original;
static intptr_t slide;
static NSString *captureToken;
static NSDictionary *capturedIdentity;
static NSTimeInterval captureStarted;
static unsigned captureCount;
static BOOL catalogReady;
static NSDictionary *selectionRequest,*selectionResult;
static NSString *readString(uintptr_t address) {
    if(!address)return nil;
    NSMutableData *data=[NSMutableData data];
    for(size_t i=0;i<512;i+=16){
        char bytes[16];mach_vm_size_t actual=0;
        if(mach_vm_read_overwrite(mach_task_self(),address+i,16,(mach_vm_address_t)bytes,&actual)!=KERN_SUCCESS || actual!=16)return nil;
        for(unsigned j=0;j<16;j++){if(!bytes[j])return [[NSString alloc] initWithData:data encoding:NSUTF8StringEncoding];[data appendBytes:bytes+j length:1];}
    }
    return nil;
}

static bool readMemory(uintptr_t address,void *bytes,size_t length) {
    mach_vm_size_t actual=0;
    return address>4096 && mach_vm_read_overwrite(mach_task_self(),address,length,(mach_vm_address_t)bytes,&actual)==KERN_SUCCESS && actual==length;
}
static void *cast(void *value,uintptr_t source,uintptr_t target) {
    if(!value)return nullptr;
    return ((void*(*)(const void*,const void*,const void*,ptrdiff_t))(slide+0x10119772cULL))(value,(const void*)(slide+source),(const void*)(slide+target),0);
}
static NSString *hashObject(id value) {
    NSData *encoded=[NSJSONSerialization dataWithJSONObject:value options:NSJSONWritingSortedKeys error:NULL];unsigned char digest[32];CC_SHA256(encoded.bytes,(CC_LONG)encoded.length,digest);char key[65];for(unsigned i=0;i<32;i++)snprintf(key+2*i,3,"%02x",digest[i]);return [NSString stringWithUTF8String:key];
}
NSDictionary *loader_voice_identity(NSArray<NSString*> *fields) {
    if(fields.count!=5)return nil;
    NSArray *keys=@[@"name",@"language",@"languageOverride",@"backendType",@"version"];
    NSMutableDictionary *identity=[NSMutableDictionary dictionary];for(unsigned i=0;i<5;i++)identity[keys[i]]=fields[i];
    identity[@"assigned"]=[identity[@"name"] length]?@YES:@NO;identity[@"source"]=@"native-reference";
    identity[@"identityKey"]=hashObject(identity);return identity;
}
NSDictionary *loader_voice_reference_identity(void *reference) {
    if(!NSThread.isMainThread || !reference)return nil;
    NSMutableArray *fields=[NSMutableArray array];
    for(unsigned i=0;i<5;i++){uintptr_t pointer=0;if(!readMemory((uintptr_t)reference+0x80+i*8,&pointer,8))return nil;NSString *value=readString(pointer);if(!value)return nil;[fields addObject:value];}
    NSMutableDictionary *identity=[loader_voice_identity(fields) mutableCopy];
    // Per-process opaque reference identity: no model address crosses the API.
    static NSString *salt;if(!salt)salt=NSUUID.UUID.UUIDString;
    identity[@"referenceKey"]=hashObject(@[salt,[NSString stringWithFormat:@"%llx",(unsigned long long)(uintptr_t)reference]]);return identity;
}
static void observeAfter(void *closure,const NativeArguments *arguments) {
    (void)closure;
    if(!pthread_main_np() || !captureToken || NSProcessInfo.processInfo.systemUptime-captureStarted>.5 || !arguments || arguments->count!=0 || !arguments->thisValue)return;
    uintptr_t type=0,getObject=0;
    if(!readMemory((uintptr_t)arguments->thisValue,&type,8) || !readMemory(type+0x38,&getObject,8))return;
    void *object=((void*(*)(void*))getObject)((char*)arguments->thisValue+8);
    object=cast(object,0x1015529d0ULL,0x10156bda8ULL);
    if(!object)return;
    void *nested=((void*(*)(void*))(slide+0x100c5ff00ULL))(object);
    void *reference=cast(nested,0x1015867e0ULL,0x1015a1df0ULL);
    if(!reference)return;
    captureCount++;
    NSDictionary *identity=loader_voice_reference_identity(reference);
    if(selectionRequest){
        if(captureCount!=1)return;
        if(![identity[@"referenceKey"] isEqual:selectionRequest[@"expectedReferenceKey"]] || ![identity[@"identityKey"] isEqual:selectionRequest[@"expectedIdentityKey"]])selectionResult=@{@"ok":@NO,@"error":@{@"code":@"STALE_VOICE_REFERENCE"}};
        else selectionResult=loader_voice_catalog_request(selectionRequest[@"voicebankId"],reference);
    } else capturedIdentity=identity;
    return;
}
// A post-observer must never unwind past an already constructed native result.
extern "C" __attribute__((visibility("hidden"))) void loader_voice_observe_after(void *closure,const NativeArguments *arguments) {
    try { @try { observeAfter(closure,arguments); }
          @catch(NSException *exception) { (void)exception;captureCount=~0U; } }
    catch(...) { captureCount=~0U; }
}
extern "C" int loader_voice_host_install(void) {
    if(!pthread_main_np())return -1;
    if(original)return 0;
    char path[PROC_PIDPATHINFO_MAXSIZE]={0};proc_pidpath(getpid(),path,sizeof path);
    if(!strstr(path,"/SV2 Extended.app/Contents/MacOS/synthv-studio-original") && !strstr(path,"/SV2 Loader Probe.app/Contents/MacOS/synthv-studio-original"))return -1;
    int status=-1;
    do {
        const struct mach_header_64 *header=nullptr;uint32_t image=0;
        for(uint32_t i=0;i<_dyld_image_count();i++){const struct mach_header *h=_dyld_get_image_header(i);if(h && h->filetype==MH_EXECUTE){header=(const struct mach_header_64*)h;image=i;break;}}
        if(!header)break;
        const uint8_t uuid[16]={0xac,0xfe,0xfc,0x17,0x25,0xc2,0x34,0xd5,0xb2,0x8a,0xe8,0xbf,0x6f,0xfb,0x32,0xb4};bool matched=false;
        const struct load_command *command=(const struct load_command*)(header+1);
        for(uint32_t i=0;i<header->ncmds;i++){if(command->cmd==LC_UUID && command->cmdsize==sizeof(struct uuid_command))matched=!memcmp(((const struct uuid_command*)command)->uuid,uuid,16);command=(const struct load_command*)((const char*)command+command->cmdsize);}
        if(!matched)break;slide=_dyld_get_image_vmaddr_slide(image);
        VoiceFunction *slot=(VoiceFunction*)(slide+0x10159bb58ULL);VoiceFunction expected=(VoiceFunction)(slide+0x100ca7754ULL);
        struct Check {uintptr_t address;size_t length;const char *hash;};
        Check checks[]={{0x100ca7754ULL,5884,"0ad7c7f2696c862fc2722a947fcd9910ffef8052dbbb3c89909763a0d466dc36"},{0x100c5ff00ULL,632,"be907822bf7579e3779427ff4265c30c7bc31a5d4e5c155e92bcd7e429fde297"}};
        bool valid=*slot==expected;
        for(const Check &check:checks){unsigned char digest[32];CC_SHA256((const void*)(slide+check.address),(CC_LONG)check.length,digest);char text[65];for(unsigned i=0;i<32;i++)snprintf(text+2*i,3,"%02x",digest[i]);if(strcmp(text,check.hash))valid=false;}
        if(!valid)break;
        mach_vm_address_t region=(mach_vm_address_t)slot;mach_vm_size_t length=0;vm_region_basic_info_data_64_t info;mach_msg_type_number_t count=VM_REGION_BASIC_INFO_COUNT_64;mach_port_t object=MACH_PORT_NULL;
        kern_return_t rc=mach_vm_region(mach_task_self(),&region,&length,VM_REGION_BASIC_INFO_64,(vm_region_info_t)&info,&count,&object);if(object!=MACH_PORT_NULL)mach_port_deallocate(mach_task_self(),object);
        if(rc!=KERN_SUCCESS || (mach_vm_address_t)slot<region || (mach_vm_address_t)slot+8>region+length)break;
        mach_vm_address_t page=(mach_vm_address_t)slot & ~((mach_vm_address_t)vm_page_size-1);
        if(mach_vm_protect(mach_task_self(),page,vm_page_size,FALSE,info.protection|VM_PROT_WRITE|VM_PROT_COPY)!=KERN_SUCCESS)break;
        original=expected;loader_voice_original_target=expected;*slot=loader_voice_return_trampoline;
        if(mach_vm_protect(mach_task_self(),page,vm_page_size,FALSE,info.protection)!=KERN_SUCCESS){*slot=expected;original=nullptr;loader_voice_original_target=nullptr;break;}
        catalogReady=loader_voice_catalog_initialize(slide);
        fprintf(stderr,"VOICE_HOST identity-adapter-installed catalog=%d\n",catalogReady);status=0;
    } while(false);
    return status;
}
static NSDictionary *failure(NSString *code) {return @{@"ok":@NO,@"error":@{@"code":code}};}
static BOOL key(id value){return [value isKindOfClass:NSString.class] && [value rangeOfString:@"^[0-9a-f]{64}$" options:NSRegularExpressionSearch].location!=NSNotFound;}
extern "C" NSDictionary *loader_voice_call(NSString *method,NSDictionary *arguments) {
    if(!NSThread.isMainThread)return failure(@"MAIN_THREAD_REQUIRED");
    if([method isEqualToString:@"voice.native_status"])return @{@"ok":@YES,@"result":@{@"identityReadable":(original?@YES:@NO),@"catalogReadable":@(catalogReady),@"selectable":@(catalogReady)}};
    if(!original)return failure(@"VOICE_NATIVE_ADAPTER_UNAVAILABLE");
    if(![arguments isKindOfClass:NSDictionary.class])return failure(@"INVALID_ARGUMENT");
    if([method isEqualToString:@"voice.catalog"]){if(arguments.count)return failure(@"INVALID_ARGUMENT");return loader_voice_catalog_request(nil,nullptr);}
    BOOL selecting=[method isEqualToString:@"voice.select.begin"];
    if([method isEqualToString:@"voice.select.validate"] || selecting){
        if(!catalogReady)return failure(@"VOICE_CATALOG_UNAVAILABLE");
        if(!key(arguments[@"voicebankId"]) || arguments.count!=(selecting?3:1) || (selecting && (!key(arguments[@"expectedIdentityKey"]) || !key(arguments[@"expectedReferenceKey"]))))return failure(@"INVALID_ARGUMENT");
        NSDictionary *target=loader_voice_catalog_request(arguments[@"voicebankId"],nullptr);if(![target[@"ok"] boolValue] || !selecting)return target;
    }
    if([method isEqualToString:@"voice.capture.begin"] || selecting){
        if(!selecting && arguments.count)return failure(@"INVALID_ARGUMENT");
        if(captureToken && NSProcessInfo.processInfo.systemUptime-captureStarted<=.5)return failure(@"VOICE_CAPTURE_BUSY");
        captureToken=NSUUID.UUID.UUIDString;capturedIdentity=nil;selectionResult=nil;selectionRequest=selecting?[arguments copy]:nil;captureCount=0;captureStarted=NSProcessInfo.processInfo.systemUptime;
        return @{@"ok":@YES,@"result":@{@"token":captureToken}};
    }
    BOOL selectionEnd=[method isEqualToString:@"voice.select.end"];
    if([method isEqualToString:@"voice.capture.end"] || selectionEnd){
        if(arguments.count!=1 || ![arguments[@"token"] isKindOfClass:NSString.class] || ![arguments[@"token"] isEqualToString:captureToken])return failure(@"VOICE_CAPTURE_TOKEN_MISMATCH");
        if(selectionEnd!=(selectionRequest!=nil))return failure(@"VOICE_CAPTURE_MODE_MISMATCH");
        BOOL valid=(selectionEnd?selectionResult!=nil:capturedIdentity!=nil) && captureCount==1 && (selectionEnd || NSProcessInfo.processInfo.systemUptime-captureStarted<=.5);
        NSDictionary *response=selectionEnd?selectionResult:@{@"ok":@YES,@"result":capturedIdentity?:@{}};
        captureToken=nil;capturedIdentity=nil;selectionRequest=nil;selectionResult=nil;
        return valid?response:failure(@"VOICE_CAPTURE_MISSING_OR_AMBIGUOUS");
    }
    return failure(@"UNSUPPORTED_HOST_CAPABILITY");
}

#else
extern "C" int loader_voice_host_install(void) {return -1;}
extern "C" NSDictionary *loader_voice_call(NSString *method,NSDictionary *arguments) {
    (void)arguments;
    if([method isEqualToString:@"voice.native_status"])return @{@"ok":@YES,@"result":@{@"identityReadable":@NO}};
    return @{@"ok":@NO,@"error":@{@"code":@"VOICE_NATIVE_ADAPTER_UNAVAILABLE"}};
}
#endif
