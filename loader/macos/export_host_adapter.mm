// Supported-build resident WAV export. Only scoped current-project callbacks
// touch native objects. Persistent jobs contain values and paths, never pointers.
#import <Foundation/Foundation.h>
#include <mach/mach.h>
#include <mach/mach_vm.h>
#include <mach-o/dyld.h>
#include <mach-o/loader.h>
#include <CommonCrypto/CommonDigest.h>
#include <libproc.h>
#include <stdatomic.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#if defined(__arm64__)
extern "C" void loader_export_return_trampoline(void);
extern "C" {__attribute__((visibility("hidden"))) void (*loader_export_original_target)()=nullptr;}
namespace {
struct Arguments {void *self;const void *values;int32_t count;};
using Getter=void(*)(); // Opaque entry, called only by the return veneer.
static Getter original;static intptr_t slide;
static NSString *captureToken,*salt,*guardToken,*guardProject;
static NSDictionary *operation,*captured;
static NSTimeInterval captureTime,guardTime;
static unsigned captures;
static NSMutableDictionary<NSString*,NSMutableDictionary*> *jobs;
static NSString *activeJob;
static NSDictionary *failure(NSString *code){return @{@"ok":@NO,@"error":@{@"code":code}};}
static NSDictionary *success(id value){return @{@"ok":@YES,@"result":value?:@{}};}
static BOOL readMemory(uintptr_t a,void *b,size_t n){mach_vm_size_t actual=0;return a>4096 && mach_vm_read_overwrite(mach_task_self(),a,n,(mach_vm_address_t)b,&actual)==KERN_SUCCESS && actual==n;}
static NSString *projectKey(uintptr_t p){NSString *value=[NSString stringWithFormat:@"%@:%llx:%@",salt,(unsigned long long)p,operation[@"projectContext"]];NSData *data=[value dataUsingEncoding:NSUTF8StringEncoding];unsigned char digest[32];CC_SHA256(data.bytes,(CC_LONG)data.length,digest);char key[65];for(unsigned i=0;i<32;i++)snprintf(key+2*i,3,"%02x",digest[i]);return [NSString stringWithUTF8String:key];}
static NSDictionary *publicJob(NSDictionary *job){NSMutableDictionary *result=[job mutableCopy];[result removeObjectForKey:@"projectKey"];return result;}
static BOOL terminal(NSString *state){return ![@[@"running",@"cancelling"] containsObject:state];}
static NSDictionary *wavInfo(NSString *path){
 NSFileHandle *file=[NSFileHandle fileHandleForReadingAtPath:path];if(!file)return nil;
 NSData *head=[file readDataOfLength:12];if(head.length!=12 || memcmp(head.bytes,"RIFF",4) || memcmp((const char*)head.bytes+8,"WAVE",4)){[file closeFile];return nil;}
 uint32_t rate=0;uint16_t channels=0,bits=0,format=0;uint64_t dataLength=0;unsigned chunks=0;
 while(chunks++<256){NSData *h=[file readDataOfLength:8];if(h.length!=8)break;const uint8_t *b=(const uint8_t*)h.bytes;uint32_t length=0;memcpy(&length,b+4,4);if(!memcmp(b,"fmt ",4) && length>=16 && length<4096){NSData *fmt=[file readDataOfLength:length];if(fmt.length!=length)break;const uint8_t *v=(const uint8_t*)fmt.bytes;memcpy(&format,v,2);memcpy(&channels,v+2,2);memcpy(&rate,v+4,4);memcpy(&bits,v+14,2);if(length&1)[file seekToFileOffset:file.offsetInFile+1];}
 else if(!memcmp(b,"data",4)){dataLength=length;break;}
 else {if(length>1073741824)break;[file seekToFileOffset:file.offsetInFile+length+(length&1)];}}
 uint64_t size=[NSFileManager.defaultManager attributesOfItemAtPath:path error:NULL].fileSize;uint64_t offset=file.offsetInFile;[file closeFile];
 if(!channels || channels>2 || !rate || !bits || (format!=1 && format!=3 && format!=0xfffe) || !dataLength || offset+dataLength>size)return nil;
 return @{@"channels":@(channels),@"sampleRate":@(rate),@"bitDepth":@(bits),@"audioBytes":@(dataLength),@"durationSeconds":@((double)dataLength/(rate*channels*(bits/8))),@"fileBytes":@(size)};
}
static void finish(NSMutableDictionary *job,uintptr_t renderer){
 NSString *state=job[@"state"];if(terminal(state))return;
 uint8_t cancelled=0;readMemory(renderer+0x2e4aa,&cancelled,1);if(cancelled)state=@"cancelling";
 if([state isEqual:@"cancelling"]){job[@"state"]=@"cancelled";return;}
 NSString *source=[job[@"stagingDirectory"] stringByAppendingPathComponent:@"render_MixDown.wav"];
 NSDictionary *audio=nil;@try{audio=wavInfo(source);}@catch(NSException *exception){(void)exception;}if(!audio || ![audio[@"channels"] isEqual:job[@"channels"]] || ![audio[@"sampleRate"] isEqual:job[@"sampleRate"]] || ![audio[@"bitDepth"] isEqual:job[@"bitDepth"]]){job[@"state"]=@"failed";job[@"error"]=@{@"code":@"INVALID_RENDER_OUTPUT"};return;}
 // Same-volume publication, atomically refusing an existing destination.
 if(renamex_np(source.fileSystemRepresentation,[job[@"outputPath"] fileSystemRepresentation],RENAME_EXCL)!=0){job[@"state"]=@"failed";job[@"error"]=@{@"code":errno==EEXIST?@"OUTPUT_EXISTS":@"OUTPUT_PUBLISH_FAILED"};return;}
 job[@"audio"]=audio;job[@"state"]=@"completed";
 [NSFileManager.defaultManager removeItemAtPath:job[@"stagingDirectory"] error:NULL];
}
static NSDictionary *perform(void *closure){
 uintptr_t context=0,root=0,project=0,owner=0,renderer=0,rendererOwner=0,rendererProject=0;
 if(!readMemory((uintptr_t)closure+8,&context,8) || !readMemory(context+8,&root,8) || !readMemory(root+0x2b50,&project,8) || !readMemory(root+0x2ab8,&owner,8) || !readMemory(owner+0x1b0,&renderer,8) || !readMemory(renderer+0x10,&rendererOwner,8) || !readMemory(rendererOwner+0x1c0,&rendererProject,8) || rendererProject!=project)return failure(@"PROJECT_RENDERER_MISMATCH");
 NSString *key=projectKey(project);uint8_t busy=0,waiting=0;if(!readMemory(renderer+0x2e4a8,&busy,1) || !readMemory(renderer+0x2e4a9,&waiting,1))return failure(@"RENDER_STATE_UNREADABLE");
 NSString *kind=operation[@"operation"];
 if([kind isEqual:@"snapshot"]){guardToken=NSUUID.UUID.UUIDString;guardProject=key;guardTime=NSProcessInfo.processInfo.systemUptime;return success(@{@"guard":guardToken,@"guardLifetimeSeconds":@15,@"busy":@(busy!=0),@"pending":@(waiting!=0),@"formats":@[@"wav"],@"mixdown":@YES});}
 NSDictionary *args=operation[@"arguments"];
 if([kind isEqual:@"start"]){
  if(![args[@"guard"] isEqual:guardToken] || ![key isEqual:guardProject] || NSProcessInfo.processInfo.systemUptime-guardTime>15)return failure(@"STALE_EXPORT_GUARD");
  if(!busy && !waiting && activeJob && !terminal(jobs[activeJob][@"state"]) && [jobs[activeJob][@"projectKey"] isEqual:key])finish(jobs[activeJob],renderer);
  if(busy || (activeJob && !terminal(jobs[activeJob][@"state"])))return failure(@"RENDER_BUSY");
  NSString *path=args[@"outputPath"];if([NSFileManager.defaultManager fileExistsAtPath:path])return failure(@"OUTPUT_EXISTS");
  NSString *parent=path.stringByDeletingLastPathComponent;BOOL directory=NO;if(![NSFileManager.defaultManager fileExistsAtPath:parent isDirectory:&directory] || !directory)return failure(@"OUTPUT_DIRECTORY_MISSING");
  NSString *identifier=NSUUID.UUID.UUIDString,*stage=[parent stringByAppendingPathComponent:[@".svext-export-" stringByAppendingString:identifier]];
  if(![NSFileManager.defaultManager createDirectoryAtPath:stage withIntermediateDirectories:NO attributes:@{NSFilePosixPermissions:@0700} error:NULL])return failure(@"OUTPUT_DIRECTORY_UNWRITABLE");
  uint8_t before[0x28];if(!readMemory(project+0x100,before,sizeof before)){[NSFileManager.defaultManager removeItemAtPath:stage error:NULL];return failure(@"RENDER_CONFIG_UNREADABLE");}
  uintptr_t destination=((uintptr_t(*)(const char*))(slide+0x100662a08ULL))(stage.UTF8String),filename=((uintptr_t(*)(const char*))(slide+0x100662a08ULL))("render");
  if(!destination || !filename){if(destination)((void(*)(void*))(slide+0x100699298ULL))(&destination);if(filename)((void(*)(void*))(slide+0x100699298ULL))(&filename);[NSFileManager.defaultManager removeItemAtPath:stage error:NULL];return failure(@"NATIVE_STRING_UNAVAILABLE");}
  uint8_t temporary[0x28];memcpy(temporary,before,sizeof temporary);memcpy(temporary,&destination,8);memcpy(temporary+8,&filename,8);
  int32_t channels=[args[@"channels"] intValue],aspiration=0,bits=[args[@"bitDepth"] intValue],rate=[args[@"sampleRate"] intValue];memcpy(temporary+0x10,&channels,4);memcpy(temporary+0x14,&aspiration,4);memcpy(temporary+0x18,&bits,4);memcpy(temporary+0x1c,&rate,4);temporary[0x20]=1;temporary[0x21]=0;
  // Native prepare synchronously captures paths/configuration before start returns.
  // Original project configuration and String ownership are restored in this scope.
  memcpy((void*)(project+0x100),temporary,sizeof temporary);
  bool started=((bool(*)(void*))(slide+0x100b99158ULL))((void*)renderer);
  memcpy((void*)(project+0x100),before,sizeof before);
  ((void(*)(void*))(slide+0x100699298ULL))(&destination);((void(*)(void*))(slide+0x100699298ULL))(&filename);
  guardToken=nil;
  if(!started){[NSFileManager.defaultManager removeItemAtPath:stage error:NULL];return failure(@"RENDER_START_FAILED");}
  NSMutableDictionary *job=[@{@"jobId":identifier,@"projectKey":key,@"state":@"running",@"outputPath":path,@"stagingDirectory":stage,@"configRestored":@YES,@"channels":@(channels),@"sampleRate":@(rate),@"bitDepth":@(bits)} mutableCopy];
  jobs[identifier]=job;activeJob=identifier;return success(publicJob(job));
 }
 NSMutableDictionary *job=jobs[args[@"jobId"]];if(!job)return failure(@"EXPORT_JOB_NOT_FOUND");
 if(terminal(job[@"state"]))return success(publicJob(job));
 if(![key isEqual:job[@"projectKey"]]){job[@"state"]=@"failed";job[@"error"]=@{@"code":@"EXPORT_PROJECT_CHANGED"};return success(publicJob(job));}
 if(!busy && !waiting){finish(job,renderer);return success(publicJob(job));}
 if([kind isEqual:@"cancel"]){
  // Match the official cancel action's release store. Do not block the UI waiting.
  __atomic_store_n((uint8_t*)(renderer+0x2e4aa),1,__ATOMIC_RELEASE);job[@"state"]=@"cancelling";
 }
 return success(publicJob(job));
}
static void observeAfter(void *closure,const Arguments *arguments){if(NSThread.isMainThread && captureToken && arguments && arguments->count==0 && NSProcessInfo.processInfo.systemUptime-captureTime<=.5){captures++;if(captures==1)captured=perform(closure);}}
static BOOL install(){
 char path[PROC_PIDPATHINFO_MAXSIZE]={0};proc_pidpath(getpid(),path,sizeof path);if(!strstr(path,"/SV2 Extended.app/Contents/MacOS/synthv-studio-original") && !strstr(path,"/SV2 Loader Probe.app/Contents/MacOS/synthv-studio-original"))return NO;
 const struct mach_header_64 *header=nullptr;unsigned image=0;for(unsigned i=0;i<_dyld_image_count();i++){auto h=_dyld_get_image_header(i);if(h && h->filetype==MH_EXECUTE){header=(const struct mach_header_64*)h;image=i;break;}}if(!header)return NO;
 const uint8_t uuid[16]={0xac,0xfe,0xfc,0x17,0x25,0xc2,0x34,0xd5,0xb2,0x8a,0xe8,0xbf,0x6f,0xfb,0x32,0xb4};BOOL matched=NO;auto command=(const struct load_command*)(header+1);for(unsigned i=0;i<header->ncmds;i++){if(command->cmd==LC_UUID)matched=!memcmp(((const struct uuid_command*)command)->uuid,uuid,16);command=(const struct load_command*)((const char*)command+command->cmdsize);}if(!matched)return NO;slide=_dyld_get_image_vmaddr_slide(image);
 struct Check{uintptr_t address;size_t length;const char *hash;};Check checks[]={
{0x100c7306cULL,472,"61e83af54352962b89aa6d71a2a376b7f26b8aab7c608f4f618e406b36a70f09"},
{0x100b99158ULL,460,"6f4a859ec21caa39c74e59c0e358e5edab8c28974cc29adf7d121a95329b7551"},
{0x100b99324ULL,780,"75956fd3a087544d7f4c0aab7f169908256a4bee78946506d96988aace7455f6"},
{0x101011580ULL,4308,"3bc249ed643d4538fd1cdd1bb8e5a594c6dcc6abc0e78906ecbf3643fc715a67"},
{0x100662a08ULL,400,"9963a491a5f165175660f70d89ab96313f835d78533c254cb9bf55da99a6f147"},
{0x100699298ULL,44,"f79a7305b5ef0e3115ede6956943945e726c6ea451df8a01f2dc4312ac25af1c"},
 };for(auto c:checks){unsigned char digest[32];CC_SHA256((void*)(slide+c.address),(CC_LONG)c.length,digest);char text[65];for(unsigned i=0;i<32;i++)snprintf(text+2*i,3,"%02x",digest[i]);if(strcmp(text,c.hash))return NO;}
 auto slot=(Getter*)(slide+0x101597858ULL);Getter expected=(Getter)(slide+0x100c7306cULL);if(*slot!=expected)return NO;
 mach_vm_address_t region=(mach_vm_address_t)slot;mach_vm_size_t length=0;vm_region_basic_info_data_64_t info;mach_msg_type_number_t count=VM_REGION_BASIC_INFO_COUNT_64;mach_port_t object=MACH_PORT_NULL;auto rc=mach_vm_region(mach_task_self(),&region,&length,VM_REGION_BASIC_INFO_64,(vm_region_info_t)&info,&count,&object);if(object!=MACH_PORT_NULL)mach_port_deallocate(mach_task_self(),object);if(rc!=KERN_SUCCESS)return NO;
 auto page=(mach_vm_address_t)slot & ~((mach_vm_address_t)vm_page_size-1);if(mach_vm_protect(mach_task_self(),page,vm_page_size,FALSE,info.protection|VM_PROT_WRITE|VM_PROT_COPY)!=KERN_SUCCESS)return NO;
 original=expected;loader_export_original_target=expected;*slot=loader_export_return_trampoline;if(mach_vm_protect(mach_task_self(),page,vm_page_size,FALSE,info.protection)!=KERN_SUCCESS){*slot=expected;original=nullptr;loader_export_original_target=nullptr;return NO;}salt=NSUUID.UUID.UUIDString;jobs=[NSMutableDictionary dictionary];return YES;
}
}
extern "C" __attribute__((visibility("hidden"))) void loader_export_observe_after(void *closure,const void *arguments){
 try { @try {observeAfter(closure,(const Arguments*)arguments);}
       @catch(NSException *exception){(void)exception;captures=~0U;} }
 catch(...){captures=~0U;}
}
extern "C" int loader_export_host_install(void){return NSThread.isMainThread && (original || install())?0:-1;}
extern "C" NSDictionary *loader_export_call(NSString *method,NSDictionary *arguments){
 if(!NSThread.isMainThread)return failure(@"MAIN_THREAD_REQUIRED");if(![arguments isKindOfClass:NSDictionary.class])return failure(@"INVALID_ARGUMENT");
 if([method isEqual:@"export.native_status"])return success(@{@"available":@(original!=nullptr)});
 if(!original)return failure(@"EXPORT_NATIVE_UNAVAILABLE");
 if([method isEqual:@"export.capture.begin"]){
  if(captureToken && NSProcessInfo.processInfo.systemUptime-captureTime<=.5)return failure(@"EXPORT_CAPTURE_BUSY");
  NSString *kind=arguments[@"operation"];NSDictionary *args=arguments[@"arguments"];
  if(arguments.count!=3 || ![arguments[@"projectContext"] isKindOfClass:NSString.class] || [arguments[@"projectContext"] length]>16384 || ![@[@"snapshot",@"start",@"status",@"cancel"] containsObject:kind] || ![args isKindOfClass:NSDictionary.class])return failure(@"INVALID_ARGUMENT");
  if([kind isEqual:@"start"]){
   NSSet *keys=[NSSet setWithArray:@[@"guard",@"outputPath",@"channels",@"sampleRate",@"bitDepth"]];if(![[NSSet setWithArray:args.allKeys] isEqual:keys])return failure(@"INVALID_ARGUMENT");
   NSString *path=args[@"outputPath"];if(![path isKindOfClass:NSString.class] || !path.isAbsolutePath || ![path.pathExtension.lowercaseString isEqual:@"wav"] || path.length>4096 || [path rangeOfString:@"\0"].location!=NSNotFound || ![path isEqual:path.stringByStandardizingPath] || ![args[@"guard"] isKindOfClass:NSString.class])return failure(@"INVALID_ARGUMENT");
   for(NSString *key in @[@"channels",@"sampleRate",@"bitDepth"])if(![args[key] isKindOfClass:NSNumber.class] || CFGetTypeID((__bridge CFTypeRef)args[key])==CFBooleanGetTypeID())return failure(@"INVALID_ARGUMENT");
   if(![@[@1,@2] containsObject:args[@"channels"]] || ![@[@44100,@48000,@88200,@96000] containsObject:args[@"sampleRate"]] || ![@[@16,@24,@32] containsObject:args[@"bitDepth"]])return failure(@"INVALID_ARGUMENT");
   if(jobs.count>=32){for(NSString *identifier in jobs.allKeys){if(terminal(jobs[identifier][@"state"])) {[jobs removeObjectForKey:identifier];break;}}}
   if(jobs.count>=32)return failure(@"EXPORT_JOB_LIMIT");
  }else if([kind isEqual:@"snapshot"]){if(args.count)return failure(@"INVALID_ARGUMENT");}
  else if(args.count!=1 || ![args[@"jobId"] isKindOfClass:NSString.class])return failure(@"INVALID_ARGUMENT");
  operation=[arguments copy];captured=nil;captures=0;captureTime=NSProcessInfo.processInfo.systemUptime;captureToken=NSUUID.UUID.UUIDString;return success(@{@"token":captureToken});
 }
 if([method isEqual:@"export.capture.end"]){
  if(arguments.count!=1 || ![arguments[@"token"] isEqual:captureToken])return failure(@"EXPORT_CAPTURE_TOKEN_MISMATCH");
  NSDictionary *result=captures==1 && captured?captured:(NSProcessInfo.processInfo.systemUptime-captureTime>.5?failure(@"EXPORT_CAPTURE_EXPIRED"):failure(@"EXPORT_CAPTURE_COUNT"));
  captureToken=nil;operation=nil;captured=nil;return result?:failure(@"EXPORT_CAPTURE_MISSING");
 }
 return failure(@"UNSUPPORTED_HOST_CAPABILITY");
}
#else
extern "C" int loader_export_host_install(void){return -1;}
extern "C" NSDictionary *loader_export_call(NSString *method,NSDictionary *arguments){(void)method;(void)arguments;return @{@"ok":@NO,@"error":@{@"code":@"EXPORT_NATIVE_UNAVAILABLE"}};}
#endif
