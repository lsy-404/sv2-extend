#import <Foundation/Foundation.h>
#include "loader_api.h"
#include <stdatomic.h>
#include <dlfcn.h>
static const loader_api_v1 *loader;
@interface BackgroundProbe:NSObject<NSURLSessionDownloadDelegate>
@property dispatch_semaphore_t complete;
@property dispatch_semaphore_t invalidated;
@property dispatch_semaphore_t progress;
@property BOOL progressed;
@property NSInteger error;
@property BOOL downloaded;
@end
@implementation BackgroundProbe
-(void)URLSession:(NSURLSession*)session downloadTask:(NSURLSessionDownloadTask*)task didWriteData:(int64_t)bytes totalBytesWritten:(int64_t)total totalBytesExpectedToWrite:(int64_t)expected{(void)session;(void)task;(void)bytes;(void)total;(void)expected;if(!self.progressed){self.progressed=YES;dispatch_semaphore_signal(self.progress);}}
-(void)URLSession:(NSURLSession*)session downloadTask:(NSURLSessionDownloadTask*)task didFinishDownloadingToURL:(NSURL*)location{(void)session;(void)task;self.downloaded=[NSData dataWithContentsOfURL:location].length>0;}
-(void)URLSession:(NSURLSession*)session task:(NSURLSessionTask*)task didCompleteWithError:(NSError*)error{(void)session;(void)task;self.error=error.code;dispatch_semaphore_signal(self.complete);}
-(void)URLSession:(NSURLSession*)session didBecomeInvalidWithError:(NSError*)error{(void)session;(void)error;dispatch_semaphore_signal(self.invalidated);}
@end


@interface CompletionHold:NSObject
@property dispatch_semaphore_t entered;
@property dispatch_semaphore_t releaseGate;
@property dispatch_semaphore_t returned;
@property BOOL bodyValid;
@end
@implementation CompletionHold
@end
static NSDictionary *blockCase(void){
 loader->set_host_network_blocked(0);CompletionHold *hold=[CompletionHold new];hold.entered=dispatch_semaphore_create(0);hold.releaseGate=dispatch_semaphore_create(0);hold.returned=dispatch_semaphore_create(0);NSOperationQueue *queue=[NSOperationQueue new];queue.maxConcurrentOperationCount=1;queue.suspended=YES;
 NSURLSession *session=[NSURLSession sessionWithConfiguration:NSURLSessionConfiguration.ephemeralSessionConfiguration delegate:nil delegateQueue:queue];NSString *base=[NSString stringWithUTF8String:getenv("LOADER_BACKGROUND_BASE")];NSURLSessionDataTask *task=[session dataTaskWithURL:[NSURL URLWithString:[base stringByAppendingString:@"/completion-held"]] completionHandler:^(NSData *data,NSURLResponse *response,NSError *error){(void)response;hold.bodyValid=!error && data.length>0;dispatch_semaphore_signal(hold.entered);dispatch_semaphore_wait(hold.releaseGate,dispatch_time(DISPATCH_TIME_NOW,5000000000));dispatch_semaphore_signal(hold.returned);}];[task resume];
 for(unsigned i=0;i<150 && task.state!=NSURLSessionTaskStateCompleted;i++)usleep(20000);loader->set_host_network_blocked(1);usleep(200000);
 uint32_t(*count)(void)=dlsym(RTLD_DEFAULT,"loader_urlsession_callbacks_pending");loader_network_state_v1 state={.abi_version=LOADER_ABI_V1,.struct_size=sizeof(state)};loader->get_host_network_state(&state);BOOL drainedQueued=state.tracked_tasks_drained;uint32_t queued=count?count():0;
 queue.suspended=NO;BOOL entered=dispatch_semaphore_wait(hold.entered,dispatch_time(DISPATCH_TIME_NOW,3000000000))==0;loader->get_host_network_state(&state);BOOL drainedExecuting=state.tracked_tasks_drained;uint32_t executing=count?count():0;
 dispatch_semaphore_signal(hold.releaseGate);BOOL returned=dispatch_semaphore_wait(hold.returned,dispatch_time(DISPATCH_TIME_NOW,3000000000))==0;for(unsigned i=0;i<100 && count && count();i++)usleep(10000);[session invalidateAndCancel];uint32_t remaining=count?count():0;
 return @{@"queuedPending":@(queued),@"executingPending":@(executing),@"remainingPending":@(remaining),@"drainedWhileQueued":@(drainedQueued),@"drainedWhileExecuting":@(drainedExecuting),@"entered":@(entered),@"returned":@(returned),@"bodyValid":@(hold.bodyValid)};
}

