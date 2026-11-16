#import <Foundation/Foundation.h>
#include "resident_http.h"
@implementation ResidentHTTPCancellation
@end
static loader_editor_dispatch handler;
static NSDictionary *last;
int loader_register_capability_provider(const char *name,loader_editor_dispatch dispatch,void *context,const char *caps){(void)name;(void)context;(void)caps;handler=dispatch;return 0;}
int network_gate_plugin_get(const char *url,loader_http_completion completion,void *context){(void)url;completion(200,0,(const uint8_t*)"OK",2,context);return 0;}
int network_gate_plugin_request(const char *url,const char *method,NSDictionary *headers,NSData *body,loader_http_completion completion,void *context){(void)method;(void)headers;(void)body;return network_gate_plugin_get(url,completion,context);}
int resident_http_request(const char *url,const char *method,NSDictionary *headers,NSData *body,ResidentHTTPCancellation *cancel,loader_http_completion completion,void *context){(void)cancel;return network_gate_plugin_request(url,method,headers,body,completion,context);}
extern int loader_http_capability_install(void);
static void receive(const char *bytes,size_t length,void *context){(void)context;last=[NSJSONSerialization JSONObjectWithData:[NSData dataWithBytes:bytes length:length] options:0 error:NULL];}
int main(){@autoreleasepool{loader_http_capability_install();NSArray *cases=@[@"[]",@"null",@"123",@"{\"url\":[]}",@"{\"url\":null}",@"{\"url\":\"file:///tmp/x\"}",@"{\"url\":\"http://localhost/\"}"];
 for(NSString *input in cases){last=nil;handler("http.get",input.UTF8String,receive,NULL,NULL);BOOL valid=[input containsString:@"http://localhost/"];if(valid?![last[@"ok"] boolValue]:![last[@"error"][@"code"] isEqualToString:@"INVALID_HTTP_REQUEST"])return 1;}
 puts("7 HTTP argument/dispatch checks passed");}return 0;}
