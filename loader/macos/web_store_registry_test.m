#import <AppKit/AppKit.h>
#import "web_store_registry.h"
#import "website_data_namespace.h"
#include <stdio.h>
static BOOL contains(WKWebsiteDataStore *value){return [loader_web_stores_snapshot() containsObject:value];}
int main(int argc,const char **argv){@autoreleasepool{
 if(argc!=2)return 64;[NSApplication sharedApplication];[NSApp setActivationPolicy:NSApplicationActivationPolicyProhibited];
 NSMutableDictionary *checks=[NSMutableDictionary dictionary];checks[@"namespaceInstalled"]=@(loader_website_data_namespace_install(argv[1])==0);
 __weak WKWebsiteDataStore *control;@autoreleasepool{WKWebsiteDataStore *value=WKWebsiteDataStore.nonPersistentDataStore;control=value;}
 WKWebsiteDataStore *before=[WKWebsiteDataStore dataStoreForIdentifier:NSUUID.UUID];
 checks[@"trackingInstalled"]=@(loader_web_store_tracking_install()==0&&loader_web_store_tracking_supported());
 checks[@"emptyRegistryDoesNotCreateStores"]=@(loader_web_stores_snapshot().count==0);
 WKWebsiteDataStore *defaults=WKWebsiteDataStore.defaultDataStore;checks[@"mappedDefaultObserved"]=@(contains(defaults));
 WKWebsiteDataStore *named=[WKWebsiteDataStore dataStoreForIdentifier:NSUUID.UUID];checks[@"mappedNamedObserved"]=@(contains(named));
 WKWebViewConfiguration *config=[WKWebViewConfiguration new];config.websiteDataStore=before;checks[@"configurationAddsExistingStore"]=@(contains(config.websiteDataStore));
 __weak WKWebsiteDataStore *weakStore;@autoreleasepool{WKWebsiteDataStore *ephemeral=WKWebsiteDataStore.nonPersistentDataStore;weakStore=ephemeral;checks[@"ephemeralObserved"]=@(contains(ephemeral)&&!ephemeral.isPersistent);}
 for(unsigned i=0;i<100&&(weakStore||control);i++)[NSRunLoop.currentRunLoop runUntilDate:[NSDate dateWithTimeIntervalSinceNow:.01]];
 checks[@"nativeControlEphemeralReleased"]=@(control==nil);checks[@"registryDoesNotRetainEphemeral"]=@(weakStore==nil);
 __block BOOL refused=NO;dispatch_semaphore_t done=dispatch_semaphore_create(0);dispatch_async(dispatch_get_global_queue(0,0),^{refused=loader_web_stores_snapshot()==nil;dispatch_semaphore_signal(done);});dispatch_semaphore_wait(done,dispatch_time(DISPATCH_TIME_NOW,NSEC_PER_SEC));checks[@"offMainSnapshotRefused"]=@(refused);
 NSMutableArray *retained=[NSMutableArray array];for(unsigned i=0;i<257;i++)[retained addObject:WKWebsiteDataStore.nonPersistentDataStore];
 checks[@"overflowRefusesAcknowledgement"]=@(!loader_web_store_tracking_supported()&&loader_web_stores_snapshot()==nil);
 NSData *data=[NSJSONSerialization dataWithJSONObject:checks options:NSJSONWritingSortedKeys error:NULL];puts([[NSString alloc]initWithData:data encoding:NSUTF8StringEncoding].UTF8String);return [checks.allValues containsObject:@NO]?1:0;
}}
