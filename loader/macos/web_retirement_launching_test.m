// Exercise a renderer proxy which exists before WebKit has a PID/taskInfo.
// Without pool-count admission this case could acknowledge an empty PID list.
#import <Foundation/Foundation.h>
#import <WebKit/WebKit.h>
#import <objc/runtime.h>
#include "web_content_retirement.h"
#include <unistd.h>
static unsigned terminationCalls;
static BOOL launching=YES,done;
static NSTimeInterval started,completed;
static NSDictionary *receipt;
int network_gate_state(loader_network_state_v1 *state){*state=(loader_network_state_v1){.abi_version=1,.struct_size=sizeof *state,.generation=7,.requested_blocked=1};return 0;}
int loader_get_profile_lease_state(loader_profile_lease_state_v1 *state){*state=(loader_profile_lease_state_v1){.abi_version=1,.struct_size=sizeof *state,.host_pid=getpid(),.mode=LOADER_PROFILE_LEASE_EXCLUSIVE,.generation=3};return 0;}
@interface LaunchingPool:NSObject
- (size_t)_webProcessCount;
- (void)_terminateAllWebContentProcesses;
@end
@implementation LaunchingPool
- (size_t)_webProcessCount{return launching?1:0;}
- (void)_terminateAllWebContentProcesses{if(!terminationCalls++)dispatch_after(dispatch_time(DISPATCH_TIME_NOW,100*NSEC_PER_MSEC),dispatch_get_main_queue(),^{launching=NO;});}
@end
static void complete(const char *body,size_t length,void *context){(void)context;receipt=[NSJSONSerialization JSONObjectWithData:[NSData dataWithBytes:body length:length] options:0 error:NULL];completed=NSProcessInfo.processInfo.systemUptime;done=YES;}
int main(int argc,const char **argv){@autoreleasepool{
 if(argc!=2)return 64;Class cls=NSClassFromString(@"WKProcessPool");Method pools=class_getClassMethod(cls,NSSelectorFromString(@"_allProcessPoolsForTesting")),infos=class_getClassMethod(cls,NSSelectorFromString(@"_webContentProcessInfo"));if(!pools||!infos)return 2;
 LaunchingPool *pool=[LaunchingPool new];IMP originalPools=method_getImplementation(pools),originalInfos=method_getImplementation(infos);method_setImplementation(pools,imp_implementationWithBlock(^id(id object){(void)object;return @[pool];}));method_setImplementation(infos,imp_implementationWithBlock(^id(id object){(void)object;return @[];}));
 started=NSProcessInfo.processInfo.systemUptime;int code=loader_retire_host_web_renderers(7,complete,NULL);NSTimeInterval deadline=started+3;while(!done&&NSProcessInfo.processInfo.systemUptime<deadline)[NSRunLoop.mainRunLoop runUntilDate:[NSDate dateWithTimeIntervalSinceNow:.01]];
 method_setImplementation(pools,originalPools);method_setImplementation(infos,originalInfos);
 NSDictionary *checks=@{@"accepted":@(code==0),@"completionReturned":@(done),@"launchingProxyTerminatedDespiteNoPID":@(terminationCalls>0&&!launching),@"waitedForProxyAndStableEmpty":@(done&&completed-started>=.19),@"successfulRendererReceipt":receipt[@"ok"]?:@NO,@"pendingCountCleared":@(loader_web_retirement_pending()==0)};BOOL passed=YES;for(NSNumber *value in checks.allValues)passed&=value.boolValue;NSDictionary *result=@{@"checks":checks,@"passed":@(passed),@"scope":@"Production retirement coordinator with controlled WebKit pool providers. Tests launching/no-PID ordering, not actual process exit or whole-host coverage."};NSData *data=[NSJSONSerialization dataWithJSONObject:result options:NSJSONWritingPrettyPrinted error:NULL];[data writeToFile:@(argv[1]) atomically:YES];puts([[NSString alloc]initWithData:data encoding:NSUTF8StringEncoding].UTF8String);return passed?0:1;
}}
