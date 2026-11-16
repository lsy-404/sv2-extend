// Real HTTP cookie tests in disposable hosts. Synthetic values only.
#import <Foundation/Foundation.h>
#import <AppKit/AppKit.h>
#include "loader_api.h"
#include <dlfcn.h>
#import <objc/runtime.h>
#include <mach-o/dyld.h>
#include <execinfo.h>
#include <stdatomic.h>
static BOOL constructorMarkerPresent;
static NSHTTPCookieStorage *constructorStore;
static NSString *constructorClass;
static BOOL constructorObjectStillCurrent,oldStoreStillHadAAtStart;
static uintptr_t constructorCF,currentCF;
static BOOL markerSurvivedCFAccessor;
static _Atomic unsigned nativeClearObserved,totalFixtureDeletes,totalRemoveSince;
static NSMutableArray *mutationFrames;
static intptr_t hostSlide;
static BOOL hasA(NSHTTPCookieStorage *store){for(NSHTTPCookie *cookie in store.cookies)if([cookie.name isEqual:@"svext_http_namespace"] && [cookie.value isEqual:@"A"])return YES;return NO;}
static uintptr_t cfStorage(NSHTTPCookieStorage *store){
 SEL selector=NSSelectorFromString(@"_cookieStorage");Method method=class_getInstanceMethod(object_getClass(store),selector);if(!method || method_getNumberOfArguments(method)!=2)return 0;
 char *type=method_copyReturnType(method);BOOL pointer=type && type[0]=='^';free(type);if(!pointer)return 0;
 return (uintptr_t)((void*(*)(id,SEL))method_getImplementation(method))(store,selector);
}
static void observeFrames(NSString *operation){
 void *stack[64];int count=backtrace(stack,64);NSMutableArray *frames=[NSMutableArray array];BOOL nativeClear=NO;
 for(int i=0;i<count;i++){uintptr_t pc=(uintptr_t)stack[i]-(uintptr_t)hostSlide;if(pc>=0x100000000ULL && pc<0x10119a400ULL){[frames addObject:[NSString stringWithFormat:@"0x%llx",(unsigned long long)pc]];if(pc>=0x1008ee87cULL && pc<0x1008ee8f8ULL)nativeClear=YES;}}
 if(nativeClear)atomic_fetch_add(&nativeClearObserved,1);
 @synchronized(mutationFrames){if(mutationFrames.count<16)[mutationFrames addObject:@{@"operation":operation,@"nativeFrames":frames}];}
}
static BOOL isCookieClass(Class cls){for(Class current=cls;current;current=class_getSuperclass(current))if(current==NSHTTPCookieStorage.class)return YES;return NO;}
__attribute__((constructor))static void observeNativeCookieClear(void){
 if(!getenv("LOADER_COOKIE_RESULT"))return;
 mutationFrames=[NSMutableArray array];for(uint32_t i=0;i<_dyld_image_count();i++)if(_dyld_get_image_header(i)->filetype==MH_EXECUTE){hostSlide=_dyld_get_image_vmaddr_slide(i);break;}
 constructorStore=NSHTTPCookieStorage.sharedHTTPCookieStorage;constructorClass=NSStringFromClass(object_getClass(constructorStore));constructorMarkerPresent=hasA(constructorStore);constructorCF=cfStorage(constructorStore);markerSurvivedCFAccessor=hasA(constructorStore);
 unsigned count=0;Class *classes=objc_copyClassList(&count);
 for(unsigned i=0;i<count;i++){Class cls=classes[i];if(!isCookieClass(cls))continue;unsigned n=0;Method *methods=class_copyMethodList(cls,&n);
  for(unsigned j=0;j<n;j++){Method method=methods[j];SEL selector=method_getName(method);IMP previous=method_getImplementation(method);
   if(selector==@selector(deleteCookie:))method_setImplementation(method,imp_implementationWithBlock(^(NSHTTPCookieStorage *object,NSHTTPCookie *cookie){if([cookie.name hasPrefix:@"svext_http"]){atomic_fetch_add(&totalFixtureDeletes,1);observeFrames(@"deleteCookie:");}((void(*)(id,SEL,id))previous)(object,selector,cookie);}));
   else if(selector==@selector(removeCookiesSinceDate:))method_setImplementation(method,imp_implementationWithBlock(^(NSHTTPCookieStorage *object,NSDate *date){if(hasA(object)){atomic_fetch_add(&totalRemoveSince,1);observeFrames(@"removeCookiesSinceDate:");}((void(*)(id,SEL,id))previous)(object,selector,date);}));
  }free(methods);
 }free(classes);
}
@interface HTTPCookieFixture:NSObject
@property NSMutableDictionary *checks;
@property NSString *marker,*base;
@property BOOL reopen,expectStartupClear;
@property NSURLSession *normal,*ephemeral,*disabled,*custom;
@property NSUInteger step;
@property NSArray *steps;
-(void)next;
-(void)start;
@end
@implementation HTTPCookieFixture
-(void)start {
 NSHTTPCookieStorage *store=NSHTTPCookieStorage.sharedHTTPCookieStorage;constructorObjectStillCurrent=store==constructorStore;currentCF=cfStorage(store);for(NSHTTPCookie *cookie in constructorStore.cookies)if([cookie.name isEqual:@"svext_http_namespace"] && [cookie.value isEqual:@"A"])oldStoreStillHadAAtStart=YES;NSURLSessionConfiguration *config=NSURLSessionConfiguration.defaultSessionConfiguration,*ephemeral=NSURLSessionConfiguration.ephemeralSessionConfiguration,*disabled=NSURLSessionConfiguration.defaultSessionConfiguration,*custom=NSURLSessionConfiguration.defaultSessionConfiguration;
 disabled.HTTPCookieStorage=nil;disabled.HTTPShouldSetCookies=NO;NSHTTPCookieStorage *customStore=NSURLSessionConfiguration.ephemeralSessionConfiguration.HTTPCookieStorage;custom.HTTPCookieStorage=customStore;
 self.normal=[NSURLSession sessionWithConfiguration:config];self.ephemeral=[NSURLSession sessionWithConfiguration:ephemeral];self.disabled=[NSURLSession sessionWithConfiguration:disabled];self.custom=[NSURLSession sessionWithConfiguration:custom];
 self.checks[@"defaultUsesIsolatedShared"]=@(config.HTTPCookieStorage==store);self.checks[@"sharedSessionUsesIsolatedShared"]=@(NSURLSession.sharedSession.configuration.HTTPCookieStorage==store);self.checks[@"ephemeralRemainsDistinct"]=@(ephemeral.HTTPCookieStorage!=store);self.checks[@"nilStoragePreserved"]=@(disabled.HTTPCookieStorage==nil);self.checks[@"explicitCustomStorePreserved"]=@(custom.HTTPCookieStorage==customStore && customStore!=store);
 BOOL hasDirect=NO;for(NSHTTPCookie *cookie in store.cookies)if([cookie.name isEqual:@"svext_http_direct"] && [cookie.value isEqual:@"A"])hasDirect=YES;self.checks[self.expectStartupClear?@"directCookieFollowsNativeStartupPolicy":@"directPersistentBefore"]=@(self.reopen && !self.expectStartupClear?hasDirect:!hasDirect);
 BOOL oldTemporary=NO;for(NSHTTPCookie *cookie in store.cookies)if([cookie.name isEqual:@"svext_http_sessiononly"])oldTemporary=YES;self.checks[@"noSessionOnlyCookieRestored"]=@(!oldTemporary);
 if(self.reopen && self.expectStartupClear){
  self.checks[@"systemRestoredPriorCookieBeforeMain"]=@(constructorMarkerPresent && markerSurvivedCFAccessor);
  self.checks[@"hostClearedTwoFixtureCookies"]=@(atomic_load(&nativeClearObserved)==2);
  self.checks[@"storageObjectsStayedStableDuringClear"]=@(constructorObjectStillCurrent && constructorCF && constructorCF==currentCF);
  self.checks[@"priorCookieAbsentAfterNativeStartup"]=@(!oldStoreStillHadAAtStart);
 }
 id before=self.reopen && !self.expectStartupClear?@"A":NSNull.null;NSString *set=[@"/set/" stringByAppendingString:self.marker];
 self.steps=@[
 @{@"session":NSURLSession.sharedSession,@"path":@"/get",@"expected":before,@"check":@"sharedBefore"},
 @{@"session":self.normal,@"path":set,@"expected":before,@"check":@"normalSetResponse"},
 @{@"session":self.normal,@"path":@"/get",@"expected":self.marker,@"check":@"automaticSetCookieStored"},
 @{@"session":NSURLSession.sharedSession,@"path":@"/get",@"expected":self.marker,@"check":@"sharedSeesNormalCookie"},
 @{@"session":self.ephemeral,@"path":@"/get",@"expected":NSNull.null,@"check":@"ephemeralNoSharedCookie"},
 @{@"session":self.disabled,@"path":@"/get",@"expected":NSNull.null,@"check":@"disabledNoSharedCookie"},
 @{@"session":self.custom,@"path":@"/get",@"expected":NSNull.null,@"check":@"customNoSharedCookie"}];
 self.checks[@"cookiePolicyNotNeverAtStart"]=@(store.cookieAcceptPolicy!=NSHTTPCookieAcceptPolicyNever);
 [self next];
}
-(void)next {
 if(self.step==self.steps.count){
  NSHTTPCookieStorage *store=NSHTTPCookieStorage.sharedHTTPCookieStorage;
  for(NSHTTPCookie *cookie in store.cookies)if([cookie.name isEqual:@"svext_http_namespace"]){self.checks[@"syntheticCookieNotSessionOnly"]=@(!cookie.sessionOnly);self.checks[@"syntheticCookieHasExpiry"]=@(cookie.expiresDate!=nil);self.checks[@"syntheticCookieExpiryFuture"]=@([cookie.expiresDate timeIntervalSinceNow]>0);}
  NSHTTPCookie *direct=[NSHTTPCookie cookieWithProperties:@{NSHTTPCookieName:@"svext_http_direct",NSHTTPCookieValue:self.marker,NSHTTPCookieDomain:@"127.0.0.1",NSHTTPCookiePath:@"/",NSHTTPCookieExpires:[NSDate dateWithTimeIntervalSinceNow:86400]}];[store setCookie:direct];
  NSHTTPCookie *temporary=[NSHTTPCookie cookieWithProperties:@{NSHTTPCookieName:@"svext_http_sessiononly",NSHTTPCookieValue:@"temporary",NSHTTPCookieDomain:@"127.0.0.1",NSHTTPCookiePath:@"/"}];[store setCookie:temporary];
  BOOL stillPresent=NO;for(NSHTTPCookie *c in store.cookies)if([c.name isEqual:@"svext_http_namespace"])stillPresent=YES;self.checks[@"persistentCookiePresentAfterSessionCookie"]=@(stillPresent);
  self.checks[@"sessionOnlyCookieAccepted"]=@(temporary.sessionOnly && [store.cookies containsObject:temporary]);
  BOOL passed=YES;for(NSNumber *value in self.checks.allValues)if(!value.boolValue)passed=NO;
  NSDictionary *result=@{@"checks":self.checks,@"passed":@(passed),@"pid":@(getpid()),@"bundleIdentifier":NSBundle.mainBundle.bundleIdentifier?:@"",@"constructorMarkerPresent":@(constructorMarkerPresent),@"markerSurvivedCFAccessor":@(markerSurvivedCFAccessor),@"constructorClass":constructorClass?:@"",@"currentClass":NSStringFromClass(object_getClass(constructorStore)),@"cfPointerWasNonzero":@(constructorCF!=0),@"cfPointerStillSame":@(constructorCF && constructorCF==currentCF),@"totalFixtureDeletes":@(atomic_load(&totalFixtureDeletes)),@"totalRemoveSince":@(atomic_load(&totalRemoveSince)),@"mutationFrames":mutationFrames?:@[],@"constructorObjectStillCurrent":@(constructorObjectStillCurrent),@"oldStoreStillHadAAtStart":@(oldStoreStillHadAAtStart),@"nativeCookieClearObserved":@(atomic_load(&nativeClearObserved)),@"expectNativeStartupClear":@(self.expectStartupClear),@"scope":self.expectStartupClear?@"System cookie namespaces with verified native startup cleanup; not account switching":@"Synthetic native cookie storage and real local HTTP only"};NSData *json=[NSJSONSerialization dataWithJSONObject:result options:0 error:NULL];const char *output=getenv("LOADER_COOKIE_RESULT");if(output)[json writeToFile:[NSString stringWithUTF8String:output] options:NSDataWritingAtomic error:NULL];fprintf(stdout,"HTTP_COOKIE_FIXTURE %s\n",[[NSString alloc]initWithData:json encoding:NSUTF8StringEncoding].UTF8String);fflush(stdout);dispatch_after(dispatch_time(DISPATCH_TIME_NOW,5*NSEC_PER_SEC),dispatch_get_main_queue(),^{[NSApp terminate:nil];});return;
 }
 NSDictionary *step=self.steps[self.step++];NSURLSession *session=step[@"session"];NSString *path=step[@"path"],*key=step[@"check"];id expected=step[@"expected"];
 [[session dataTaskWithURL:[NSURL URLWithString:[self.base stringByAppendingString:path]] completionHandler:^(NSData *data,NSURLResponse *response,NSError *error){
  (void)response;id payload=data?[NSJSONSerialization JSONObjectWithData:data options:0 error:NULL]:nil;id value=[payload isKindOfClass:NSDictionary.class]?payload[@"marker"]:nil;
  self.checks[key]=@(!error && value && [value isEqual:expected]);dispatch_async(dispatch_get_main_queue(),^{[self next];});
 }] resume];
}
@end
static HTTPCookieFixture *fixture;
int loader_plugin_init_v1(const loader_api_v1 *api){
 if(!api || api->abi_version!=LOADER_ABI_V1 || !NSThread.isMainThread)return 1;
 const char *marker=getenv("LOADER_COOKIE_MARKER"),*base=getenv("LOADER_COOKIE_BASE");if(!marker || !base)return 2;
 fixture=[HTTPCookieFixture new];fixture.marker=[NSString stringWithUTF8String:marker];fixture.base=[NSString stringWithUTF8String:base];fixture.reopen=getenv("LOADER_COOKIE_REOPEN")!=NULL;fixture.expectStartupClear=getenv("LOADER_COOKIE_EXPECT_NATIVE_CLEAR")!=NULL;fixture.checks=[NSMutableDictionary dictionary];
 dispatch_after(dispatch_time(DISPATCH_TIME_NOW,5*NSEC_PER_SEC),dispatch_get_main_queue(),^{[fixture start];});return 0;
}
