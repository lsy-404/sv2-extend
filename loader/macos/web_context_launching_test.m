// Controlled proxy ordering test, not actual WebKit process exit evidence.
#import <Foundation/Foundation.h>
#import <WebKit/WebKit.h>
#import <objc/runtime.h>
#import "web_store_registry.h"
#include "web_content_retirement.h"
#include <unistd.h>
static unsigned terminations,pidReads,replies;static BOOL launching=YES,done,unknown;static id store;
static NSTimeInterval started,completed;static NSDictionary *receipt;
BOOL loader_web_store_tracking_supported(void){return YES;}
NSArray<WKWebsiteDataStore*> *loader_web_stores_snapshot(void){return @[store];}
int network_gate_state(loader_network_state_v1 *s){*s=(loader_network_state_v1){.abi_version=1,.struct_size=sizeof *s,.generation=7,.requested_blocked=1};return 0;}
int loader_get_profile_lease_state(loader_profile_lease_state_v1 *s){*s=(loader_profile_lease_state_v1){.abi_version=1,.struct_size=sizeof *s,.host_pid=getpid(),.mode=LOADER_PROFILE_LEASE_EXCLUSIVE,.generation=3};return 0;}
@interface EmptyPool:NSObject
- (size_t)_webProcessCount;
- (void)_terminateAllWebContentProcesses;
@end
@implementation EmptyPool
- (size_t)_webProcessCount{return 0;}
- (void)_terminateAllWebContentProcesses{}
@end
@interface LaunchingStore:NSObject
- (BOOL)_networkProcessExists;
- (pid_t)_networkProcessIdentifier;
- (void)_terminateNetworkProcess;
@end
@implementation LaunchingStore
- (BOOL)_networkProcessExists{return launching;}
- (pid_t)_networkProcessIdentifier{pidReads++;return 0;}
- (void)_terminateNetworkProcess{if(!terminations++)dispatch_after(dispatch_time(DISPATCH_TIME_NOW,100*NSEC_PER_MSEC),dispatch_get_main_queue(),^{launching=NO;});}
@end
@interface UnobservedInfo:NSObject
- (pid_t)pid;
@end
@implementation UnobservedInfo
- (pid_t)pid{return getpid();} // Must never be sent to kill/termination machinery.
@end
static void complete(const char *body,size_t length,void *ctx){(void)ctx;receipt=[NSJSONSerialization JSONObjectWithData:[NSData dataWithBytes:body length:length] options:0 error:NULL];completed=NSProcessInfo.processInfo.systemUptime;replies++;done=YES;}
static void waitForCompletion(void){NSTimeInterval deadline=NSProcessInfo.processInfo.systemUptime+3;while(!done&&NSProcessInfo.processInfo.systemUptime<deadline)[NSRunLoop.mainRunLoop runUntilDate:[NSDate dateWithTimeIntervalSinceNow:.01]];}
int main(int argc,const char **argv){@autoreleasepool{
 if(argc!=2)return 64;Class cls=NSClassFromString(@"WKProcessPool");Method p=class_getClassMethod(cls,NSSelectorFromString(@"_allProcessPoolsForTesting")),r=class_getClassMethod(cls,NSSelectorFromString(@"_webContentProcessInfo")),n=class_getClassMethod(cls,NSSelectorFromString(@"_networkingProcessInfo"));if(!p||!r||!n)return 2;
 EmptyPool *pool=[EmptyPool new];UnobservedInfo *info=[UnobservedInfo new];store=[LaunchingStore new];IMP oldP=method_getImplementation(p),oldR=method_getImplementation(r),oldN=method_getImplementation(n);
 method_setImplementation(p,imp_implementationWithBlock(^id(id object){(void)object;return @[pool];}));method_setImplementation(r,imp_implementationWithBlock(^id(id object){(void)object;return @[];}));method_setImplementation(n,imp_implementationWithBlock(^id(id object){(void)object;return unknown?@[info]:@[];}));
 started=NSProcessInfo.processInfo.systemUptime;int code=loader_retire_host_web_contexts(7,complete,NULL);waitForCompletion();
 NSMutableDictionary *checks=[@{@"accepted":@(code==0),@"callbackOnce":@(done&&replies==1),@"launchingProxyWithoutPIDObserved":@(pidReads>0),@"launchingProxyTerminated":@(terminations>0&&!launching),@"waitedForProxyAndStableEmpty":@(done&&completed-started>=.19),@"networkReceiptSucceeded":@([receipt[@"ok"]boolValue]&&[receipt[@"result"][@"networkProcessesExited"]boolValue]),@"pendingCountCleared":@(loader_web_retirement_pending()==0)} mutableCopy];
 unknown=YES;done=NO;receipt=nil;unsigned previousTerminations=terminations;int unknownCode=loader_retire_host_web_contexts(7,complete,NULL);waitForCompletion();checks[@"unobservedProcessRefusesReceipt"]=@(unknownCode==0&&done&&![receipt[@"ok"]boolValue]&&[receipt[@"error"][@"code"]isEqual:@"WEB_NETWORK_CONTEXT_OBSERVATION_UNAVAILABLE"]);checks[@"unobservedProcessNotTerminated"]=@(terminations==previousTerminations);checks[@"failureCallbackOnceAndCountCleared"]=@(replies==2&&loader_web_retirement_pending()==0);
 method_setImplementation(p,oldP);method_setImplementation(r,oldR);method_setImplementation(n,oldN);BOOL passed=YES;for(NSNumber *value in checks.allValues)passed&=value.boolValue;NSDictionary *result=@{@"checks":checks,@"passed":@(passed),@"scope":@"Production coordinator with controlled empty PID list and launching proxy, plus unobserved process refusal. No actual process-exit or whole-host claim."};NSData *data=[NSJSONSerialization dataWithJSONObject:result options:NSJSONWritingPrettyPrinted error:NULL];[data writeToFile:@(argv[1]) atomically:YES];puts([[NSString alloc]initWithData:data encoding:NSUTF8StringEncoding].UTF8String);return passed?0:1;
}}
