#import <Foundation/Foundation.h>
#import <AppKit/AppKit.h>
#include "loader_api.h"
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
static const loader_api_v1 *loader;
static NSURLSession *session;
static NSString *base;
static NSPanel *panel;
static NSTextField *status;
@interface ProbeControls : NSObject
-(void)allow:(id)sender;
-(void)block:(id)sender;
@end
@implementation ProbeControls
-(void)allow:(id)sender { (void)sender; loader->set_host_network_blocked(0); status.stringValue=@"NSURLSession gate: ALLOWED"; }
-(void)block:(id)sender { (void)sender; loader->set_host_network_blocked(1); status.stringValue=@"NSURLSession gate: BLOCKED"; }
@end
static ProbeControls *controls;
static void showPanel(void) {
    controls=[ProbeControls new];
    panel=[[NSPanel alloc] initWithContentRect:NSMakeRect(160,160,480,180) styleMask:NSWindowStyleMaskTitled|NSWindowStyleMaskClosable backing:NSBackingStoreBuffered defer:NO];
    panel.title=@"Open SV2 Loader - Native Probe"; panel.releasedWhenClosed=NO; panel.level=NSFloatingWindowLevel; panel.hidesOnDeactivate=NO; panel.worksWhenModal=YES;
    NSTextField *title=[NSTextField labelWithString:[NSString stringWithFormat:@"Resident in SV2 PID %d | ABI %u",getpid(),loader->abi_version]];
    title.frame=NSMakeRect(20,130,440,22); [panel.contentView addSubview:title];
    status=[NSTextField labelWithString:loader->host_network_blocked() ? @"NSURLSession gate: BLOCKED" : @"NSURLSession gate: ALLOWED"];
    status.frame=NSMakeRect(20,100,440,22); [panel.contentView addSubview:status];
    NSTextField *scope=[NSTextField labelWithString:@"Experimental copy; NSURLSession only; no firewall guarantee."];
    scope.frame=NSMakeRect(20,72,440,22); [panel.contentView addSubview:scope];
    NSButton *allow=[NSButton buttonWithTitle:@"Allow host requests" target:controls action:@selector(allow:)]; allow.frame=NSMakeRect(20,20,200,32); [panel.contentView addSubview:allow];
    NSButton *block=[NSButton buttonWithTitle:@"Block host requests" target:controls action:@selector(block:)]; block.frame=NSMakeRect(240,20,220,32); [panel.contentView addSubview:block];
    [panel makeKeyAndOrderFront:nil];
}
static void pluginComplete(int32_t statusCode,int32_t errorCode,const uint8_t *data,size_t length,void *context) {
    (void)data; (void)length;
    void (^next)(NSInteger) = CFBridgingRelease(context);
    NSInteger code=errorCode ? errorCode : statusCode;
    fprintf(stderr,"NETWORK_PROBE pid=%d label=plugin-while-blocked result=%ld\n",getpid(),(long)code); fflush(stderr);
    dispatch_async(dispatch_get_main_queue(), ^{ next(code); });
}
static void request(NSString *label, BOOL exempt, void(^next)(NSInteger)) {
    NSURL *url=[NSURL URLWithString:[base stringByAppendingFormat:@"/%@",label]];
    if(exempt) {
        void *context=(__bridge_retained void*)[next copy];
        if(loader->plugin_http_get(url.absoluteString.UTF8String,pluginComplete,context)!=0) { CFRelease(context); next(-1); }
        return;
    }
    NSURLSessionDataTask *task=[session dataTaskWithURL:url completionHandler:^(NSData *data,NSURLResponse *response,NSError *error) {
        (void)data;
        NSInteger code=error ? error.code : ((NSHTTPURLResponse*)response).statusCode;
        fprintf(stderr,"NETWORK_PROBE pid=%d label=%s result=%ld\n",getpid(),label.UTF8String,(long)code); fflush(stderr);
        dispatch_async(dispatch_get_main_queue(), ^{ next(code); });
    }];
    [task resume];
}
int loader_plugin_init_v1(const loader_api_v1 *api) {
    if(!api || api->abi_version!=LOADER_ABI_V1 || api->struct_size<sizeof(*api) || api->host_pid!=getpid() || !pthread_main_np()) return 1;
    loader=api; loader->log("sample-plugin-api-ok");
    char marker[128]; int markerLength=snprintf(marker,sizeof(marker),"{\"hostPid\":%d,\"abiVersion\":%u}\n",getpid(),api->abi_version);
    int storageResult=loader->plugin_storage_write("startup.json",(const uint8_t*)marker,(size_t)markerLength);
    fprintf(stderr,"LOADER_PLUGIN_STORAGE pid=%d result=%d\n",getpid(),storageResult);
    const char *startBlocked=getenv("LOADER_PROBE_BLOCK_HOST_AT_START");
    if(startBlocked && strcmp(startBlocked,"1")==0) loader->set_host_network_blocked(1);
    const char *endpoint=getenv("LOADER_PROBE_HTTP_BASE"); if(!endpoint) { dispatch_after(dispatch_time(DISPATCH_TIME_NOW,2000000000),dispatch_get_main_queue(),^{ showPanel(); }); return 0; }
    base=[NSString stringWithUTF8String:endpoint];
    NSURLSessionConfiguration *config=[NSURLSessionConfiguration ephemeralSessionConfiguration];
    config.timeoutIntervalForRequest=3; config.requestCachePolicy=NSURLRequestReloadIgnoringLocalCacheData;
    session=[NSURLSession sessionWithConfiguration:config];
    request(@"allowed",NO,^(NSInteger c1){
        request(@"slow",NO,^(NSInteger c2){
            request(@"blocked",NO,^(NSInteger c3){
                request(@"plugin-while-blocked",YES,^(NSInteger c4){
                    loader->set_host_network_blocked(0);
                    request(@"restored",NO,^(NSInteger c5){
                        BOOL ok=c1==200 && c2==NSURLErrorCancelled && c3==NSURLErrorCancelled && c4==200 && c5==200 && !loader->host_network_blocked();
                        loader->log(ok ? "network-toggle-selftest-ok" : "network-toggle-selftest-failed");
                        [session finishTasksAndInvalidate];
                    });
                });
            });
        });
        dispatch_after(dispatch_time(DISPATCH_TIME_NOW,200000000),dispatch_get_main_queue(),^{ loader->set_host_network_blocked(1); });
    });
    return 0;
}
