#import <Foundation/Foundation.h>
#include "loader_api.h"
#include <unistd.h>
static loader_editor_dispatch handler;static NSDictionary *last;
int socket_gate_scope(int scope){(void)scope;return 0;}
int loader_register_capability_provider(const char *name,loader_editor_dispatch dispatch,void *context,const char *caps){(void)name;(void)context;(void)caps;handler=dispatch;return 0;}
extern int loader_http_capability_install(void);
static void receive(const char *bytes,size_t length,void *context){(void)context;last=[NSJSONSerialization JSONObjectWithData:[NSData dataWithBytes:bytes length:length] options:0 error:NULL];}
static NSDictionary *call(const char *method,NSDictionary *args){NSData *data=[NSJSONSerialization dataWithJSONObject:args options:0 error:NULL];NSString *text=[[NSString alloc]initWithData:data encoding:NSUTF8StringEncoding];last=nil;handler(method,text.UTF8String,receive,NULL,NULL);return last;}
static NSDictionary *waitFor(NSString *identifier){for(int i=0;i<500;i++){NSDictionary *value=call("http.status",@{@"requestId":identifier});if(![value[@"result"][@"pending"] boolValue])return value;usleep(10000);}return @{};}
int main(int argc,char **argv){@autoreleasepool{if(argc!=2)return 2;loader_http_capability_install();NSString *url=@(argv[1]);NSMutableDictionary *checks=[NSMutableDictionary new];
 NSString *identifier=call("http.start",@{@"url":[url stringByAppendingString:@"/slow"]})[@"result"][@"requestId"];if(!identifier)return 3;
 checks[@"pendingBeforeCancel"]=@([call("http.status",@{@"requestId":identifier})[@"result"][@"pending"] boolValue]);usleep(150000);
 checks[@"cancelAccepted"]=@([call("http.cancel",@{@"requestId":identifier})[@"result"][@"cancelRequested"] boolValue]);
 checks[@"repeatCancelAccepted"]=@([call("http.cancel",@{@"requestId":identifier})[@"ok"] boolValue]);
 NSDictionary *terminal=waitFor(identifier);checks[@"transportCancelled"]=@([terminal[@"result"][@"response"][@"error"][@"code"] isEqual:@"HTTP_CANCELLED"]);
 checks[@"terminalConsumedOnce"]=@([call("http.status",@{@"requestId":identifier})[@"error"][@"code"] isEqual:@"HTTP_REQUEST_NOT_FOUND"]);
 checks[@"cancelConsumedIsMissing"]=@([call("http.cancel",@{@"requestId":identifier})[@"error"][@"code"] isEqual:@"HTTP_REQUEST_NOT_FOUND"]);
 NSString *second=call("http.start",@{@"url":[url stringByAppendingString:@"/fast"]})[@"result"][@"requestId"];usleep(200000);
 NSDictionary *cancel=call("http.cancel",@{@"requestId":second});checks[@"completedCancelDoesNotChangeResult"]=@([cancel[@"result"][@"completed"] boolValue] && ![cancel[@"result"][@"cancelRequested"] boolValue]);terminal=waitFor(second);checks[@"nextRequestUnaffected"]=@([terminal[@"result"][@"response"][@"ok"] boolValue]);
 checks[@"badCancelRejected"]=@([call("http.cancel",@{@"requestId":@[]})[@"error"][@"code"] isEqual:@"INVALID_HTTP_REQUEST"]);
 NSString *third=call("http.start",@{@"url":[url stringByAppendingString:@"/early"]})[@"result"][@"requestId"];call("http.cancel",@{@"requestId":third});terminal=waitFor(third);checks[@"immediateCancel"]=@([terminal[@"result"][@"response"][@"error"][@"code"] isEqual:@"HTTP_CANCELLED"]);
 BOOL passed=YES;for(NSNumber *value in checks.allValues)passed=passed&&value.boolValue;NSData *result=[NSJSONSerialization dataWithJSONObject:@{@"checks":checks,@"passed":@(passed),@"scope":@"Real resident libcurl cancellation in standalone native process; not official JS acceptance"} options:NSJSONWritingPrettyPrinted error:NULL];fwrite(result.bytes,1,result.length,stdout);return passed?0:1;}}
