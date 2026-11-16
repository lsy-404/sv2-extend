// Explicit native API, never part of the ordinary online/offline toggle.
// Retires this UI process's WebKit renderers (including workers in its pools).
// Does not delete website data or claim whole-host network quiescence.
#import <Foundation/Foundation.h>
#import <WebKit/WebKit.h>
#import <objc/message.h>
#import <objc/runtime.h>
#include "web_content_retirement.h"
#include "startup_policy.h"
#import "web_store_registry.h"
#include <libproc.h>
#include <errno.h>
#include <signal.h>
#include <stdatomic.h>
#include <unistd.h>
extern int network_gate_state(loader_network_state_v1*);
static _Atomic unsigned pendingCount;
static BOOL method(Class cls,SEL selector,char result){Method m=class_getInstanceMethod(cls,selector);if(!m)return NO;NSMethodSignature *s=[NSMethodSignature signatureWithObjCTypes:method_getTypeEncoding(m)];return s.numberOfArguments==2&&s.methodReturnType[0]==result;}
static BOOL classMethod(Class cls,SEL selector){return cls&&method(object_getClass(cls),selector,'@');}
int loader_web_retirement_supported(void){Class pool=NSClassFromString(@"WKProcessPool");return classMethod(pool,NSSelectorFromString(@"_allProcessPoolsForTesting"))&&classMethod(pool,NSSelectorFromString(@"_webContentProcessInfo"))&&method(pool,NSSelectorFromString(@"_terminateAllWebContentProcesses"),'v')&&method(pool,NSSelectorFromString(@"_webProcessCount"),@encode(size_t)[0]);}
int loader_web_context_retirement_supported(void){Class pool=NSClassFromString(@"WKProcessPool");Class store=WKWebsiteDataStore.class;return loader_web_retirement_supported()&&loader_web_store_tracking_supported()&&classMethod(pool,NSSelectorFromString(@"_networkingProcessInfo"))&&method(store,NSSelectorFromString(@"_networkProcessExists"),@encode(BOOL)[0])&&method(store,NSSelectorFromString(@"_networkProcessIdentifier"),'i')&&method(store,NSSelectorFromString(@"_terminateNetworkProcess"),'v');}
static NSDictionary *stamp(pid_t pid){struct proc_bsdinfo info={0};if(pid<=0||pid==getpid())return nil;if(proc_pidinfo(pid,PROC_PIDTBSDINFO,0,&info,sizeof info)!=sizeof info)return nil;if(info.pbi_pid!=(uint32_t)pid||info.pbi_uid!=getuid())return nil;return @{@"pid":@(pid),@"sec":@(info.pbi_start_tvsec),@"usec":@(info.pbi_start_tvusec)};}
static BOOL exited(NSDictionary *old){pid_t pid=[old[@"pid"] intValue];if(kill(pid,0)<0&&errno==ESRCH)return YES;NSDictionary *now=stamp(pid);return now&&(![now[@"sec"] isEqual:old[@"sec"]]||![now[@"usec"] isEqual:old[@"usec"]]);}
static NSString *key(NSDictionary *value){return [NSString stringWithFormat:@"%@:%@:%@",value[@"pid"],value[@"sec"],value[@"usec"]];}
static NSDictionary *snapshot(void){
 if(!loader_web_retirement_supported())return nil;Class cls=NSClassFromString(@"WKProcessPool");id pools=((id(*)(id,SEL))objc_msgSend)(cls,NSSelectorFromString(@"_allProcessPoolsForTesting")),infos=((id(*)(id,SEL))objc_msgSend)(cls,NSSelectorFromString(@"_webContentProcessInfo"));
 if(![pools isKindOfClass:NSArray.class]||![infos isKindOfClass:NSArray.class]||[pools count]>128||[infos count]>256)return nil;NSMutableArray *stamps=[NSMutableArray array];size_t processCount=0;for(id pool in pools){if(!method(object_getClass(pool),NSSelectorFromString(@"_terminateAllWebContentProcesses"),'v')||!method(object_getClass(pool),NSSelectorFromString(@"_webProcessCount"),@encode(size_t)[0]))return nil;size_t n=((size_t(*)(id,SEL))objc_msgSend)(pool,NSSelectorFromString(@"_webProcessCount"));if(n>256||processCount>256-n)return nil;processCount+=n;}
 for(id info in infos){if(!method(object_getClass(info),@selector(pid),'i'))return nil;pid_t pid=((pid_t(*)(id,SEL))objc_msgSend)(info,@selector(pid));NSDictionary *identity=stamp(pid);if(!identity){if(kill(pid,0)<0&&errno==ESRCH)continue;return nil;}[stamps addObject:identity];}
 return @{@"pools":pools,@"stamps":stamps,@"processCount":@(processCount)};
}
static NSDictionary *networkSnapshot(void){
 if(!loader_web_context_retirement_supported())return nil;NSArray *stores=loader_web_stores_snapshot();if(!stores||stores.count>256)return nil;
 id infos=((id(*)(id,SEL))objc_msgSend)(NSClassFromString(@"WKProcessPool"),NSSelectorFromString(@"_networkingProcessInfo"));if(![infos isKindOfClass:NSArray.class]||[infos count]>256)return nil;
 NSMutableArray *stamps=[NSMutableArray array],*liveStores=[NSMutableArray array];NSMutableSet *covered=[NSMutableSet set];
 for(id store in stores){if(!method(object_getClass(store),NSSelectorFromString(@"_networkProcessExists"),@encode(BOOL)[0])||!method(object_getClass(store),NSSelectorFromString(@"_networkProcessIdentifier"),'i')||!method(object_getClass(store),NSSelectorFromString(@"_terminateNetworkProcess"),'v'))return nil;
  if(!((BOOL(*)(id,SEL))objc_msgSend)(store,NSSelectorFromString(@"_networkProcessExists")))continue;
  [liveStores addObject:store]; // Includes a launching proxy without a PID.
  pid_t pid=((pid_t(*)(id,SEL))objc_msgSend)(store,NSSelectorFromString(@"_networkProcessIdentifier"));
  if(pid>0){[covered addObject:@(pid)];NSDictionary *identity=stamp(pid);if(identity)[stamps addObject:identity];else if(!(kill(pid,0)<0&&errno==ESRCH))return nil;}
 }
 for(id info in infos){if(!method(object_getClass(info),@selector(pid),'i'))return nil;pid_t pid=((pid_t(*)(id,SEL))objc_msgSend)(info,@selector(pid));if(pid<=0)continue;
  // Never acknowledge an unobserved native process, nor kill an arbitrary PID.
  if(![covered containsObject:@(pid)])return nil;NSDictionary *identity=stamp(pid);if(identity)[stamps addObject:identity];else if(!(kill(pid,0)<0&&errno==ESRCH))return nil;
 }
 return @{@"stores":liveStores,@"stamps":stamps,@"processCount":@([infos count]),@"proxyCount":@(liveStores.count)};
}
@interface WebRetirement:NSObject
@property uint64_t networkGeneration,leaseGeneration;
@property loader_json_completion completion;
@property void *context;
@property NSMutableDictionary<NSString*,NSDictionary*> *processes;
@property NSTimeInterval deadline,emptySince;
@property BOOL finished,includeNetwork;
@property NSMutableDictionary<NSString*,NSDictionary*> *networkProcesses;
- (void)poll;
@end
static WebRetirement *active;
@implementation WebRetirement
- (void)finish:(NSString*)error{
 if(self.finished)return;self.finished=YES;active=nil;
 NSMutableDictionary *receipt=[@{@"hostPid":@(getpid()),@"networkGeneration":@(self.networkGeneration),@"profileLeaseGeneration":@(self.leaseGeneration),@"renderersExited":@YES,@"retiredRendererCount":@(self.processes.count),@"websiteDataDeleted":@NO,@"wholeHostNetworkVerified":@NO} mutableCopy];
 if(self.includeNetwork){receipt[@"networkProcessesExited"]=@YES;receipt[@"retiredNetworkProcessCount"]=@(self.networkProcesses.count);receipt[@"contextScope"]=@"observed-stores-and-native-global-process-list";}
 NSDictionary *value=error?@{@"ok":@NO,@"error":@{@"code":error}}:@{@"ok":@YES,@"result":receipt};
 NSData *body=[NSJSONSerialization dataWithJSONObject:value options:NSJSONWritingSortedKeys error:NULL];
 @try{self.completion(body.bytes,body.length,self.context);}@finally{self.completion=NULL;self.context=NULL;atomic_fetch_sub(&pendingCount,1);}
}
- (void)poll{
 if(self.finished)return;
 loader_network_state_v1 network={.abi_version=1,.struct_size=sizeof network};loader_profile_lease_state_v1 lease={.abi_version=1,.struct_size=sizeof lease};
 if(network_gate_state(&network)!=0||!network.requested_blocked||network.generation!=self.networkGeneration){[self finish:@"STALE_NETWORK_GENERATION"];return;}
 if(loader_get_profile_lease_state(&lease)!=0||lease.host_pid!=getpid()||lease.mode!=LOADER_PROFILE_LEASE_EXCLUSIVE||lease.generation!=self.leaseGeneration){[self finish:@"PROFILE_LEASE_CHANGED"];return;}
 if(NSProcessInfo.processInfo.systemUptime>=self.deadline){[self finish:@"WEB_RENDERER_RETIREMENT_TIMEOUT"];return;}
 @try{
  NSDictionary *current=snapshot();if(!current){[self finish:@"WEB_RENDERER_OBSERVATION_UNAVAILABLE"];return;}
  for(NSDictionary *identity in current[@"stamps"])self.processes[key(identity)]=identity;
  if(self.processes.count>256){[self finish:@"WEB_RENDERER_LIMIT"];return;}
  NSDictionary *network=self.includeNetwork?networkSnapshot():nil;if(self.includeNetwork&&!network){[self finish:@"WEB_NETWORK_CONTEXT_OBSERVATION_UNAVAILABLE"];return;}
  for(NSDictionary *identity in network[@"stamps"])self.networkProcesses[key(identity)]=identity;
  if(self.networkProcesses.count>256){[self finish:@"WEB_NETWORK_PROCESS_LIMIT"];return;}
  BOOL oldGone=YES;for(NSDictionary *identity in self.processes.allValues)if(!exited(identity)){oldGone=NO;break;}
  for(NSDictionary *identity in self.networkProcesses.allValues)if(!exited(identity)){oldGone=NO;break;}
  BOOL networkEmpty=!self.includeNetwork||(![network[@"processCount"] unsignedLongLongValue]&&![network[@"proxyCount"] unsignedLongLongValue]);
  if(![current[@"stamps"] count]&&![current[@"processCount"] unsignedLongLongValue]&&networkEmpty&&oldGone){if(!self.emptySince)self.emptySince=NSProcessInfo.processInfo.systemUptime;if(NSProcessInfo.processInfo.systemUptime-self.emptySince>=.1){[self finish:nil];return;}}
  else{self.emptySince=0;for(id pool in current[@"pools"])((void(*)(id,SEL))objc_msgSend)(pool,NSSelectorFromString(@"_terminateAllWebContentProcesses"));for(id store in network[@"stores"])((void(*)(id,SEL))objc_msgSend)(store,NSSelectorFromString(@"_terminateNetworkProcess"));}
 }@catch(NSException *exception){(void)exception;[self finish:@"WEB_RENDERER_NATIVE_ERROR"];return;}
 dispatch_after(dispatch_time(DISPATCH_TIME_NOW,20*NSEC_PER_MSEC),dispatch_get_main_queue(),^{[self poll];});
}
@end
static int beginRetirement(BOOL includeNetwork,uint64_t expected,loader_json_completion complete,void *context){
 if(!NSThread.isMainThread||!complete)return LOADER_ERR_INVALID;if(active)return LOADER_ERR_PROFILE_BUSY;if(includeNetwork?!loader_web_context_retirement_supported():!loader_web_retirement_supported())return LOADER_ERR_NOT_SUPPORTED;
 loader_network_state_v1 network={.abi_version=1,.struct_size=sizeof network};if(network_gate_state(&network)!=0||!network.requested_blocked)return LOADER_ERR_INVALID;if(network.generation!=expected)return LOADER_ERR_STALE_GENERATION;
 loader_profile_lease_state_v1 lease={.abi_version=1,.struct_size=sizeof lease};if(loader_get_profile_lease_state(&lease)!=0||lease.host_pid!=getpid()||lease.mode!=LOADER_PROFILE_LEASE_EXCLUSIVE)return LOADER_ERR_PROFILE_BUSY;
 WebRetirement *operation=[WebRetirement new];operation.networkGeneration=expected;operation.leaseGeneration=lease.generation;operation.completion=complete;operation.context=context;operation.deadline=NSProcessInfo.processInfo.systemUptime+5;operation.processes=[NSMutableDictionary dictionary];operation.includeNetwork=includeNetwork;operation.networkProcesses=[NSMutableDictionary dictionary];active=operation;atomic_fetch_add(&pendingCount,1);dispatch_async(dispatch_get_main_queue(),^{[operation poll];});return 0;
}
int loader_retire_host_web_renderers(uint64_t expected,loader_json_completion complete,void *context){return beginRetirement(NO,expected,complete,context);}
int loader_retire_host_web_contexts(uint64_t expected,loader_json_completion complete,void *context){return beginRetirement(YES,expected,complete,context);}
uint32_t loader_web_retirement_pending(void){return atomic_load(&pendingCount);}
