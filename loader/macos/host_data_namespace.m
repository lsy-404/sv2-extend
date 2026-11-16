// Scoped native application-data namespace. No account/session parsing or reload.
#import <Foundation/Foundation.h>
#include <CommonCrypto/CommonDigest.h>
#include <mach-o/dyld.h>
#include <mach-o/loader.h>
#include <execinfo.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <limits.h>
#include <unistd.h>
#include <string.h>
static NSString *mappedHome;
static char *productDirectory;
static intptr_t hostSlide;
static atomic_bool enabled;
static _Thread_local bool scanning;
static NSString *scopedHome(void){
    if(!atomic_load_explicit(&enabled,memory_order_acquire) || scanning)return NSHomeDirectory();
    scanning=true;void *stack[64];int count=backtrace(stack,64);bool match=false;
    for(int i=0;i<count;i++){uintptr_t pc=(uintptr_t)stack[i]-(uintptr_t)hostSlide;if((pc>=0x1009c76c8ULL && pc<0x1009c77dcULL)||(pc>=0x1009c77dcULL && pc<0x1009c7948ULL)){match=true;break;}}
    scanning=false;return match?mappedHome:NSHomeDirectory();
}
__attribute__((used)) static struct{const void *replacement,*original;} homeInterpose __attribute__((section("__DATA,__interpose")))={(const void*)scopedHome,(const void*)NSHomeDirectory};
const char *loader_host_data_directory(void){return atomic_load_explicit(&enabled,memory_order_acquire)?productDirectory:NULL;}
static BOOL profile(void){
#if defined(__arm64__)
    const struct mach_header_64 *header=NULL;unsigned index=0;for(unsigned i=0;i<_dyld_image_count();i++){const struct mach_header *h=_dyld_get_image_header(i);if(h && h->filetype==MH_EXECUTE){header=(const void*)h;index=i;break;}}if(!header)return NO;
    const uint8_t expected[16]={0xac,0xfe,0xfc,0x17,0x25,0xc2,0x34,0xd5,0xb2,0x8a,0xe8,0xbf,0x6f,0xfb,0x32,0xb4};BOOL match=NO;const struct load_command *command=(const void*)(header+1);
    for(unsigned i=0;i<header->ncmds;i++){if(command->cmd==LC_UUID)match=!memcmp(((const struct uuid_command*)command)->uuid,expected,16);command=(const void*)((const char*)command+command->cmdsize);}if(!match)return NO;hostSlide=_dyld_get_image_vmaddr_slide(index);
    struct Check{uintptr_t address;size_t length;const char *hash;} checks[]={
        {0x1009c76c8ULL,276,"b575f43df42b84766a54b4cd6af4f22eb95ce2003d59d97f77be23a578bdc72c"},
        {0x1009c77dcULL,364,"977becac6ddc7f120bb8e5fbfd15fbe6ab8c0f347e92566b5b7b9ffc6652bb4c"}};
    for(unsigned i=0;i<sizeof checks/sizeof checks[0];i++){unsigned char digest[32];CC_SHA256((void*)(hostSlide+checks[i].address),(CC_LONG)checks[i].length,digest);char hex[65];for(unsigned j=0;j<32;j++)snprintf(hex+j*2,3,"%02x",digest[j]);if(strcmp(hex,checks[i].hash))return NO;}return YES;
#else
    return NO;
#endif
}
int loader_host_data_install_from_environment(void){
    const char *flag=getenv("OPEN_SV2_ISOLATE_HOST_DATA");if(!flag || strcmp(flag,"1"))return 0;
    char executable[PATH_MAX];uint32_t size=sizeof executable;if(_NSGetExecutablePath(executable,&size)!=0)return -1;const char *name=strrchr(executable,'/');if(!name || strcmp(name+1,"synthv-studio-original") || !profile())return -1;
    const char *root=getenv("OPEN_SV2_LOADER_DATA_DIR"),*home=getenv("OPEN_SV2_HOST_DATA_HOME");if(!root || !home)return -1;
    @autoreleasepool {
        NSString *rawRoot=[NSString stringWithUTF8String:root],*rawHome=[NSString stringWithUTF8String:home];if(!rawRoot.isAbsolutePath || !rawHome.isAbsolutePath)return -1;
        NSString *canonicalRoot=rawRoot.stringByStandardizingPath.stringByResolvingSymlinksInPath,*canonicalHome=rawHome.stringByStandardizingPath.stringByResolvingSymlinksInPath;
        id bound=NSBundle.mainBundle.infoDictionary[@"OpenSV2LoaderDataDirectory"];
        if(bound && (![bound isEqual:canonicalRoot] || ![NSBundle.mainBundle.infoDictionary[@"OpenSV2HostDataNamespace"] isEqual:@YES]))return -1;
        if(![canonicalHome isEqual:[canonicalRoot stringByAppendingPathComponent:@"host-home"]])return -1;
        BOOL directory=NO;if(![NSFileManager.defaultManager fileExistsAtPath:canonicalHome isDirectory:&directory] || !directory)return -1;
        NSData *data=[NSData dataWithContentsOfFile:[canonicalHome stringByAppendingPathComponent:@".loader-host-data.json"]];id marker=data?[NSJSONSerialization JSONObjectWithData:data options:0 error:NULL]:nil;
        if(![marker isKindOfClass:NSDictionary.class] || ![marker[@"kind"] isEqual:@"OpenSV2.host-data.v1"] || ![marker[@"schema"] isKindOfClass:NSNumber.class] || [marker[@"schema"] integerValue]!=1)return -1;
        NSString *product=[canonicalHome stringByAppendingPathComponent:@"Library/Application Support/Dreamtonics/Synthesizer V Studio 2"];
        if(![product.stringByResolvingSymlinksInPath isEqual:product] || ![NSFileManager.defaultManager fileExistsAtPath:product isDirectory:&directory] || !directory)return -1;
        __block BOOL valid=YES;
        NSDirectoryEnumerator *entries=[NSFileManager.defaultManager enumeratorAtURL:[NSURL fileURLWithPath:canonicalHome] includingPropertiesForKeys:@[NSURLIsSymbolicLinkKey] options:0 errorHandler:^BOOL(NSURL *url,NSError *error){(void)url;(void)error;valid=NO;return NO;}];
        if(!entries)return -1;
        for(NSURL *entry in entries){NSNumber *link=nil;NSError *error=nil;if(![entry getResourceValue:&link forKey:NSURLIsSymbolicLinkKey error:&error] || link.boolValue)return -1;}
        if(!valid)return -1;
        mappedHome=[canonicalHome copy];productDirectory=strdup(product.fileSystemRepresentation);if(!productDirectory)return -1;atomic_store_explicit(&enabled,true,memory_order_release);
    }
    fprintf(stderr,"HOST_DATA_NAMESPACE enabled=1 profile=sv2-2.3.0-arm64\n");return 0;
}
