// Shared resident HTTP capability for scripting frontends. No account/session state.
#import <Foundation/Foundation.h>
#include "resident_http.h"
#include <arpa/inet.h>
extern int network_gate_plugin_get(const char*,loader_http_completion,void*);
extern int network_gate_plugin_request(const char*,const char*,NSDictionary*,NSData*,loader_http_completion,void*);
extern int loader_register_capability_provider(const char*,loader_editor_dispatch,void*,const char*);
@interface ScriptHTTPReply : NSObject
@property loader_json_completion completion;
@property void *context;
@property(atomic) NSDictionary *response;
@property ResidentHTTPCancellation *cancellation;
@end
@implementation ScriptHTTPReply
@end
static NSMutableDictionary<NSString*,ScriptHTTPReply*> *requests;
static void reply(ScriptHTTPReply *job,NSDictionary *value) {
    if(!job.completion){job.response=value;return;}
    NSData *data=[NSJSONSerialization dataWithJSONObject:value options:0 error:NULL];job.completion(data.bytes,data.length,job.context);
}
static void received(int32_t status,int32_t error,const uint8_t *bytes,size_t length,void *context) {
    ScriptHTTPReply *job=CFBridgingRelease(context);
    if(error==-20999){reply(job,@{@"ok":@NO,@"error":@{@"code":@"HTTP_CANCELLED"}});return;}
    if(error){reply(job,@{@"ok":@NO,@"error":@{@"code":@"HTTP_TRANSPORT_ERROR",@"transportCode":@(error)}});return;}
    if(length>524288){reply(job,@{@"ok":@NO,@"error":@{@"code":@"HTTP_RESPONSE_TOO_LARGE"}});return;}
    NSData *body=[NSData dataWithBytes:bytes length:length];NSString *text=[[NSString alloc] initWithData:body encoding:NSUTF8StringEncoding];
    reply(job,@{@"ok":@YES,@"result":@{@"status":@(status),@"body":text?:[body base64EncodedStringWithOptions:0],@"bodyEncoding":text?@"utf8":@"base64",@"byteLength":@(length),@"redirectsFollowed":@NO}});
}
static void invoke(const char *method,const char *arguments,loader_json_completion complete,void *context,void *owner) {
    (void)owner;ScriptHTTPReply *job=[ScriptHTTPReply new];job.completion=complete;job.context=context;
    id args=[NSJSONSerialization JSONObjectWithData:[[NSString stringWithUTF8String:arguments] dataUsingEncoding:NSUTF8StringEncoding] options:0 error:NULL];
    if(![args isKindOfClass:NSDictionary.class]){reply(job,@{@"ok":@NO,@"error":@{@"code":@"INVALID_HTTP_REQUEST"}});return;}
    if(!strcmp(method,"http.status") || !strcmp(method,"http.cancel")) {
        NSString *identifier=args[@"requestId"];
        if([args count]!=1 || ![identifier isKindOfClass:NSString.class] || identifier.length>64){reply(job,@{@"ok":@NO,@"error":@{@"code":@"INVALID_HTTP_REQUEST"}});return;}
        ScriptHTTPReply *active=requests[identifier];
        if(!active){reply(job,@{@"ok":@NO,@"error":@{@"code":@"HTTP_REQUEST_NOT_FOUND"}});return;}
        if(!strcmp(method,"http.cancel")){
            BOOL completed=active.response!=nil;
            if(!completed)active.cancellation.cancelled=YES;
            reply(job,@{@"ok":@YES,@"result":@{@"cancelRequested":@(!completed),@"completed":@(completed)}});return;
        }
        NSDictionary *response=active.response;
        reply(job,@{@"ok":@YES,@"result":response?@{@"pending":@NO,@"response":response}:@{@"pending":@YES}});
        if(response)[requests removeObjectForKey:identifier];
        return;
    }
    NSString *url=args[@"url"];NSURLComponents *parts=[url isKindOfClass:NSString.class]?[NSURLComponents componentsWithString:url]:nil;
    BOOL start=strcmp(method,"http.start")==0;
    BOOL generic=start || strcmp(method,"http.request")==0;
    if((!generic && [args count]!=1) || !parts || ![@[@"http",@"https"] containsObject:parts.scheme.lowercaseString] || !parts.host.length || parts.user || parts.password || parts.fragment || url.length>8192){reply(job,@{@"ok":@NO,@"error":@{@"code":@"INVALID_HTTP_REQUEST"}});return;}
    NSString *host=parts.host.lowercaseString;
    NSString *verb=@"GET";NSDictionary *headers=nil;NSData *payload=nil;
    if(generic){
        NSSet *allowed=[NSSet setWithArray:@[@"url",@"method",@"headers",@"body"]];
        for(id key in args)if(![allowed containsObject:key])goto invalid;
        if(args[@"method"]){if(![args[@"method"] isKindOfClass:NSString.class])goto invalid;verb=args[@"method"];}
        if(![@[@"GET",@"HEAD",@"POST",@"PUT",@"PATCH",@"DELETE",@"OPTIONS"] containsObject:verb])goto invalid;
        headers=args[@"headers"];
        if(headers && (![headers isKindOfClass:NSDictionary.class] || headers.count>32))goto invalid;
        NSUInteger headerBytes=0;
        for(NSString *key in headers){
            id value=headers[key];
            if(![value isKindOfClass:NSString.class] || !key.length || [key rangeOfCharacterFromSet:[[NSCharacterSet characterSetWithCharactersInString:@"!#$%&'*+-.^_`|~0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz"] invertedSet]].location!=NSNotFound)goto invalid;
            if([@[@"host",@"content-length",@"transfer-encoding",@"connection",@"expect",@"proxy-authorization",@"proxy-connection"] containsObject:key.lowercaseString])goto invalid;
            if([value rangeOfCharacterFromSet:[NSCharacterSet controlCharacterSet]].location!=NSNotFound)goto invalid;
            headerBytes += [key lengthOfBytesUsingEncoding:NSUTF8StringEncoding]+[value lengthOfBytesUsingEncoding:NSUTF8StringEncoding];
            if(headerBytes>16384)goto invalid;
        }
        if(args[@"body"]){
            if(![args[@"body"] isKindOfClass:NSString.class] || [@[@"GET",@"HEAD"] containsObject:verb])goto invalid;
            payload=[args[@"body"] dataUsingEncoding:NSUTF8StringEncoding];if(!payload || payload.length>65536)goto invalid;
        }
    }
    // Per-launch script permissions; these do not alter host network policy.
    struct in_addr v4;BOOL loopback4=inet_pton(AF_INET,host.UTF8String,&v4)==1 && (ntohl(v4.s_addr)>>24)==127;
    BOOL local=[host isEqualToString:@"localhost"] || [host hasSuffix:@".localhost"] || loopback4 || [host isEqualToString:@"::1"] || [host isEqualToString:@"[::1]"];
    const char *permission=getenv(local?"OPEN_SV2_SCRIPT_HTTP_LOCAL":"OPEN_SV2_SCRIPT_HTTP_ONLINE");
    if(permission && strcmp(permission,"0")==0){reply(job,@{@"ok":@NO,@"error":@{@"code":@"HTTP_ACCESS_DENIED"}});return;}
    if(start){
        if(!requests)requests=[NSMutableDictionary dictionary];
        if(requests.count>=64){reply(job,@{@"ok":@NO,@"error":@{@"code":@"HTTP_BUSY"}});return;}
        NSString *identifier=NSUUID.UUID.UUIDString;
        ScriptHTTPReply *active=[ScriptHTTPReply new];active.cancellation=[ResidentHTTPCancellation new];requests[identifier]=active;
        void *retained=(__bridge_retained void*)active;
        int result=resident_http_request(url.UTF8String,verb.UTF8String,headers,payload,active.cancellation,received,retained);
        if(result!=LOADER_OK){CFBridgingRelease(retained);[requests removeObjectForKey:identifier];reply(job,@{@"ok":@NO,@"error":@{@"code":@"HTTP_BROKER_UNAVAILABLE"}});return;}
        dispatch_after(dispatch_time(DISPATCH_TIME_NOW,60000000000),dispatch_get_main_queue(),^{ScriptHTTPReply *expired=requests[identifier];expired.cancellation.cancelled=YES;[requests removeObjectForKey:identifier];});
        reply(job,@{@"ok":@YES,@"result":@{@"requestId":identifier}});return;
    }
    void *opaque=(__bridge_retained void*)job;
    int result=generic ? network_gate_plugin_request(url.UTF8String,verb.UTF8String,headers,payload,received,opaque) : network_gate_plugin_get(url.UTF8String,received,opaque);
    if(result!=LOADER_OK){CFBridgingRelease(opaque);reply(job,@{@"ok":@NO,@"error":@{@"code":@"HTTP_BROKER_UNAVAILABLE"}});}
    return;
invalid:
    reply(job,@{@"ok":@NO,@"error":@{@"code":@"INVALID_HTTP_REQUEST"}});
}
int loader_http_capability_install(void) {
    return loader_register_capability_provider("loader.http",invoke,NULL,"{\"methods\":[\"http.get\",\"http.request\",\"http.start\",\"http.status\",\"http.cancel\"],\"transport\":\"resident-plugin-http\",\"limits\":{\"responseBytes\":524288,\"requestTimeoutSeconds\":8,\"redirects\":false},\"tlsVerification\":true}");
}