@interface DerivedBackgroundProbe:BackgroundProbe
@property dispatch_semaphore_t releaseGate;
@end
@implementation DerivedBackgroundProbe
-(void)URLSession:(NSURLSession*)session task:(NSURLSessionTask*)task didCompleteWithError:(NSError*)error{[super URLSession:session task:task didCompleteWithError:error];dispatch_semaphore_wait(self.releaseGate,dispatch_time(DISPATCH_TIME_NOW,5000000000));}
@end
static NSDictionary *inheritanceCase(void){
 loader->set_host_network_blocked(0);DerivedBackgroundProbe *delegate=[DerivedBackgroundProbe new];delegate.complete=dispatch_semaphore_create(0);delegate.invalidated=dispatch_semaphore_create(0);delegate.progress=dispatch_semaphore_create(0);delegate.releaseGate=dispatch_semaphore_create(0);NSOperationQueue *queue=[NSOperationQueue new];queue.maxConcurrentOperationCount=1;
 NSURLSession *session=[NSURLSession sessionWithConfiguration:NSURLSessionConfiguration.ephemeralSessionConfiguration delegate:delegate delegateQueue:queue];NSString *base=[NSString stringWithUTF8String:getenv("LOADER_BACKGROUND_BASE")];NSURLSessionDownloadTask *task=[session downloadTaskWithURL:[NSURL URLWithString:[base stringByAppendingString:@"/delegate-inheritance"]]];[task resume];BOOL superReturned=dispatch_semaphore_wait(delegate.complete,dispatch_time(DISPATCH_TIME_NOW,3000000000))==0;loader->set_host_network_blocked(1);usleep(200000);
 uint32_t(*count)(void)=dlsym(RTLD_DEFAULT,"loader_urlsession_callbacks_pending");uint32_t before=count?count():0;loader_network_state_v1 state={.abi_version=LOADER_ABI_V1,.struct_size=sizeof(state)};loader->get_host_network_state(&state);BOOL drained=state.tracked_tasks_drained;
 dispatch_semaphore_signal(delegate.releaseGate);for(unsigned i=0;i<100 && count && count();i++)usleep(10000);[session invalidateAndCancel];BOOL invalidated=dispatch_semaphore_wait(delegate.invalidated,dispatch_time(DISPATCH_TIME_NOW,3000000000))==0;
 return @{@"superReturned":@(superReturned),@"pendingAfterSuper":@(before),@"drainedBeforeDerivedReturns":@(drained),@"pendingAfterDerivedReturns":@(count?count():0),@"invalidated":@(invalidated),@"downloaded":@(delegate.downloaded)};
}

