#import <Foundation/Foundation.h>
#include "resident_http.h"
@implementation ResidentHTTPCancellation
@end

#include "socket_gate.h"
#include <curl/curl.h>
static int transferProgress(void *context,curl_off_t downTotal,curl_off_t downNow,curl_off_t upTotal,curl_off_t upNow) {
    (void)downTotal;(void)downNow;(void)upTotal;(void)upNow;
    return [(__bridge ResidentHTTPCancellation*)context cancelled]?1:0;
}
static size_t pluginCurlWrite(char *bytes,size_t size,size_t count,void *context) {
    NSMutableData *body=(__bridge NSMutableData*)context;
    size_t length=size*count;
    if(length>8*1024*1024 || body.length+length>8*1024*1024)return 0;
    [body appendBytes:bytes length:length];return length;
}
int resident_http_request(const char *url, const char *method, NSDictionary *headers, NSData *payload, ResidentHTTPCancellation *cancellation, loader_http_completion completion, void *context) {
    if(!url || !completion)return LOADER_ERR_INVALID;
    NSString *text=[NSString stringWithUTF8String:url];NSURL *target=[NSURL URLWithString:text];
    if(!target || !([target.scheme isEqualToString:@"http"] || [target.scheme isEqualToString:@"https"]))return LOADER_ERR_INVALID;
    NSString *verb=[NSString stringWithUTF8String:method ?: "GET"];
    NSDictionary *fields=[headers copy];NSData *data=[payload copy];
    dispatch_async(dispatch_get_global_queue(QOS_CLASS_UTILITY,0),^{
        @autoreleasepool {
            int previous=socket_gate_scope(SOCKET_SCOPE_PLUGIN);
            CURL *curl=curl_easy_init();NSMutableData *body=[NSMutableData data];long status=0;CURLcode result=CURLE_FAILED_INIT;
            if(curl) {
                if(cancellation){curl_easy_setopt(curl,CURLOPT_NOPROGRESS,0L);curl_easy_setopt(curl,CURLOPT_XFERINFOFUNCTION,transferProgress);curl_easy_setopt(curl,CURLOPT_XFERINFODATA,(__bridge void*)cancellation);}
                curl_easy_setopt(curl,CURLOPT_URL,text.UTF8String);curl_easy_setopt(curl,CURLOPT_TIMEOUT,8L);
                curl_easy_setopt(curl,CURLOPT_NOSIGNAL,1L);curl_easy_setopt(curl,CURLOPT_PROXY,"");
                curl_easy_setopt(curl,CURLOPT_PROTOCOLS_STR,"http,https");curl_easy_setopt(curl,CURLOPT_FOLLOWLOCATION,0L);
                curl_easy_setopt(curl,CURLOPT_WRITEFUNCTION,pluginCurlWrite);curl_easy_setopt(curl,CURLOPT_WRITEDATA,(__bridge void*)body);
                struct curl_slist *list=NULL;
                for(NSString *name in fields)list=curl_slist_append(list,[[NSString stringWithFormat:@"%@: %@",name,fields[name]] UTF8String]);
                // An explicit empty Expect avoids an unsolicited 100-continue delay.
                list=curl_slist_append(list,"Expect:");curl_easy_setopt(curl,CURLOPT_HTTPHEADER,list);
                curl_easy_setopt(curl,CURLOPT_CUSTOMREQUEST,verb.UTF8String);
                if(data){curl_easy_setopt(curl,CURLOPT_POSTFIELDS,data.bytes ?: "");curl_easy_setopt(curl,CURLOPT_POSTFIELDSIZE_LARGE,(curl_off_t)data.length);}
                if([verb isEqualToString:@"HEAD"])curl_easy_setopt(curl,CURLOPT_NOBODY,1L);
                result=curl_easy_perform(curl);curl_easy_getinfo(curl,CURLINFO_RESPONSE_CODE,&status);curl_easy_cleanup(curl);curl_slist_free_all(list);
            }
            socket_gate_scope(previous);
            completion((int32_t)status,cancellation.cancelled?-20999:result==CURLE_OK?0:-(int32_t)(20000+result),body.bytes,body.length,context);
        }
    });return LOADER_OK;
}

int network_gate_plugin_request(const char *url,const char *method,NSDictionary *headers,NSData *payload,loader_http_completion completion,void *context) {
    return resident_http_request(url,method,headers,payload,nil,completion,context);
}
int network_gate_plugin_get(const char *url,loader_http_completion completion,void *context) {
    return network_gate_plugin_request(url,"GET",nil,nil,completion,context);
}
