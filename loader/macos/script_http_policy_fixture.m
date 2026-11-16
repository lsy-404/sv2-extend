#import <Foundation/Foundation.h>
#include "loader_api.h"
static const loader_api_v1 *loader;
static NSMutableDictionary *answers;
static void received(const char *bytes,size_t length,void *context){
 NSString *key=(__bridge_transfer NSString*)context;id value=[NSJSONSerialization JSONObjectWithData:[NSData dataWithBytes:bytes length:length] options:0 error:NULL];answers[key]=value?:@{};
 if(answers.count==2){NSDictionary *report=@{@"pid":@(getpid()),@"localEnvironment":@((getenv("OPEN_SV2_SCRIPT_HTTP_LOCAL")?:"")),@"onlineEnvironment":@((getenv("OPEN_SV2_SCRIPT_HTTP_ONLINE")?:"")),@"responses":answers};NSData *data=[NSJSONSerialization dataWithJSONObject:report options:NSJSONWritingPrettyPrinted error:NULL];[data writeToFile:@(getenv("LOADER_POLICY_TEST_RESULT")) atomically:YES];}
}
int loader_plugin_init_v1(const loader_api_v1 *api){loader=api;answers=[NSMutableDictionary new];dispatch_after(dispatch_time(DISPATCH_TIME_NOW,NSEC_PER_SEC),dispatch_get_main_queue(),^{
 for(NSString *key in @[@"local",@"online"]){NSString *url=[key isEqual:@"local"]?@(getenv("LOADER_POLICY_TEST_URL")):@"https://example.com/";NSData *data=[NSJSONSerialization dataWithJSONObject:@{@"url":url} options:0 error:NULL];NSString *args=[[NSString alloc]initWithData:data encoding:NSUTF8StringEncoding];loader->request_capability_json("http.get",args.UTF8String,received,(__bridge_retained void*)key);}
 });return 0;}
