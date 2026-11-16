// Real WebKit context lifecycle diagnostic; never included in shipped resources.
#import <AppKit/AppKit.h>
#import <WebKit/WebKit.h>
#import <objc/message.h>
#include "loader_api.h"
#include <dlfcn.h>
#include <errno.h>
#include <signal.h>
#include <unistd.h>
static const loader_api_v1 *api;
static NSMutableDictionary *checks,*result;
static unsigned stage,jsReplies,coreReplies,rejectedReplies;
static BOOL jsMain;static NSInteger jsError;
static NSArray<NSNumber*> *oldNetwork,*oldRenderers;
static WKWebsiteDataStore *store;
static WKWebView *view;
static NSDictionary *decode(const char *json,size_t length){return [NSJSONSerialization JSONObjectWithData:[NSData dataWithBytes:json length:length] options:0 error:NULL];}
static NSArray *pids(NSString *selector){id values=((id(*)(id,SEL))objc_msgSend)(NSClassFromString(@"WKProcessPool"),NSSelectorFromString(selector));NSMutableArray *pids=[NSMutableArray array];for(id value in values)[pids addObject:@(((pid_t(*)(id,SEL))objc_msgSend)(value,NSSelectorFromString(@"pid")))];return pids;}
static BOOL alive(NSNumber *pid){return [pid intValue]>0&&kill([pid intValue],0)==0;}
static BOOL noneAlive(NSArray *pids){for(NSNumber *pid in pids)if(alive(pid))return NO;return YES;}
static loader_network_state_v1 state(void){loader_network_state_v1 s={.abi_version=1,.struct_size=sizeof s};if(api->get_host_network_state(&s))abort();return s;}
static void save(void){result[@"checks"]=checks;result[@"pid"]=@(getpid());NSData *data=[NSJSONSerialization dataWithJSONObject:result options:NSJSONWritingPrettyPrinted error:NULL];[data writeToFile:@(getenv("EXTEND_WEB_CONTEXT_RESULT")) options:NSDataWritingAtomic error:NULL];}
static void rejected(const char *json,size_t length,void *context){(void)json;(void)length;(void)context;rejectedReplies++;}
static void completed(const char *json,size_t length,void *context){
 (void)context;coreReplies++;NSDictionary *reply=decode(json,length);result[@"contextReceipt"]=reply;checks[@"contextAcceptedCompletionOnceMain"]=@(coreReplies==1&&NSThread.isMainThread);
 uint32_t(*pending)(void)=dlsym(RTLD_DEFAULT,"loader_web_retirement_pending");checks[@"completionStillCounted"]=@(pending&&pending()>0&&!state().tracked_tasks_drained);
 checks[@"nativeContextReceiptSucceeded"]=@([reply[@"ok"]boolValue]&&[reply[@"result"][@"networkProcessesExited"]boolValue]&&[reply[@"result"][@"renderersExited"]boolValue]&&![reply[@"result"][@"websiteDataDeleted"]boolValue]&&![reply[@"result"][@"wholeHostNetworkVerified"]boolValue]);
 checks[@"oldNetworkKernelPIDsExited"]=@(oldNetwork.count&&noneAlive(oldNetwork));checks[@"oldRendererKernelPIDsExited"]=@(oldRenderers.count&&noneAlive(oldRenderers));checks[@"nativeNetworkListEmpty"]=@(pids(@"_networkingProcessInfo").count==0);
 dispatch_after(dispatch_time(DISPATCH_TIME_NOW,200*NSEC_PER_MSEC),dispatch_get_main_queue(),^{
  checks[@"pendingJavaScriptNativeFailureOnce"]=@(jsReplies==1&&jsMain&&jsError!=0);checks[@"completionReturnClearsCount"]=@(pending&&pending()==0);checks[@"rejectedCallsNeverReply"]=@(rejectedReplies==0);
  result[@"nativeJavaScriptError"]=@(jsError);checks[@"exclusiveReleasedAfterCompletion"]=@(api->release_profile_exclusive("test.web-context","retire")==0);checks[@"onlineRestored"]=@(api->set_host_network_blocked(0)==0);
  [store.httpCookieStore getAllCookies:^(NSArray<NSHTTPCookie*> *cookies){BOOL found=NO;for(NSHTTPCookie *cookie in cookies)if([cookie.name isEqual:@"extend_context_probe"]&&[cookie.value isEqual:@"probe"])found=YES;checks[@"persistentCookieSurvives"]=@(found);NSArray *newNetwork=pids(@"_networkingProcessInfo");checks[@"nativeNetworkCanRelaunch"]=@(newNetwork.count>0);result[@"networkAfterRestore"]=newNetwork;save();}];
 });
}
static void retireCombined(void){
 oldNetwork=pids(@"_networkingProcessInfo");oldRenderers=pids(@"_webContentProcessInfo");result[@"networkBeforeCombined"]=oldNetwork;result[@"renderersBeforeCombined"]=oldRenderers;
 checks[@"pendingJSBeforeContextRetirement"]=@(jsReplies==0);uint64_t generation=state().generation;
 checks[@"staleGenerationRefused"]=@(api->retire_host_web_contexts(generation+1,rejected,NULL)==LOADER_ERR_STALE_GENERATION);
 checks[@"nullCompletionRefused"]=@(api->retire_host_web_contexts(generation,NULL,NULL)==LOADER_ERR_INVALID);
 int code=api->retire_host_web_contexts(generation,completed,NULL);checks[@"contextCallAccepted"]=@(code==0);checks[@"concurrentRetirementRefused"]=@(api->retire_host_web_contexts(generation,rejected,NULL)==LOADER_ERR_PROFILE_BUSY);if(code)save();
}
static void rendererCompleted(const char *json,size_t length,void *context){(void)context;NSDictionary *reply=decode(json,length);result[@"rendererReceipt"]=reply;checks[@"rendererOnlySucceeded"]=@([reply[@"ok"]boolValue]);BOOL survives=NO;for(NSNumber *pid in oldNetwork)if(alive(pid))survives=YES;checks[@"rendererOnlyLeavesNetworkContext"]=@(survives&&pids(@"_networkingProcessInfo").count>0);stage=2;[view loadHTMLString:@"<html><body>context phase two</body></html>" baseURL:nil];}
@interface ContextNavigation:NSObject<WKNavigationDelegate>@end
static ContextNavigation *navigation;
@implementation ContextNavigation
- (void)webView:(WKWebView*)webView didFinishNavigation:(WKNavigation*)nav{
 (void)nav;if(stage==0){stage=1;
  NSHTTPCookie *cookie=[NSHTTPCookie cookieWithProperties:@{NSHTTPCookieName:@"extend_context_probe",NSHTTPCookieValue:@"probe",NSHTTPCookieDomain:@"extend.invalid",NSHTTPCookiePath:@"/",NSHTTPCookieExpires:[NSDate dateWithTimeIntervalSinceNow:3600]}];
  [store.httpCookieStore setCookie:cookie completionHandler:^{[store.httpCookieStore getAllCookies:^(NSArray<NSHTTPCookie*> *cookies){checks[@"cookieBeforeRetirement"]=@(cookies.count>0);oldNetwork=pids(@"_networkingProcessInfo");result[@"networkBeforeRendererOnly"]=oldNetwork;
   checks[@"onlineContextCallRefused"]=@(api->retire_host_web_contexts(state().generation,rejected,NULL)==LOADER_ERR_INVALID);checks[@"policyBlocked"]=@(api->set_host_network_blocked(1)==0);
   checks[@"sharedLeaseContextCallRefused"]=@(api->retire_host_web_contexts(state().generation,rejected,NULL)==LOADER_ERR_PROFILE_BUSY);checks[@"exclusiveAcquired"]=@(api->acquire_profile_exclusive("test.web-context","retire")==0);
   checks[@"rendererOnlyAccepted"]=@(api->retire_host_web_renderers(state().generation,rendererCompleted,NULL)==0);
  }];}];
 }else if(stage==2){stage=3;
  [webView callAsyncJavaScript:@"return new Promise(resolve => {});" arguments:@{} inFrame:nil inContentWorld:WKContentWorld.pageWorld completionHandler:^(id value,NSError *error){(void)value;jsReplies++;jsMain=NSThread.isMainThread;jsError=error.code;}];
  dispatch_after(dispatch_time(DISPATCH_TIME_NOW,150*NSEC_PER_MSEC),dispatch_get_main_queue(),^{retireCombined();});
 }
}
@end
int loader_plugin_init_v1(const loader_api_v1 *value){
 if(!value||value->struct_size<offsetof(loader_api_v1,retire_host_web_contexts)+sizeof value->retire_host_web_contexts||!value->retire_host_web_contexts)return -1;api=value;checks=[NSMutableDictionary dictionary];result=[NSMutableDictionary dictionary];
 checks[@"contextCapabilityAdvertised"]=@((api->capabilities&LOADER_CAP_WEB_CONTEXT_RETIREMENT)!=0);
 dispatch_async(dispatch_get_main_queue(),^{store=[WKWebsiteDataStore dataStoreForIdentifier:NSUUID.UUID];WKWebViewConfiguration *configuration=[WKWebViewConfiguration new];configuration.websiteDataStore=store;view=[[WKWebView alloc]initWithFrame:NSMakeRect(0,0,32,32) configuration:configuration];store=view.configuration.websiteDataStore;navigation=[ContextNavigation new];view.navigationDelegate=navigation;[view loadHTMLString:@"<html><body>context phase one</body></html>" baseURL:nil];});return 0;
}
