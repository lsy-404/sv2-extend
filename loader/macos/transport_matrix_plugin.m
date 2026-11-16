#import <Foundation/Foundation.h>
#import <WebKit/WebKit.h>
#import <Network/Network.h>
#import <Security/Security.h>
#include "loader_api.h"
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/uio.h>
#include <unistd.h>
#include <errno.h>
static const loader_api_v1 *loader;
static NSString *base,*webBase;
static int phase,holdFD=-1,duplicateFD=-1,preConnect=-1,preConnectX=-1,preUDP=-1;
static WKWebView *firstView,*currentView;
static void advance(void);
static struct sockaddr_in endpoint(unsigned port) { struct sockaddr_in a={0};a.sin_len=sizeof a;a.sin_family=AF_INET;a.sin_port=htons(port);inet_pton(AF_INET,"127.0.0.1",&a.sin_addr);return a; }
static void record(const char *name,long result) {fprintf(stderr,"MATRIX phase=%d case=%s result=%ld\n",phase,name,result);fflush(stderr);}
static long tcpRequest(BOOL extended,BOOL ipv6) {
    NSURL *url=[NSURL URLWithString:base];int fd=socket(ipv6?AF_INET6:AF_INET,SOCK_STREAM,0);if(fd<0)return -errno;
    struct timeval timeout={2,0};setsockopt(fd,SOL_SOCKET,SO_RCVTIMEO,&timeout,sizeof timeout);
    struct sockaddr_storage address={0};socklen_t size;
    if(ipv6){struct sockaddr_in6 a={0};a.sin6_len=sizeof a;a.sin6_family=AF_INET6;a.sin6_port=htons((unsigned short)atoi(getenv("LOADER_MATRIX_HTTP6_PORT")));inet_pton(AF_INET6,"::1",&a.sin6_addr);memcpy(&address,&a,sizeof a);size=sizeof a;}
    else {struct sockaddr_in a=endpoint(url.port.unsignedIntValue);memcpy(&address,&a,sizeof a);size=sizeof a;}
    int result;
    if(extended){sa_endpoints_t ends={0};ends.sae_dstaddr=(struct sockaddr*)&address;ends.sae_dstaddrlen=size;result=connectx(fd,&ends,SAE_ASSOCID_ANY,0,NULL,0,NULL,NULL);}
    else result=connect(fd,(struct sockaddr*)&address,size);
    long outcome=-errno;
    if(!result){char request[256];int count=snprintf(request,sizeof request,"GET /phase%d-%s HTTP/1.0\r\nHost: localhost\r\n\r\n",phase,ipv6?"ipv6":extended?"connectx":"tcp");write(fd,request,(size_t)count);char response[256];outcome=recv(fd,response,sizeof response,0)>0?200:-1;}
    close(fd);return outcome;
}
static void rawTests(void) {
    record("tcp",tcpRequest(NO,NO));record("connectx",tcpRequest(YES,NO));record("ipv6",tcpRequest(NO,YES));
    struct sockaddr_in udp=endpoint((unsigned)atoi(getenv("LOADER_MATRIX_UDP_PORT")));
    char payload[64];int count=snprintf(payload,sizeof payload,"phase%d-udp",phase);
    int fd=phase==1?preUDP:socket(AF_INET,SOCK_DGRAM,0);
    ssize_t sent=sendto(fd,payload,(size_t)count,0,(struct sockaddr*)&udp,sizeof udp);record("udp-sendto",sent>=0?1:-errno);
    struct iovec iov={payload,(size_t)count};struct msghdr message={0};message.msg_name=&udp;message.msg_namelen=sizeof udp;message.msg_iov=&iov;message.msg_iovlen=1;
    sent=sendmsg(fd,&message,0);record("udp-sendmsg",sent>=0?1:-errno);if(phase!=1 && fd>=0)close(fd);
    if(phase==0) {
        struct sockaddr_in hold=endpoint((unsigned)atoi(getenv("LOADER_MATRIX_HOLD_PORT")));
        holdFD=socket(AF_INET,SOCK_STREAM,0);if(connect(holdFD,(struct sockaddr*)&hold,sizeof hold)==0){send(holdFD,"before",6,0);char ack[4];recv(holdFD,ack,sizeof ack,0);}
        duplicateFD=dup(holdFD);
        preConnect=socket(AF_INET,SOCK_STREAM,0);preConnectX=socket(AF_INET,SOCK_STREAM,0);preUDP=socket(AF_INET,SOCK_DGRAM,0);
    }
    if(phase==1) {
        struct sockaddr_in target=endpoint([NSURL URLWithString:base].port.unsignedIntValue);
        record("precreated-connect",connect(preConnect,(struct sockaddr*)&target,sizeof target));
        sa_endpoints_t ends={0};ends.sae_dstaddr=(struct sockaddr*)&target;ends.sae_dstaddrlen=sizeof target;
        record("precreated-connectx",connectx(preConnectX,&ends,SAE_ASSOCID_ANY,0,NULL,0,NULL,NULL));
        record("existing-send",send(holdFD,"after",5,0));record("existing-write",write(holdFD,"after",5));
        struct iovec item={(void*)"after",5};record("existing-writev",writev(holdFD,&item,1));
        record("duplicate-send",send(duplicateFD,"after",5,0));
        char bytes[4];record("existing-recv",recv(holdFD,bytes,sizeof bytes,MSG_DONTWAIT));record("duplicate-read",read(duplicateFD,bytes,sizeof bytes));
    }
}
static void networkFrameworkTest(void) {
    NSURL *url=[NSURL URLWithString:base];char port[16];snprintf(port,sizeof port,"%u",url.port.unsignedIntValue);
    nw_parameters_t parameters=nw_parameters_create_secure_tcp(NW_PARAMETERS_DISABLE_PROTOCOL,NW_PARAMETERS_DEFAULT_CONFIGURATION);
    nw_connection_t connection=nw_connection_create(nw_endpoint_create_host("127.0.0.1",port),parameters);
    dispatch_semaphore_t completed=dispatch_semaphore_create(0);__block long result=-1;
    nw_connection_set_queue(connection,dispatch_get_global_queue(QOS_CLASS_UTILITY,0));
    nw_connection_set_state_changed_handler(connection,^(nw_connection_state_t state,nw_error_t error){
        (void)error;
        if(state==nw_connection_state_ready){
            NSData *data=[[NSString stringWithFormat:@"GET /phase%d-network-framework HTTP/1.0\r\nHost: localhost\r\n\r\n",phase] dataUsingEncoding:NSUTF8StringEncoding];
            dispatch_data_t content=dispatch_data_create(data.bytes,data.length,dispatch_get_global_queue(QOS_CLASS_UTILITY,0),DISPATCH_DATA_DESTRUCTOR_DEFAULT);
            nw_connection_send(connection,content,NW_CONNECTION_DEFAULT_MESSAGE_CONTEXT,true,^(nw_error_t sendError){
                if(sendError){dispatch_semaphore_signal(completed);return;}
                nw_connection_receive(connection,1,1024,^(dispatch_data_t received,nw_content_context_t context,bool done,nw_error_t receiveError){(void)context;(void)done;(void)receiveError;if(received && dispatch_data_get_size(received))result=200;dispatch_semaphore_signal(completed);});
            });
        } else if(state==nw_connection_state_failed || state==nw_connection_state_waiting)dispatch_semaphore_signal(completed);
    });
    nw_connection_start(connection);dispatch_semaphore_wait(completed,dispatch_time(DISPATCH_TIME_NOW,3000000000));nw_connection_cancel(connection);record("network-framework",result);
}
@interface MatrixNavigation : NSObject<WKNavigationDelegate>
@end
static MatrixNavigation *navigation;
static void finishWebCase(void) {
    if(phase==1 || phase==2) {
        [firstView callAsyncJavaScript:[NSString stringWithFormat:@"try {let r=await fetch('/phase%d-subresource',{cache:'no-store'});return r.status;}catch(e){return -1;}",phase] arguments:@{} inFrame:nil inContentWorld:WKContentWorld.pageWorld completionHandler:^(id result,NSError *error){record("existing-page-fetch",error?-2:[result longValue]);advance();}];
    } else advance();
}
@implementation MatrixNavigation
-(void)webView:(WKWebView*)view didReceiveAuthenticationChallenge:(NSURLAuthenticationChallenge*)challenge completionHandler:(void(^)(NSURLSessionAuthChallengeDisposition,NSURLCredential*))completion {
    (void)view;
    const char *fixture=getenv("LOADER_MATRIX_TLS_DER");
    if(fixture && [challenge.protectionSpace.host isEqualToString:@"loader-audit.test"] && [challenge.protectionSpace.authenticationMethod isEqualToString:NSURLAuthenticationMethodServerTrust]) {
        SecTrustRef trust=challenge.protectionSpace.serverTrust;
        CFArrayRef certificates=SecTrustCopyCertificateChain(trust);
        if(certificates && CFArrayGetCount(certificates)>0) {
            SecCertificateRef leaf=(SecCertificateRef)CFArrayGetValueAtIndex(certificates,0);
            NSData *actual=CFBridgingRelease(SecCertificateCopyData(leaf));
            NSData *expected=[NSData dataWithContentsOfFile:[NSString stringWithUTF8String:fixture]];
            if([actual isEqualToData:expected]) {CFRelease(certificates);completion(NSURLSessionAuthChallengeUseCredential,[NSURLCredential credentialForTrust:trust]);return;}
        }
        if(certificates)CFRelease(certificates);
    }
    completion(NSURLSessionAuthChallengePerformDefaultHandling,nil);
}
-(void)webView:(WKWebView*)view didFinishNavigation:(WKNavigation*)item {(void)view;(void)item;record("webkit",200);finishWebCase();}
-(void)webView:(WKWebView*)view didFailProvisionalNavigation:(WKNavigation*)item withError:(NSError*)error {(void)view;(void)item;record("webkit",error.code);finishWebCase();}
@end
static void executePhase(void) {
    loader->set_host_network_blocked(phase==1);
    dispatch_after(dispatch_time(DISPATCH_TIME_NOW,300000000),dispatch_get_global_queue(QOS_CLASS_UTILITY,0),^{
        rawTests();networkFrameworkTest();
        dispatch_async(dispatch_get_main_queue(),^{
            currentView=[[WKWebView alloc] initWithFrame:NSMakeRect(0,0,320,180) configuration:[WKWebViewConfiguration new]];currentView.navigationDelegate=navigation;
            if(phase==0)firstView=currentView;
            WKNavigation *item=[currentView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:[webBase stringByAppendingFormat:@"/phase%d-webkit",phase]]]];
            if(!item){record("webkit",-1);finishWebCase();}
        });
    });
}
static void advance(void) {
    if(++phase<3)executePhase();else {if(holdFD>=0)close(holdFD);if(duplicateFD>=0)close(duplicateFD);if(preConnect>=0)close(preConnect);if(preConnectX>=0)close(preConnectX);if(preUDP>=0)close(preUDP);loader->log("transport-matrix-complete");}
}
int loader_plugin_init_v1(const loader_api_v1 *api) {
    if(!api || api->abi_version!=LOADER_ABI_V1 || api->struct_size<sizeof(*api))return 1;
    loader=api;const char *endpoint=getenv("LOADER_PROBE_HTTP_BASE");if(!endpoint)return 1;base=[NSString stringWithUTF8String:endpoint];webBase=getenv("LOADER_MATRIX_WEBKIT_BASE")?[NSString stringWithUTF8String:getenv("LOADER_MATRIX_WEBKIT_BASE")]:base;navigation=[MatrixNavigation new];
    dispatch_after(dispatch_time(DISPATCH_TIME_NOW,500000000),dispatch_get_main_queue(),^{executePhase();});return 0;
}
