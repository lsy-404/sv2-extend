/* Deliberately bypass the Loader HTTP API to audit the scope of host transport gating. */
#import <Foundation/Foundation.h>
#import <AppKit/AppKit.h>
#import <WebKit/WebKit.h>
#include "loader_api.h"
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>
#include <stdio.h>
static const loader_api_v1 *loader;
static WKWebView *view;
static NSURLSession *session;
@interface CoverageNavigation : NSObject <WKNavigationDelegate>
@end
@implementation CoverageNavigation
-(void)webView:(WKWebView*)webView didFinishNavigation:(WKNavigation*)navigation {
    (void)webView;(void)navigation;
    fprintf(stderr,"COVERAGE_PROBE pid=%d webkit-finished blocked=%d\n",getpid(),loader->host_network_blocked());fflush(stderr);
}
-(void)webView:(WKWebView*)webView didFailProvisionalNavigation:(WKNavigation*)navigation withError:(NSError*)error {
    (void)webView;(void)navigation;
    fprintf(stderr,"COVERAGE_PROBE pid=%d webkit-error=%ld\n",getpid(),(long)error.code);fflush(stderr);
}
@end
static CoverageNavigation *navigationDelegate;
int loader_plugin_init_v1(const loader_api_v1 *api) {
    if(!api || api->abi_version!=LOADER_ABI_V1 || api->struct_size<sizeof(*api))return 1;
    loader=api;
    const char *base=getenv("LOADER_PROBE_HTTP_BASE");if(!base)return 1;
    NSString *endpoint=[NSString stringWithUTF8String:base];
    loader->set_host_network_blocked(1);
    loader_network_state_v1 state={.abi_version=LOADER_ABI_V1,.struct_size=sizeof(state)};
    int snapshotResult=loader->get_host_network_state(&state);
    int readiness=loader->verify_host_network_quiescence(state.generation,&state);
    int stale=loader->verify_host_network_quiescence(state.generation+1,&state);
    fprintf(stderr,"COVERAGE_PROBE pid=%d state-result=%d quiescence-result=%d stale-result=%d verified=%u\n",getpid(),snapshotResult,readiness,stale,state.whole_host_verified);fflush(stderr);
    session=[NSURLSession sessionWithConfiguration:[NSURLSessionConfiguration ephemeralSessionConfiguration]];
    [[session dataTaskWithURL:[NSURL URLWithString:[endpoint stringByAppendingString:@"/urlsession-control"]] completionHandler:^(NSData *data,NSURLResponse *response,NSError *error) {
        (void)data;(void)response;
        fprintf(stderr,"COVERAGE_PROBE pid=%d urlsession-error=%ld blocked=%d\n",getpid(),(long)error.code,loader->host_network_blocked());fflush(stderr);
    }] resume];
    dispatch_async(dispatch_get_global_queue(QOS_CLASS_UTILITY,0),^{
        NSURL *url=[NSURL URLWithString:endpoint];int fd=socket(AF_INET,SOCK_STREAM,0);
        struct timeval timeout={3,0};setsockopt(fd,SOL_SOCKET,SO_RCVTIMEO,&timeout,sizeof(timeout));
        struct sockaddr_in address={0};address.sin_family=AF_INET;address.sin_port=htons(url.port.unsignedShortValue);inet_pton(AF_INET,"127.0.0.1",&address.sin_addr);
        int connected=connect(fd,(struct sockaddr*)&address,sizeof(address));
        ssize_t received=-1;
        if(connected==0) {
            const char request[]="GET /socket-bypass HTTP/1.0\r\nHost: localhost\r\n\r\n";
            send(fd,request,sizeof(request)-1,0);char response[256];received=recv(fd,response,sizeof(response),0);
        }
        if(fd>=0)close(fd);
        fprintf(stderr,"COVERAGE_PROBE pid=%d socket-connect=%d response-bytes=%zd blocked=%d\n",getpid(),connected,received,loader->host_network_blocked());fflush(stderr);
    });
    navigationDelegate=[CoverageNavigation new];
    view=[[WKWebView alloc] initWithFrame:NSMakeRect(0,0,320,180) configuration:[WKWebViewConfiguration new]];
    view.navigationDelegate=navigationDelegate;
    [view loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:[endpoint stringByAppendingString:@"/webkit-bypass"]]]];
    loader->log("coverage-audit-started-policy-blocked");
    return 0;
}
