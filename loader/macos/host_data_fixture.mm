// Disposable namespace ABI/path fixture; never packaged in a release.
#import <Foundation/Foundation.h>
#include "loader_api.h"
#include <mach-o/dyld.h>
#include <CommonCrypto/CommonDigest.h>
#include <mach/mach_vm.h>
#include <initializer_list>
#include <fcntl.h>
#include <unistd.h>
static intptr_t slide;
struct NativeFile{uintptr_t text;~NativeFile(){((void(*)(void*))(slide+0x100699298ULL))(&text);}};
static NSString *nativePath(void){NativeFile value=((NativeFile(*)())(slide+0x1009c76c8ULL))();NSMutableData *bytes=[NSMutableData data];for(unsigned i=0;i<4096;i++){char c;mach_vm_size_t actual=0;if(mach_vm_read_overwrite(mach_task_self(),value.text+i,1,(mach_vm_address_t)&c,&actual)!=KERN_SUCCESS || actual!=1)return nil;if(!c)return [[NSString alloc]initWithData:bytes encoding:NSUTF8StringEncoding];[bytes appendBytes:&c length:1];}return nil;}
extern "C" int loader_plugin_init_v1(const loader_api_v1 *api){
 if(!api || api->abi_version!=LOADER_ABI_V1 || api->struct_size<offsetof(loader_api_v1,host_data_directory)+sizeof(api->host_data_directory) || !api->host_data_directory || !NSThread.isMainThread)return 1;
 const char *raw=api->host_data_directory(),*root=getenv("OPEN_SV2_LOADER_DATA_DIR");if(!raw || !root)return 2;
 for(unsigned i=0;i<_dyld_image_count();i++)if(_dyld_get_image_header(i)->filetype==MH_EXECUTE){slide=_dyld_get_image_vmaddr_slide(i);break;}
 unsigned char digest[32];CC_SHA256((void*)(slide+0x100699298ULL),44,digest);char hex[65];for(unsigned i=0;i<32;i++)snprintf(hex+2*i,3,"%02x",digest[i]);if(strcmp(hex,"f79a7305b5ef0e3115ede6956943945e726c6ea451df8a01f2dc4312ac25af1c"))return 3;
 NSString *expected=[[NSString stringWithUTF8String:root] stringByAppendingPathComponent:@"host-home/Library/Application Support/Dreamtonics/Synthesizer V Studio 2"];
 BOOL matches=[nativePath() isEqual:expected],apiMatches=[[NSString stringWithUTF8String:raw] isEqual:expected];
 __block unsigned successes=0;dispatch_group_t group=dispatch_group_create();NSObject *lock=[NSObject new];
 for(unsigned i=0;i<8;i++)dispatch_group_async(group,dispatch_get_global_queue(QOS_CLASS_USER_INITIATED,0),^{@autoreleasepool{unsigned local=0;for(unsigned j=0;j<16;j++)if([nativePath() isEqual:expected])local++;@synchronized(lock){successes+=local;}}});
 dispatch_group_notify(group,dispatch_get_main_queue(),^{
  BOOL written=NO;const char *name=getenv("LOADER_HOST_DATA_SENTINEL");if(name && !strchr(name,'/') && matches && apiMatches){NSString *target=[nativePath() stringByAppendingPathComponent:[NSString stringWithUTF8String:name]];int fd=open(target.fileSystemRepresentation,O_WRONLY|O_CREAT|O_EXCL|O_NOFOLLOW,0600);if(fd>=0){const char body[]="resident namespace write";written=write(fd,body,sizeof(body)-1)==sizeof(body)-1 && fsync(fd)==0;close(fd);}}
  NSDictionary *checks=@{@"nativeResolvedDirectoryWriteSucceeded":@(written),@"namespaceCapabilityPresent":@((api->capabilities&LOADER_CAP_HOST_DATA_NAMESPACE)!=0),@"genericAPIPathMatches":@(apiMatches),@"nativeMainPathMatches":@(matches),@"worker128PathsMatch":@(successes==128),@"ordinaryHomePreserved":@(![NSHomeDirectory() isEqual:[[NSString stringWithUTF8String:root] stringByAppendingPathComponent:@"host-home"]]),@"actualHostPIDMatches":@(api->host_pid==getpid())};
  const char *output=getenv("LOADER_HOST_DATA_RESULT");if(output){NSData *data=[NSJSONSerialization dataWithJSONObject:@{@"checks":checks,@"pid":@(getpid()),@"namespaceDirectory":expected} options:0 error:NULL];[data writeToFile:[NSString stringWithUTF8String:output] options:NSDataWritingAtomic error:NULL];}
 });return 0;
}
