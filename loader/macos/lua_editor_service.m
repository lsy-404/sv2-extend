#import <Foundation/Foundation.h>
#include "loader_api.h"
#include <unistd.h>
extern int loader_register_editor_adapter(loader_editor_dispatch,void*,const char*);
extern int loader_unregister_editor_adapter(loader_editor_dispatch,void*);
@interface ResidentEditorJob : NSObject
@property loader_json_completion completion;
@property void *context;
@property NSDictionary *request;
@property BOOL taken;
@end
@implementation ResidentEditorJob
@end
static NSMutableDictionary<NSString*,ResidentEditorJob*> *jobs;
static NSString *epoch;
static NSTimeInterval heartbeat;
static NSArray *methods;
BOOL loader_lua_editor_online(void) {
    return NSThread.isMainThread && epoch && NSProcessInfo.processInfo.systemUptime-heartbeat<=2;
}
static NSDictionary *failure(NSString *code) {return @{@"ok":@NO,@"error":@{@"code":code}};}
static NSDictionary *success(id result) {return @{@"ok":@YES,@"result":result?:@{}};}
static void reply(ResidentEditorJob *job,NSDictionary *value) {
    NSData *data=[NSJSONSerialization dataWithJSONObject:value options:0 error:NULL];job.completion(data.bytes,data.length,job.context);
}
static BOOL noNulls(id value) {
    if(value==NSNull.null)return NO;
    if([value isKindOfClass:NSArray.class]){for(id item in value)if(!noNulls(item))return NO;}
    else if([value isKindOfClass:NSDictionary.class]){for(id item in [value allValues])if(!noNulls(item))return NO;}
    return YES;
}
static void invoke(const char *name,const char *arguments,loader_json_completion complete,void *context,void *adapterContext) {
    (void)adapterContext;
    ResidentEditorJob *job=[ResidentEditorJob new];job.completion=complete;job.context=context;
    if(!NSThread.isMainThread || !epoch || NSProcessInfo.processInfo.systemUptime-heartbeat>2){reply(job,failure(@"EDITOR_PROVIDER_OFFLINE"));return;}
    NSData *data=[[NSString stringWithUTF8String:arguments] dataUsingEncoding:NSUTF8StringEncoding];id args=[NSJSONSerialization JSONObjectWithData:data options:0 error:NULL];
    if(![args isKindOfClass:NSDictionary.class] || !noNulls(args)){reply(job,failure(@"INVALID_ARGUMENT"));return;}
    if(jobs.count>=32){reply(job,failure(@"EDITOR_BUSY"));return;}
    NSString *identifier=NSUUID.UUID.UUIDString;
    job.request=@{@"id":identifier,@"epoch":epoch,@"method":[NSString stringWithUTF8String:name],@"arguments":args,@"expires":@((long long)NSDate.date.timeIntervalSince1970+4)};
    jobs[identifier]=job;
    dispatch_after(dispatch_time(DISPATCH_TIME_NOW,6000000000),dispatch_get_main_queue(),^{ResidentEditorJob *late=jobs[identifier];if(late){[jobs removeObjectForKey:identifier];reply(late,failure(@"EDITOR_TIMEOUT"));}});
}
static void monitor(NSString *generation) {
    dispatch_after(dispatch_time(DISPATCH_TIME_NOW,500000000),dispatch_get_main_queue(),^{
        if(![epoch isEqualToString:generation])return;
        if(NSProcessInfo.processInfo.systemUptime-heartbeat<=2){monitor(generation);return;}
        NSArray *abandoned=jobs.allValues;[jobs removeAllObjects];
        loader_unregister_editor_adapter(invoke,NULL);epoch=nil;methods=nil;
        for(ResidentEditorJob *job in abandoned)reply(job,failure(@"EDITOR_PROVIDER_OFFLINE"));
    });
}
NSDictionary *loader_lua_editor_call(NSString *method,NSDictionary *arguments) {
    if(!NSThread.isMainThread)return failure(@"MAIN_THREAD_REQUIRED");
    if(![arguments isKindOfClass:NSDictionary.class])return failure(@"INVALID_ARGUMENT");
    if([method isEqualToString:@"editor.attach"]) {
        if(epoch)return failure(@"EDITOR_PROVIDER_ALREADY_ATTACHED");
        NSArray *requested=arguments[@"methods"];
        NSSet *supported=[NSSet setWithArray:@[@"selection.read",@"notes.set_pitch",@"pitch.read_range",@"pitch.replace_range",@"transport.status",@"transport.play",@"transport.stop",@"voicebanks.current",@"voicebanks.project",@"voicebanks.list",@"voicebanks.select",@"exports.snapshot",@"exports.start",@"exports.status",@"exports.cancel"]];
        if(![requested isKindOfClass:NSArray.class] || !requested.count || requested.count>supported.count)return failure(@"INVALID_ARGUMENT");
        for(id name in requested)if(![name isKindOfClass:NSString.class] || ![supported containsObject:name])return failure(@"UNSUPPORTED_HOST_CAPABILITY");
        NSDictionary *caps=@{@"methods":requested,@"transport":@"resident-native-call",@"provider":@"official-lua",@"editorVersion":arguments[@"editorVersion"]?:@0};
        NSData *data=[NSJSONSerialization dataWithJSONObject:caps options:0 error:NULL];NSString *encoded=[[NSString alloc] initWithData:data encoding:NSUTF8StringEncoding];
        if(loader_register_editor_adapter(invoke,NULL,encoded.UTF8String)!=LOADER_OK)return failure(@"EDITOR_REGISTRATION_REJECTED");
        jobs=[NSMutableDictionary dictionary];epoch=NSUUID.UUID.UUIDString;heartbeat=NSProcessInfo.processInfo.systemUptime;methods=[requested copy];monitor(epoch);
        fprintf(stderr,"LUA_EDITOR attached pid=%d transport=resident-native-call\n",getpid());
        return success(@{@"epoch":epoch,@"methods":methods});
    }
    if(!epoch || ![arguments[@"epoch"] isEqualToString:epoch])return failure(@"EDITOR_EPOCH_MISMATCH");
    heartbeat=NSProcessInfo.processInfo.systemUptime;
    if([method isEqualToString:@"editor.next"]) {
        for(ResidentEditorJob *job in jobs.allValues)if(!job.taken){job.taken=YES;return success(@{@"request":job.request});}
        return success(@{});
    }
    if([method isEqualToString:@"editor.reply"]) {
        NSString *identifier=arguments[@"id"];NSDictionary *value=arguments[@"response"];
        if(![identifier isKindOfClass:NSString.class] || ![value isKindOfClass:NSDictionary.class] || !value[@"ok"] || CFGetTypeID((__bridge CFTypeRef)value[@"ok"])!=CFBooleanGetTypeID())return failure(@"INVALID_ARGUMENT");
        ResidentEditorJob *job=jobs[identifier];if(!job || !job.taken)return failure(@"EDITOR_REQUEST_EXPIRED");
        [jobs removeObjectForKey:identifier];reply(job,value);return success(@{});
    }
    return failure(@"UNSUPPORTED_HOST_CAPABILITY");
}
