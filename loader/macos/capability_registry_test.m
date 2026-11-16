// Isolated registry contract tests. Mock providers are never linked into Loader.
#import <Foundation/Foundation.h>
#include "loader_api.h"
extern int loader_register_capability_provider(const char*,loader_editor_dispatch,void*,const char*);
extern int loader_unregister_capability_provider(const char*,void*);
extern void loader_editor_request(const char*,const char*,loader_json_completion,void*);
@interface Capture : NSObject
@property NSDictionary *value;
@property NSUInteger count;
@end
@implementation Capture
@end
static loader_json_completion held;
static void *heldContext;
static void capture(const char *json,size_t length,void *context) {
    Capture *result=(__bridge Capture*)context;result.count++;result.value=[NSJSONSerialization JSONObjectWithData:[NSData dataWithBytes:json length:length] options:0 error:NULL];
}
static void pump(double seconds) {
    NSDate *end=[NSDate dateWithTimeIntervalSinceNow:seconds];
    while(end.timeIntervalSinceNow>0)[NSRunLoop.currentRunLoop runMode:NSDefaultRunLoopMode beforeDate:[NSDate dateWithTimeIntervalSinceNow:MIN(.01,end.timeIntervalSinceNow)]];
}
static Capture *start(const char *name,const char *args) {Capture *result=[Capture new];loader_editor_request(name,args,capture,(__bridge void*)result);return result;}
static Capture *request(const char *name,const char *args) {Capture *result=start(name,args);for(int i=0;i<800 && !result.count;i++)pump(.01);return result;}
static void provider(const char *name,const char *args,loader_json_completion done,void *context,void *owner) {
    (void)args;
    if(!strcmp(name,"test.delayed") || !strcmp(name,"test.timeout")){held=done;heldContext=context;return;}
    if(!strcmp(name,"test.malformed")){done("[]",2,context);return;}
    if(!strcmp(name,"test.background")){dispatch_async(dispatch_get_global_queue(QOS_CLASS_UTILITY,0),^{done("{\"ok\":true}",11,context);});return;}
    NSString *text=[NSString stringWithFormat:@"{\"ok\":true,\"result\":{\"provider\":\"%s\",\"mainThread\":%@}}",(char*)owner,NSThread.isMainThread?@"true":@"false"];
    done(text.UTF8String,strlen(text.UTF8String),context);
    if(!strcmp(name,"test.duplicate"))done(text.UTF8String,strlen(text.UTF8String),context);
}
int main(void) {@autoreleasepool {
    NSMutableDictionary *checks=[NSMutableDictionary dictionary];
    checks[@"provider-a-registered"]=@(loader_register_capability_provider("test.editor",provider,"editor","{\"methods\":[\"notes.read\"]}")==0);
    checks[@"provider-b-coexists"]=@(loader_register_capability_provider("test.voices",provider,"voices","{\"methods\":[\"voicebanks.current\"]}")==0);
    checks[@"editor-route-main-thread"]=@([request("notes.read","{}").value[@"result"][@"provider"] isEqual:@"editor"] && [request("notes.read","{}").value[@"result"][@"mainThread"] boolValue]);
    checks[@"voice-route-independent"]=@([request("voicebanks.current","{}").value[@"result"][@"provider"] isEqual:@"voices"]);
    checks[@"collision-rejected"]=@(loader_register_capability_provider("test.collision",provider,"collision","{\"methods\":[\"test.leak\",\"notes.read\"]}")!=0);
    checks[@"failed-registration-atomic"]=@([request("test.leak","{}").value[@"error"][@"code"] isEqual:@"UNSUPPORTED_HOST_CAPABILITY"]);
    checks[@"duplicate-method-rejected"]=@(loader_register_capability_provider("test.dupMethods",provider,"duplicate","{\"methods\":[\"test.x\",\"test.x\"]}")!=0);
    checks[@"private-registration-rejected"]=@(loader_register_capability_provider("test.private",provider,"private","{\"methods\":[\"account.switch\"]}")!=0);
    checks[@"private-request-rejected"]=@([request("session.read","{}").value[@"error"][@"code"] isEqual:@"METHOD_NOT_PUBLIC"]);
    checks[@"foreign-owner-unregister-rejected"]=@(loader_unregister_capability_provider("test.voices","wrong")!=0);
    checks[@"wrong-owner-preserves-provider"]=@([request("voicebanks.current","{}").value[@"ok"] boolValue]);
    checks[@"public-snapshot-union"]=@([request("capabilities","{}").value[@"capabilities"][@"methods"] count]==2);
    loader_register_capability_provider("test.lifecycle",provider,"lifecycle","{\"methods\":[\"test.delayed\",\"test.duplicate\",\"test.background\",\"test.malformed\",\"test.timeout\"]}");
    Capture *doubleReply=request("test.duplicate","{}");pump(.05);checks[@"duplicate-completion-suppressed"]=@(doubleReply.count==1);
    checks[@"background-completion-marshalled"]=@([request("test.background","{}").value[@"ok"] boolValue]);
    checks[@"malformed-response-rejected"]=@([request("test.malformed","{}").value[@"error"][@"code"] isEqual:@"INVALID_CAPABILITY_RESPONSE"]);
    checks[@"nonobject-arguments-rejected"]=@([request("notes.read","[]").value[@"error"][@"code"] isEqual:@"INVALID_ARGUMENT"]);
    Capture *delayed=start("test.delayed","{}");pump(.03);checks[@"request-started"]=@(held!=NULL && delayed.count==0);
    checks[@"provider-unregistered"]=@(loader_unregister_capability_provider("test.lifecycle","lifecycle")==0);
    checks[@"inflight-removed-once"]=@(delayed.count==1 && [delayed.value[@"error"][@"code"] isEqual:@"CAPABILITY_PROVIDER_REMOVED"]);
    held("{\"ok\":true}",11,heldContext);pump(.03);checks[@"late-completion-ignored"]=@(delayed.count==1);
    checks[@"other-provider-survives"]=@([request("notes.read","{}").value[@"ok"] boolValue]);
    loader_register_capability_provider("test.timeoutProvider",provider,"timeout","{\"methods\":[\"test.timeout\"]}");
    Capture *timed=request("test.timeout","{}");checks[@"timeout-bounded"]=@(timed.count==1 && [timed.value[@"error"][@"code"] isEqual:@"CAPABILITY_TIMEOUT"]);
    held("{\"ok\":true}",11,heldContext);pump(.03);checks[@"late-after-timeout-ignored"]=@(timed.count==1);
    loader_unregister_capability_provider("test.timeoutProvider","timeout");loader_unregister_capability_provider("test.editor","editor");loader_unregister_capability_provider("test.voices","voices");
    checks[@"empty-registry-not-ready"]=@(![request("capabilities","{}").value[@"editorReady"] boolValue]);
    BOOL passed=YES;for(NSNumber *value in checks.allValues)if(!value.boolValue)passed=NO;
    NSData *data=[NSJSONSerialization dataWithJSONObject:@{@"checks":checks,@"passed":@(passed),@"mock_providers_only":@YES} options:NSJSONWritingPrettyPrinted|NSJSONWritingSortedKeys error:NULL];fwrite(data.bytes,1,data.length,stdout);fputc('\n',stdout);return passed?0:1;
}}
