#include "web_store_registry.h"
#import <Foundation/Foundation.h>
#import <WebKit/WebKit.h>
#import <Network/Network.h>
#import <objc/runtime.h>
#include "socket_gate.h"
#include "website_data_namespace.h"
#include "webkit_rtc_gate.h"
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <poll.h>
#include <unistd.h>
#include <stdatomic.h>
static uint16_t proxyPort;
static NSString *proxyPassword;
static NSHashTable<WKWebView*> *views;
static _Atomic unsigned activeHostRelays;
static BOOL installed;
static WKContentRuleList *blockRules;
static _Atomic unsigned pendingPolicyUpdates;
static BOOL receiveBytes(int fd,void *buffer,size_t count) {
    unsigned char *p=buffer;while(count){ssize_t n=recv(fd,p,count,0);if(n<=0)return NO;p+=n;count-=(size_t)n;}return YES;
}
static BOOL sendBytes(int fd,const void *buffer,size_t count) {
    const unsigned char *p=buffer;while(count){ssize_t n=socket_gate_send(fd,p,count,0);if(n<=0)return NO;p+=n;count-=(size_t)n;}return YES;
}
static void reply(int fd,unsigned char code) {unsigned char bytes[10]={5,code,0,1,0,0,0,0,0,0};sendBytes(fd,bytes,sizeof bytes);}
static void handle_connection(int client) {
    struct timeval timeout={5,0};setsockopt(client,SOL_SOCKET,SO_RCVTIMEO,&timeout,sizeof timeout);
    unsigned char prefix[2],methods[255];
    if(!receiveBytes(client,prefix,2) || prefix[0]!=5 || !prefix[1] || !receiveBytes(client,methods,prefix[1]))return;
    BOOL passwordAuth=NO;for(unsigned i=0;i<prefix[1];i++)if(methods[i]==2)passwordAuth=YES;
    unsigned char selection[2]={5,passwordAuth?2:255};if(!sendBytes(client,selection,2) || !passwordAuth)return;
    unsigned char user[256]={0},password[256]={0},passwordLength;
    if(!receiveBytes(client,prefix,2) || prefix[0]!=1 || !receiveBytes(client,user,prefix[1]) || !receiveBytes(client,&passwordLength,1) || !receiveBytes(client,password,passwordLength))return;
    BOOL authenticated=!strcmp((char*)user,"host") && !strcmp((char*)password,proxyPassword.UTF8String);
    unsigned char result[2]={1,authenticated?0:1};if(!sendBytes(client,result,2) || !authenticated)return;
    unsigned char request[4],raw[256]={0},portBytes[2];char hostname[INET6_ADDRSTRLEN+256]={0};
    if(!receiveBytes(client,request,4) || request[0]!=5 || request[1]!=1){reply(client,7);return;}
    if(request[3]==1){if(!receiveBytes(client,raw,4))return;inet_ntop(AF_INET,raw,hostname,sizeof hostname);}
    else if(request[3]==4){if(!receiveBytes(client,raw,16))return;inet_ntop(AF_INET6,raw,hostname,sizeof hostname);}
    else if(request[3]==3){unsigned char count;if(!receiveBytes(client,&count,1) || !count || !receiveBytes(client,hostname,count))return;}
    else {reply(client,8);return;}
    if(!receiveBytes(client,portBytes,2))return;
    if(socket_gate_is_blocked()){reply(client,2);fprintf(stderr,"WEBKIT_GATE denied-tunnel\n");return;}
    char port[16];snprintf(port,sizeof port,"%u",((unsigned)portBytes[0]<<8)|portBytes[1]);
    atomic_fetch_add(&activeHostRelays,1);
    struct addrinfo hints={0},*addresses=NULL;hints.ai_socktype=SOCK_STREAM;hints.ai_family=AF_UNSPEC;
    int remote=-1;socket_gate_scope(SOCKET_SCOPE_HOST_RELAY);
#ifdef LOADER_GATE_TESTING
    if(!strcmp(hostname,"loader-audit.test"))snprintf(hostname,sizeof hostname,"127.0.0.1");
#endif
    if(getaddrinfo(hostname,port,&hints,&addresses)==0) {
        for(struct addrinfo *a=addresses;a;a=a->ai_next) {
            remote=socket_gate_open(a->ai_family,a->ai_socktype,a->ai_protocol);if(remote<0)continue;
            if(socket_gate_connect(remote,a->ai_addr,a->ai_addrlen)==0)break;socket_gate_close(remote);remote=-1;
        }freeaddrinfo(addresses);
    }
    socket_gate_scope(SOCKET_SCOPE_CONTROL);
    if(remote>=0) {
        fprintf(stderr,"WEBKIT_GATE connected-tunnel\n");
        reply(client,0);struct pollfd pairs[2]={{client,POLLIN,0},{remote,POLLIN,0}};
        while(!socket_gate_is_blocked() && poll(pairs,2,1000)>=0) {
            BOOL stop=NO;
            for(int i=0;i<2;i++)if(pairs[i].revents) {
                char buffer[16384];ssize_t n=recv(pairs[i].fd,buffer,sizeof buffer,0);if(n<=0){stop=YES;break;}
                if(socket_gate_is_blocked() || !sendBytes(pairs[1-i].fd,buffer,(size_t)n)){stop=YES;break;}
            }
            if(stop)break;
        }socket_gate_close(remote);
    } else reply(client,5);
    atomic_fetch_sub(&activeHostRelays,1);
}
static void relay(int client) {
    int previous=socket_gate_scope(SOCKET_SCOPE_CONTROL);
    @autoreleasepool { handle_connection(client); }
    socket_gate_close(client);socket_gate_scope(previous);
}
static void configure(WKWebsiteDataStore *store) {
    if(@available(macOS 14.0,*)) {
        char port[16];snprintf(port,sizeof port,"%u",proxyPort);
        nw_proxy_config_t proxy=nw_proxy_config_create_socksv5(nw_endpoint_create_host("127.0.0.1",port));
        nw_proxy_config_set_username_and_password(proxy,"host",proxyPassword.UTF8String);
        nw_proxy_config_set_failover_allowed(proxy,false);
        nw_proxy_config_add_match_domain(proxy,"");
        nw_proxy_config_clear_excluded_domains(proxy);
        store.proxyConfigurations=@[proxy];
        fprintf(stderr,"WEBKIT_GATE configured-store proxy-count=%lu\n",(unsigned long)store.proxyConfigurations.count);
    }
}
int webkit_gate_install(void) {
    if(installed)return 0;
    if(@available(macOS 14.0,*)) {} else return -1;
    const char *isolate=getenv("OPEN_SV2_ISOLATE_WEBSITE_DATA");
    if(isolate && !strcmp(isolate,"1") && loader_website_data_namespace_install(getenv("OPEN_SV2_LOADER_DATA_DIR"))!=0)return -1;
    if(loader_web_store_tracking_install()!=0)fprintf(stderr,"WEBKIT_CONTEXT store-tracking-unavailable=1\n");
    views=[NSHashTable weakObjectsHashTable];proxyPassword=NSUUID.UUID.UUIDString;
    if(loader_webkit_rtc_install()!=0)fprintf(stderr,"WEBKIT_RTC native-new-connection-guard=0 existing-revocation-verified=0\n");
    int previous=socket_gate_scope(SOCKET_SCOPE_CONTROL);
    int listener=socket_gate_open(AF_INET,SOCK_STREAM,0);struct sockaddr_in address={0};address.sin_len=sizeof address;address.sin_family=AF_INET;address.sin_addr.s_addr=htonl(INADDR_LOOPBACK);
    if(listener<0 || bind(listener,(struct sockaddr*)&address,sizeof address)<0 || listen(listener,32)<0){if(listener>=0)socket_gate_close(listener);socket_gate_scope(previous);return -1;}
    socklen_t size=sizeof address;getsockname(listener,(struct sockaddr*)&address,&size);proxyPort=ntohs(address.sin_port);
    dispatch_async(dispatch_get_global_queue(QOS_CLASS_UTILITY,0),^{
        socket_gate_scope(SOCKET_SCOPE_CONTROL);
        for(;;){int client=accept(listener,NULL,NULL);if(client<0)break;socket_gate_adopt(client,SOCKET_SCOPE_CONTROL);dispatch_async(dispatch_get_global_queue(QOS_CLASS_UTILITY,0),^{relay(client);});}
    });
    socket_gate_scope(previous);
    Method method=class_getInstanceMethod([WKWebView class],@selector(initWithFrame:configuration:));
    IMP original=method_getImplementation(method);
    method_setImplementation(method,imp_implementationWithBlock(^id(WKWebView *object,NSRect frame,WKWebViewConfiguration *config){
        config.websiteDataStore=loader_website_data_namespace_map(config.websiteDataStore);
        loader_web_store_register(config.websiteDataStore);
        configure(config.websiteDataStore);
        BOOL requestedRTC=loader_webkit_rtc_prepare(config);
        WKWebView *result=((id(*)(id,SEL,NSRect,id))original)(object,@selector(initWithFrame:configuration:),frame,config);
        if(result) {
            loader_web_store_register(result.configuration.websiteDataStore);
            loader_webkit_rtc_register(result,requestedRTC);
            [views addObject:result];
            if(socket_gate_is_blocked() && blockRules)[result.configuration.userContentController addContentRuleList:blockRules];
        }
        return result;
    }));
    Method load=class_getInstanceMethod([WKWebView class],@selector(loadRequest:));
    IMP originalLoad=method_getImplementation(load);
    method_setImplementation(load,imp_implementationWithBlock(^id(WKWebView *view,NSURLRequest *request){
        if(socket_gate_is_blocked()) { fprintf(stderr,"WEBKIT_GATE denied-navigation\n");return nil; }
        return ((id(*)(id,SEL,id))originalLoad)(view,@selector(loadRequest:),request);
    }));
    configure([WKWebsiteDataStore defaultDataStore]);
    atomic_fetch_add(&pendingPolicyUpdates,1);
    NSString *rules=@"[{\"trigger\":{\"url-filter\":\".*\"},\"action\":{\"type\":\"block\"}}]";
    [[WKContentRuleListStore defaultStore] compileContentRuleListForIdentifier:@"OpenSV2Loader.HostOffline.v1" encodedContentRuleList:rules completionHandler:^(WKContentRuleList *list,NSError *error){
        (void)error;blockRules=list;
        if(list && socket_gate_is_blocked())for(WKWebView *view in [views allObjects])[view.configuration.userContentController addContentRuleList:list];
        atomic_fetch_sub(&pendingPolicyUpdates,1);
    }];
    installed=YES;return 0;
}
void webkit_gate_update_policy(void) {
    atomic_fetch_add(&pendingPolicyUpdates,1);
    void (^update)(void)=^{
        for(WKWebView *view in [views allObjects]) {
            loader_webkit_rtc_update(view);
            if(socket_gate_is_blocked()) {
                [view stopLoading];
                if(blockRules)[view.configuration.userContentController addContentRuleList:blockRules];
            } else if(blockRules)[view.configuration.userContentController removeContentRuleList:blockRules];
        }
        atomic_fetch_sub(&pendingPolicyUpdates,1);
    };
    if(NSThread.isMainThread)update();else dispatch_async(dispatch_get_main_queue(),update);
}
uint32_t webkit_gate_pending_updates(void) {return atomic_load(&pendingPolicyUpdates)+(blockRules?0:1);}
uint32_t webkit_gate_active_relays(void) { return atomic_load(&activeHostRelays); }
