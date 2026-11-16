// Diagnostic only: no microphone/camera access, only local STUN candidate gathering.
#import <Foundation/Foundation.h>
#import <WebKit/WebKit.h>
#import <objc/message.h>
#include <errno.h>
#include <signal.h>
#include <dlfcn.h>
#include "loader_api.h"
static const loader_api_v1 *loader;
static WKWebView *view,*firstView;
static NSMutableArray *observations;
static id<WKNavigationDelegate> navigation;
static unsigned phase;
static BOOL createdWhileBlocked;
static BOOL restoredDocument;
static unsigned pendingJSReplies;
static NSInteger pendingJSError;
static BOOL pendingJSOnMain;
static NSTimeInterval pendingJSReplyAt;
static NSMutableDictionary *coreChecks;
static NSDictionary *coreReceipt;
static unsigned coreReplies,rejectedReplies;
static BOOL coreReplyOnMain,coreReplyCounted;
static BOOL coreCountCleared;
static void rejected(const char *body,size_t length,void *context){(void)body;(void)length;(void)context;rejectedReplies++;}
static void retired(const char *body,size_t length,void *context){(void)context;coreReplies++;coreReplyOnMain=NSThread.isMainThread;coreReceipt=[NSJSONSerialization JSONObjectWithData:[NSData dataWithBytes:body length:length] options:0 error:NULL];loader_network_state_v1 state={.abi_version=1,.struct_size=sizeof state};loader->get_host_network_state(&state);uint32_t(*count)(void)=dlsym(RTLD_DEFAULT,"loader_web_retirement_pending");coreReplyCounted=count&&count()==1&&!state.tracked_tasks_drained;dispatch_async(dispatch_get_main_queue(),^{coreCountCleared=count&&count()==0;});}
static void saveResult(void){loader->set_host_network_blocked(0);loader_network_state_v1 state={.abi_version=1,.struct_size=sizeof state};loader->get_host_network_state(&state);NSDictionary *value=@{@"pid":@(loader->host_pid),@"phases":observations,@"unverifiedPaths":@(state.unverified_paths),@"wholeHostVerified":@(state.whole_host_verified),@"coreRetirementChecks":coreChecks?:@{},@"coreRetirementReceipt":coreReceipt?:@{},@"coreReplyCount":@(coreReplies),@"coreReplyOnMain":@(coreReplyOnMain),@"coreReplyCounted":@(coreReplyCounted),@"coreCountCleared":@(coreCountCleared),@"rejectedCallReplies":@(rejectedReplies),@"scope":@"Actual WKWebView local STUN gathering audit; unavailable/no baseline traffic is inconclusive, never isolation proof."};NSData *data=[NSJSONSerialization dataWithJSONObject:value options:NSJSONWritingPrettyPrinted error:NULL];[data writeToFile:@(getenv("LOADER_WEBRTC_RESULT")) options:NSDataWritingAtomic error:NULL];}
static void leaseChanged(const char *body,size_t length,void *context){(void)context;id value=[NSJSONSerialization JSONObjectWithData:[NSData dataWithBytes:body length:length] options:0 error:NULL];coreChecks[@"releasedLeaseFailsBeforeRetirement"]=@([value[@"error"][@"code"] isEqual:@"PROFILE_LEASE_CHANGED"]&&NSThread.isMainThread);pid_t pid=((pid_t(*)(id,SEL))objc_msgSend)(view,NSSelectorFromString(@"_webProcessIdentifier"));coreChecks[@"releasedLeasePreservesRenderer"]=@(pid>0&&kill(pid,0)==0);dispatch_async(dispatch_get_main_queue(),^{saveResult();});}
static void policyChanged(const char *body,size_t length,void *context){(void)context;id value=[NSJSONSerialization JSONObjectWithData:[NSData dataWithBytes:body length:length] options:0 error:NULL];coreChecks[@"changedPolicyFailsBeforeRetirement"]=@([value[@"error"][@"code"] isEqual:@"STALE_NETWORK_GENERATION"]&&NSThread.isMainThread);pid_t pid=((pid_t(*)(id,SEL))objc_msgSend)(view,NSSelectorFromString(@"_webProcessIdentifier"));coreChecks[@"changedPolicyPreservesRenderer"]=@(pid>0&&kill(pid,0)==0);loader->release_profile_exclusive("test.web-renderers","negative-policy");dispatch_async(dispatch_get_main_queue(),^{loader->set_host_network_blocked(1);loader_network_state_v1 state={.abi_version=1,.struct_size=sizeof state};loader->get_host_network_state(&state);loader->acquire_profile_exclusive("test.web-renderers","negative-lease");coreChecks[@"leaseFailureAcceptedInitially"]=@(loader->retire_host_web_renderers(state.generation,leaseChanged,NULL)==0);loader->release_profile_exclusive("test.web-renderers","negative-lease");});}
static void negativeChecks(void){loader->set_host_network_blocked(0);loader_network_state_v1 state={.abi_version=1,.struct_size=sizeof state};loader->get_host_network_state(&state);coreChecks[@"onlinePolicyRefused"]=@(loader->retire_host_web_renderers(state.generation,rejected,NULL)==LOADER_ERR_INVALID);loader->set_host_network_blocked(1);loader->get_host_network_state(&state);loader->acquire_profile_exclusive("test.web-renderers","negative-policy");coreChecks[@"nullCompletionRefused"]=@(loader->retire_host_web_renderers(state.generation,NULL,NULL)==LOADER_ERR_INVALID);coreChecks[@"policyFailureAcceptedInitially"]=@(loader->retire_host_web_renderers(state.generation,policyChanged,NULL)==0);loader->set_host_network_blocked(0);}
static void finish(void){
 if(!getenv("LOADER_WEBRTC_POOL_RESET")&&!getenv("LOADER_WEBRTC_CORE_RETIRE"))[firstView evaluateJavaScript:@"for(const pc of (window.extendPeers||[]))pc.close();true" completionHandler:nil];
 [view evaluateJavaScript:@"for(const pc of (window.extendPeers||[]))pc.close();true" completionHandler:^(id result,NSError *error){(void)result;(void)error;
  if(getenv("LOADER_WEBRTC_CORE_RETIRE")&&[coreChecks[@"apiAvailable"] boolValue])negativeChecks();else saveResult();
 }];
}
static void execute(void){
 if(phase==3&&!restoredDocument){restoredDocument=YES;loader->set_host_network_blocked(0);[view loadHTMLString:@"<!doctype html><title>Fresh online document</title>" baseURL:[NSURL URLWithString:@"http://localhost/"]];return;}
 if(phase==2&&!createdWhileBlocked){createdWhileBlocked=YES;view=[[WKWebView alloc]initWithFrame:NSMakeRect(0,0,320,180) configuration:[WKWebViewConfiguration new]];view.navigationDelegate=navigation;[view loadHTMLString:@"<!doctype html><title>Created while blocked</title>" baseURL:[NSURL URLWithString:@"http://localhost/"]];return;}
 int code=loader->set_host_network_blocked(phase==1||phase==2);unsigned current=phase;unsigned port=(unsigned)atoi(getenv(current==0?"LOADER_WEBRTC_PORT0":current==1?"LOADER_WEBRTC_PORT1":current==2?"LOADER_WEBRTC_PORT2":"LOADER_WEBRTC_PORT3"));
 NSTimeInterval started=NSDate.date.timeIntervalSince1970;
 if(current==1&&(getenv("LOADER_WEBRTC_POOL_RESET")||getenv("LOADER_WEBRTC_PENDING_CONTROL")||getenv("LOADER_WEBRTC_CORE_RETIRE"))){
  // Submit work before termination. Do not rely on a new JS request to a dead
  // page to acknowledge that its previous renderer has gone away.
  [view callAsyncJavaScript:@"window.extendPendingPromise=new Promise(resolve=>{window.extendResolve=resolve});await window.extendPendingPromise;return true" arguments:@{} inFrame:nil inContentWorld:WKContentWorld.pageWorld completionHandler:^(id result,NSError *error){(void)result;pendingJSReplies++;pendingJSError=error.code;pendingJSOnMain=NSThread.isMainThread;pendingJSReplyAt=NSDate.date.timeIntervalSince1970;}];
  dispatch_after(dispatch_time(DISPATCH_TIME_NOW,100*NSEC_PER_MSEC),dispatch_get_main_queue(),^{
   Class poolClass=NSClassFromString(@"WKProcessPool");SEL list=NSSelectorFromString(@"_allProcessPoolsForTesting"),info=NSSelectorFromString(@"_webContentProcessInfo"),terminate=NSSelectorFromString(@"_terminateAllWebContentProcesses");BOOL available=[poolClass respondsToSelector:list]&&[poolClass respondsToSelector:info];NSArray *pools=available?((id(*)(id,SEL))objc_msgSend)(poolClass,list):@[],*processes=available?((id(*)(id,SEL))objc_msgSend)(poolClass,info):@[];NSMutableArray *pids=[NSMutableArray array];for(id process in processes)if([process respondsToSelector:@selector(pid)])[pids addObject:@(((pid_t(*)(id,SEL))objc_msgSend)(process,@selector(pid)))];
   BOOL wasPending=pendingJSReplies==0;NSTimeInterval resetAt=NSDate.date.timeIntervalSince1970;BOOL control=getenv("LOADER_WEBRTC_PENDING_CONTROL")!=NULL;
   BOOL coreMode=getenv("LOADER_WEBRTC_CORE_RETIRE")!=NULL;
   if(coreMode){loader_network_state_v1 state={.abi_version=1,.struct_size=sizeof state};loader->get_host_network_state(&state);BOOL apiAvailable=loader->struct_size>=offsetof(loader_api_v1,retire_host_web_renderers)+sizeof loader->retire_host_web_renderers&&(loader->capabilities&LOADER_CAP_WEB_RENDERER_RETIREMENT)&&loader->retire_host_web_renderers;coreChecks=[NSMutableDictionary dictionary];coreChecks[@"apiAvailable"]=@(apiAvailable);
    if(apiAvailable){coreChecks[@"staleGenerationRefused"]=@(loader->retire_host_web_renderers(state.generation+1,rejected,NULL)==LOADER_ERR_STALE_GENERATION);coreChecks[@"sharedLeaseRefused"]=@(loader->retire_host_web_renderers(state.generation,rejected,NULL)==LOADER_ERR_PROFILE_BUSY);coreChecks[@"exclusiveAcquired"]=@(loader->acquire_profile_exclusive("test.web-renderers","probe")==0);coreChecks[@"accepted"]=@(loader->retire_host_web_renderers(state.generation,retired,NULL)==0);coreChecks[@"concurrentOperationRefused"]=@(loader->retire_host_web_renderers(state.generation,rejected,NULL)==LOADER_ERR_PROFILE_BUSY);}
   }else if(!control){for(id pool in pools){if([pool respondsToSelector:terminate])((void(*)(id,SEL))objc_msgSend)(pool,terminate);else available=NO;}}
   dispatch_after(dispatch_time(DISPATCH_TIME_NOW,2200*NSEC_PER_MSEC),dispatch_get_main_queue(),^{
    NSUInteger alive=0;for(NSNumber *pid in pids)if(kill(pid.intValue,0)==0||errno!=ESRCH)alive++;NSArray *remaining=available?((id(*)(id,SEL))objc_msgSend)(poolClass,info):@[];
    [observations addObject:@{@"phase":@1,@"policyCode":@(code),@"blocked":@(loader->host_network_blocked()),@"startedAtEpoch":@(started),@"completedAtEpoch":@(NSDate.date.timeIntervalSince1970),@"resetAtEpoch":@(resetAt),@"poolReset":@(!control),@"poolSPIAvailable":@(available),@"processesBefore":@(pids.count),@"oldPIDsStillAlive":@(alive),@"processesAfter":@(remaining.count),@"pendingAtReset":@(wasPending),@"pendingJSReplies":@(pendingJSReplies),@"pendingJSError":@(pendingJSError),@"pendingJSOnMain":@(pendingJSOnMain),@"pendingJSReplyAtEpoch":@(pendingJSReplyAt)}];if(control)[view evaluateJavaScript:@"window.extendResolve(true);true" completionHandler:nil];if(coreMode)coreChecks[@"exclusiveReleased"]=@(loader->release_profile_exclusive("test.web-renderers","probe")==0);phase=2;execute();
   });
  });return;
 }
 BOOL nativePreference=getenv("LOADER_WEBRTC_NATIVE_PREF")!=NULL;SEL setter=NSSelectorFromString(@"_setPeerConnectionEnabled:");BOOL preferenceAvailable=[view.configuration.preferences respondsToSelector:setter];
 if(nativePreference&&preferenceAvailable)((void(*)(id,SEL,BOOL))objc_msgSend)(view.configuration.preferences,setter,current!=1&&current!=2);
 SEL getter=NSSelectorFromString(@"_peerConnectionEnabled");BOOL actualPreference=preferenceAvailable?((BOOL(*)(id,SEL))objc_msgSend)(view.configuration.preferences,getter):NO;

 NSString *javascript=[NSString stringWithFormat:@"const type=typeof RTCPeerConnection; if(type!=='function')return {rtcType:type};window.extendPeers=window.extendPeers||[];try{const RTC=window.extendCachedRTC||(window.extendCachedRTC=RTCPeerConnection);const pc=new RTC({iceServers:[{urls:'stun:127.0.0.1:%u'}],iceTransportPolicy:'all'});window.extendPeers.push(pc);pc.createDataChannel('extend-local-audit');let candidates=0;pc.onicecandidate=e=>{if(e.candidate)candidates++};await pc.setLocalDescription(await pc.createOffer());await new Promise(r=>setTimeout(r,2200));return {rtcType:type,gathering:pc.iceGatheringState,candidateCount:candidates};}catch(e){return {rtcType:type,errorName:e.name};}",port];
 [view callAsyncJavaScript:javascript arguments:@{} inFrame:nil inContentWorld:WKContentWorld.pageWorld completionHandler:^(id result,NSError *error){
  NSMutableDictionary *value=[@{@"phase":@(current),@"policyCode":@(code),@"blocked":@(loader->host_network_blocked()),@"startedAtEpoch":@(started),@"completedAtEpoch":@(NSDate.date.timeIntervalSince1970),@"nativePreferenceTest":@(nativePreference),@"nativePreferenceAvailable":@(preferenceAvailable),@"actualPreference":@(actualPreference)} mutableCopy];if([result isKindOfClass:NSDictionary.class])value[@"javascript"]=result;if(error)value[@"errorCode"]=@(error.code);[observations addObject:value];
  NSTimeInterval remaining=MAX(0,2.2-(NSDate.date.timeIntervalSince1970-started));dispatch_after(dispatch_time(DISPATCH_TIME_NOW,(int64_t)(remaining*NSEC_PER_SEC)),dispatch_get_main_queue(),^{if(++phase<4)execute();else finish();});
 }];
}
@interface RealtimeNavigation:NSObject<WKNavigationDelegate>
@end

@implementation RealtimeNavigation
- (void)webView:(WKWebView*)webView didFinishNavigation:(WKNavigation*)item{(void)webView;(void)item;execute();}
@end
int loader_plugin_init_v1(const loader_api_v1 *api){if(!api||api->abi_version!=1)return 1;loader=api;observations=[NSMutableArray array];navigation=[RealtimeNavigation new];dispatch_async(dispatch_get_main_queue(),^{view=[[WKWebView alloc]initWithFrame:NSMakeRect(0,0,320,180) configuration:[WKWebViewConfiguration new]];firstView=view;view.navigationDelegate=navigation;[view loadHTMLString:@"<!doctype html><title>Extend local WebRTC audit</title>" baseURL:[NSURL URLWithString:@"http://localhost/"]];});return 0;}
