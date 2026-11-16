// Disposable integration fixture. Never packaged in Loader.app.
#import <Foundation/Foundation.h>
#import <WebKit/WebKit.h>
#include "loader_api.h"
@interface WebsiteDataFixture:NSObject<WKNavigationDelegate>
@property NSMutableDictionary *checks;
@property NSArray<WKWebsiteDataStore*> *stores;
@property WKWebView *view;
@property NSString *marker,*base;
@property BOOL reopening;
@property NSUInteger index;
-(void)cookies;
-(void)storage;
@end
@implementation WebsiteDataFixture
-(NSString*)expectedBefore {return self.reopening && self.index<2?@"A":nil;}
-(void)cookies {
 if(self.index==3){self.index=0;[self storage];return;}
 NSUInteger index=self.index;WKWebsiteDataStore *store=self.stores[index];NSString *expected=[self expectedBefore];
 [store.httpCookieStore getAllCookies:^(NSArray<NSHTTPCookie*> *cookies){
  NSString *before=nil;for(NSHTTPCookie *cookie in cookies)if([cookie.name isEqual:@"svext_namespace_probe"])before=cookie.value;
  self.checks[[NSString stringWithFormat:@"cookie%luBefore",(unsigned long)index]]=@(expected?[before isEqual:expected]:!before);
  NSHTTPCookie *cookie=[NSHTTPCookie cookieWithProperties:@{NSHTTPCookieName:@"svext_namespace_probe",NSHTTPCookieValue:self.marker,NSHTTPCookieDomain:@"127.0.0.1",NSHTTPCookiePath:@"/",NSHTTPCookieExpires:[NSDate dateWithTimeIntervalSinceNow:86400]}];
  [store.httpCookieStore setCookie:cookie completionHandler:^{[store.httpCookieStore getAllCookies:^(NSArray<NSHTTPCookie*> *values){
   BOOL found=NO;for(NSHTTPCookie *value in values)if([value.name isEqual:cookie.name] && [value.value isEqual:self.marker])found=YES;
   self.checks[[NSString stringWithFormat:@"cookie%luWriteRead",(unsigned long)index]]=@(found);self.index++;[self cookies];
  }];}];
 }];
}
-(void)storage {
 if(self.index==3){
  self.view=nil;BOOL passed=YES;for(NSNumber *value in self.checks.allValues)if(!value.boolValue)passed=NO;
  NSDictionary *report=@{@"checks":self.checks,@"passed":@(passed),@"pid":@(getpid()),@"defaultIdentifier":self.stores[0].identifier.UUIDString?:@"",@"namedIdentifier":self.stores[1].identifier.UUIDString?:@"",@"scope":@"Synthetic cookies/localStorage only; no real account data read or copied"};
  NSData *data=[NSJSONSerialization dataWithJSONObject:report options:0 error:NULL];fprintf(stdout,"WEBSITE_DATA_FIXTURE %s\n",[[NSString alloc]initWithData:data encoding:NSUTF8StringEncoding].UTF8String);fflush(stdout);return;
 }
 WKWebViewConfiguration *config=[WKWebViewConfiguration new];config.websiteDataStore=self.stores[self.index];self.view=[[WKWebView alloc]initWithFrame:NSMakeRect(0,0,400,300) configuration:config];self.view.navigationDelegate=self;
 self.checks[[NSString stringWithFormat:@"view%luStoreMatches",(unsigned long)self.index]]=@(self.index==2?!self.view.configuration.websiteDataStore.isPersistent:[self.view.configuration.websiteDataStore.identifier isEqual:self.stores[self.index].identifier]);
 [self.view loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:[self.base stringByAppendingString:@"/fixture"]]]];
}
-(void)webView:(WKWebView*)webView didFinishNavigation:(WKNavigation*)navigation {
 (void)navigation;NSUInteger index=self.index;NSString *expected=[self expectedBefore];
 [webView evaluateJavaScript:@"localStorage.getItem('svext_namespace_probe')" completionHandler:^(id before,NSError *error){
  self.checks[[NSString stringWithFormat:@"storage%luBefore",(unsigned long)index]]=@(!error && (expected?[before isEqual:expected]:(before==nil || before==NSNull.null)));
  NSString *script=[NSString stringWithFormat:@"localStorage.setItem('svext_namespace_probe','%@');localStorage.getItem('svext_namespace_probe')",self.marker];
  [webView evaluateJavaScript:script completionHandler:^(id result,NSError *writeError){self.checks[[NSString stringWithFormat:@"storage%luWriteRead",(unsigned long)index]]=@(!writeError && [result isEqual:self.marker]);self.index++;[self storage];}];
 }];
}
-(void)failed:(NSError*)error {(void)error;self.checks[[NSString stringWithFormat:@"navigation%luSucceeded",(unsigned long)self.index]]=@NO;self.index++;[self storage];}
-(void)webView:(WKWebView*)view didFailNavigation:(WKNavigation*)navigation withError:(NSError*)error {(void)view;(void)navigation;[self failed:error];}
-(void)webView:(WKWebView*)view didFailProvisionalNavigation:(WKNavigation*)navigation withError:(NSError*)error {(void)view;(void)navigation;[self failed:error];}
@end
static WebsiteDataFixture *fixture;
int loader_plugin_init_v1(const loader_api_v1 *api){
 if(!api || api->abi_version!=LOADER_ABI_V1 || !NSThread.isMainThread)return 1;
 if(@available(macOS 14.0,*)){
  const char *marker=getenv("LOADER_WEBSITE_MARKER"),*base=getenv("LOADER_WEBSITE_BASE");if(!marker || !base || (strcmp(marker,"A") && strcmp(marker,"B")))return 2;
  fixture=[WebsiteDataFixture new];fixture.marker=[NSString stringWithUTF8String:marker];fixture.base=[NSString stringWithUTF8String:base];fixture.reopening=getenv("LOADER_WEBSITE_REOPEN")!=NULL;fixture.checks=[NSMutableDictionary dictionary];
  NSUUID *original=[[NSUUID alloc]initWithUUIDString:@"B820859B-C9F8-41C3-BAC7-B16B69C71E6C"];WKWebsiteDataStore *defaults=WKWebsiteDataStore.defaultDataStore,*named=[WKWebsiteDataStore dataStoreForIdentifier:original],*temporary=WKWebsiteDataStore.nonPersistentDataStore;
  fixture.stores=@[defaults,named,temporary];fixture.checks[@"defaultHasProfileIdentifier"]=@(defaults.identifier!=nil);fixture.checks[@"namedIdentifierRemapped"]=@(![named.identifier isEqual:original]);fixture.checks[@"defaultAndNamedDistinct"]=@(![defaults.identifier isEqual:named.identifier]);fixture.checks[@"namedFactoryStable"]=@([[WKWebsiteDataStore dataStoreForIdentifier:original].identifier isEqual:named.identifier]);fixture.checks[@"mappedIdentifierIdempotent"]=@([[WKWebsiteDataStore dataStoreForIdentifier:named.identifier].identifier isEqual:named.identifier]);fixture.checks[@"nonpersistentPreserved"]=@(!temporary.isPersistent && !temporary.identifier);
  BOOL rejectedZero=NO;@try{[WKWebsiteDataStore dataStoreForIdentifier:[[NSUUID alloc]initWithUUIDString:@"00000000-0000-0000-0000-000000000000"]];}@catch(NSException *exception){(void)exception;rejectedZero=YES;}fixture.checks[@"zeroIdentifierStillRejected"]=@(rejectedZero);
  dispatch_async(dispatch_get_main_queue(),^{[fixture cookies];});return 0;
 }return 3;
}