@interface ProbeForwarder:NSProxy
@property id target;
@end
@implementation ProbeForwarder
-(NSMethodSignature*)methodSignatureForSelector:(SEL)selector{return [self.target methodSignatureForSelector:selector];}
-(void)forwardInvocation:(NSInvocation*)invocation{[invocation invokeWithTarget:self.target];}
-(BOOL)respondsToSelector:(SEL)selector{return [self.target respondsToSelector:selector];}
@end
static NSDictionary *proxyCase(void){
 loader->set_host_network_blocked(0);BackgroundProbe *delegate=[BackgroundProbe new];delegate.complete=dispatch_semaphore_create(0);delegate.invalidated=dispatch_semaphore_create(0);delegate.progress=dispatch_semaphore_create(0);ProbeForwarder *proxy=[ProbeForwarder alloc];proxy.target=delegate;NSOperationQueue *queue=[NSOperationQueue new];queue.maxConcurrentOperationCount=1;
 NSURLSession *session=[NSURLSession sessionWithConfiguration:NSURLSessionConfiguration.ephemeralSessionConfiguration delegate:(id<NSURLSessionDelegate>)proxy delegateQueue:queue];NSString *base=[NSString stringWithUTF8String:getenv("LOADER_BACKGROUND_BASE")];NSURLSessionDownloadTask *task=[session downloadTaskWithURL:[NSURL URLWithString:[base stringByAppendingString:@"/forwarding-delegate"]]];[task resume];BOOL completed=dispatch_semaphore_wait(delegate.complete,dispatch_time(DISPATCH_TIME_NOW,3000000000))==0;[session invalidateAndCancel];BOOL invalidated=dispatch_semaphore_wait(delegate.invalidated,dispatch_time(DISPATCH_TIME_NOW,3000000000))==0;uint32_t(*count)(void)=dlsym(RTLD_DEFAULT,"loader_urlsession_callbacks_pending");
 return @{@"originalDelegatePreserved":@(completed && delegate.downloaded && delegate.error==0),@"invalidated":@(invalidated),@"coverageRemainsConservative":@(count && count()>0)};
}
static NSDictionary *runCase(NSString *phase,BOOL initiallyBlocked,BOOL revokeLive){
 loader->set_host_network_blocked(initiallyBlocked);NSString *identifier=[@"io.opensv2.background-test." stringByAppendingString:NSUUID.UUID.UUIDString];NSURLSessionConfiguration *config=[NSURLSessionConfiguration backgroundSessionConfigurationWithIdentifier:identifier];config.discretionary=NO;config.sessionSendsLaunchEvents=NO;config.timeoutIntervalForRequest=8;config.timeoutIntervalForResource=25;
 BackgroundProbe *delegate=[BackgroundProbe new];delegate.complete=dispatch_semaphore_create(0);delegate.invalidated=dispatch_semaphore_create(0);delegate.progress=dispatch_semaphore_create(0);NSOperationQueue *queue=[NSOperationQueue new];queue.maxConcurrentOperationCount=1;
 NSURLSession *session=[NSURLSession sessionWithConfiguration:config delegate:delegate delegateQueue:queue];NSString *base=[NSString stringWithUTF8String:getenv("LOADER_BACKGROUND_BASE")];NSURLSessionDownloadTask *task=[session downloadTaskWithURL:[NSURL URLWithString:[base stringByAppendingFormat:@"/background-%@",phase]]];[task resume];BOOL hadProgress=NO,wasRunning=NO,drainedWithDelegateHeld=NO,completionHeld=NO;NSInteger taskStateAtBarrier=-1;uint32_t pendingAtBarrier=0;uint32_t(*callbackCount)(void)=dlsym(RTLD_DEFAULT,"loader_urlsession_callbacks_pending");
 if(revokeLive){hadProgress=dispatch_semaphore_wait(delegate.progress,dispatch_time(DISPATCH_TIME_NOW,8000000000))==0;wasRunning=task.state==NSURLSessionTaskStateRunning;queue.suspended=YES;loader->set_host_network_blocked(1);usleep(200000);loader_network_state_v1 state={.abi_version=LOADER_ABI_V1,.struct_size=sizeof(state)};loader->get_host_network_state(&state);drainedWithDelegateHeld=state.tracked_tasks_drained;taskStateAtBarrier=task.state;if(callbackCount)pendingAtBarrier=callbackCount();completionHeld=dispatch_semaphore_wait(delegate.complete,DISPATCH_TIME_NOW)!=0;queue.suspended=NO;}
 BOOL completed=dispatch_semaphore_wait(delegate.complete,dispatch_time(DISPATCH_TIME_NOW,12000000000))==0;
 [session invalidateAndCancel];BOOL invalidated=dispatch_semaphore_wait(delegate.invalidated,dispatch_time(DISPATCH_TIME_NOW,5000000000))==0;
 return @{@"configurationHasIdentifier":@(config.identifier!=nil),@"completed":@(completed),@"invalidated":@(invalidated),@"downloaded":@(delegate.downloaded),@"error":@(delegate.error),@"taskCompleted":@(task.state==NSURLSessionTaskStateCompleted),@"hadProgress":@(hadProgress),@"wasRunningAtBlock":@(wasRunning),@"receivedBytes":@(task.countOfBytesReceived),@"drainedWithDelegateHeld":@(drainedWithDelegateHeld),@"taskStateAtBarrier":@(taskStateAtBarrier),@"completionHeld":@(completionHeld),@"callbacksPendingAtBarrier":@(pendingAtBarrier),@"callbacksPendingAfter":@(callbackCount?callbackCount():0)};
}
int loader_plugin_init_v1(const loader_api_v1 *api){if(!api || api->abi_version!=LOADER_ABI_V1)return 1;loader=api;dispatch_async(dispatch_get_global_queue(QOS_CLASS_UTILITY,0),^{@autoreleasepool{
 NSMutableDictionary *report=[NSMutableDictionary dictionary];report[@"blocked"]=runCase(@"blocked",YES,NO);report[@"allowed"]=runCase(@"allowed",NO,NO);report[@"live"]=runCase(@"live",NO,YES);loader->set_host_network_blocked(0);report[@"restored"]=runCase(@"restored",NO,NO);report[@"completionBlock"]=blockCase();report[@"delegateInheritance"]=inheritanceCase();report[@"forwardingDelegate"]=proxyCase();loader->set_host_network_blocked(0);
 NSData *data=[NSJSONSerialization dataWithJSONObject:report options:0 error:NULL];fprintf(stdout,"BACKGROUND_PROBE %s\n",[[NSString alloc]initWithData:data encoding:NSUTF8StringEncoding].UTF8String);fflush(stdout);fprintf(stderr,"BACKGROUND_PROBE_DONE\n");fflush(stderr);
 }});return 0;}
