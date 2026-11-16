// Native Lua call shim. os.getenv is used ONLY as an existing Lua->C trampoline:
// reserved calls never read or write the process environment. All other getenv
// calls retain libc behavior. No Lua VM internals or version-specific addresses.
#import <Foundation/Foundation.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dlfcn.h>
extern int loader_javascript_gate_ready(void);
extern NSDictionary *loader_export_call(NSString*,NSDictionary*);
extern NSDictionary *loader_voice_call(NSString*,NSDictionary*);
extern NSDictionary *loader_lua_editor_call(NSString *method,NSDictionary *arguments);
extern NSDictionary *loader_plugin_ui_call(NSString*,NSDictionary*);
extern NSDictionary *loader_lua_capability_call(NSString *method,NSDictionary *arguments);
extern NSDictionary *loader_capability_snapshot(void);
static _Atomic int enabled;
static _Thread_local char *replyBuffer;
static const char prefix[]="__OPEN_SV2_NATIVE_CALL_V1__:";
static char *respond(NSDictionary *value) {
    NSData *data=[NSJSONSerialization dataWithJSONObject:value options:0 error:NULL];
    if(!data || data.length>1048576)return NULL;
    char *next=malloc(data.length+1);if(!next)return NULL;
    memcpy(next,data.bytes,data.length);next[data.length]=0;free(replyBuffer);replyBuffer=next;return next;
}
int loader_lua_call_gate_install(void) {if(!pthread_main_np())return -1;atomic_store(&enabled,1);return 0;}
const char *loader_script_call_json_for_runtime(const char *body,size_t length,int javascript) {
    if(!atomic_load(&enabled))return NULL;
    @autoreleasepool {
        if(!pthread_main_np())return respond(@{@"ok":@NO,@"error":@{@"code":@"MAIN_THREAD_REQUIRED"}});
        if(length>1048576)return respond(@{@"ok":@NO,@"error":@{@"code":@"REQUEST_TOO_LARGE"}});
        id request=[NSJSONSerialization JSONObjectWithData:[NSData dataWithBytes:body length:length] options:0 error:NULL];
        if(![request isKindOfClass:NSDictionary.class] || ![request[@"method"] isKindOfClass:NSString.class])return respond(@{@"ok":@NO,@"error":@{@"code":@"INVALID_ARGUMENT"}});
        NSString *method=request[@"method"];
        id arguments=request[@"arguments"]?:@{};if(![arguments isKindOfClass:NSDictionary.class])return respond(@{@"ok":@NO,@"error":@{@"code":@"INVALID_ARGUMENT"}});
        if(length>262144 && ![method isEqualToString:@"editor.reply"])return respond(@{@"ok":@NO,@"error":@{@"code":@"REQUEST_TOO_LARGE"}});
        int (*allowed)(const char*,const char*)=dlsym(RTLD_DEFAULT,"loader_plugin_ui_feature_allowed");
        NSString *cleanupTarget=[method isEqual:@"call.submit"]?arguments[@"method"]:method;
        BOOL finishing=[cleanupTarget isKindOfClass:NSString.class]&&[@[@"http.status",@"http.cancel",@"exports.status",@"exports.cancel",@"export.status",@"export.cancel"] containsObject:cleanupTarget];
        // These guarded primitives serve Loader's built-in editor provider,
        // including MCP; user-facing Lua/JS function switches must not disable it.
        BOOL internal=[@[@"voice.native_status",@"voice.catalog",@"voice.capture.begin",@"voice.capture.end",@"voice.select.begin",@"voice.select.validate",@"voice.select.end",@"export.native_status",@"export.capture.begin",@"export.capture.end"] containsObject:method];
        BOOL control=internal||[method hasPrefix:@"ui."]||[method hasPrefix:@"editor."]||[method isEqual:@"runtime.info"]||[method isEqual:@"call.result"];
        const char *identifier=javascript?"opensv2.javascript-extend":"opensv2.lua-extend";
        if(!control&&allowed){
            if(!finishing&&!allowed(identifier,"enabled"))return respond(@{@"ok":@NO,@"error":@{@"code":@"EXTENSION_DISABLED"}});
            NSString *target=[method isEqual:@"call.submit"]?arguments[@"method"]:method;
            if([target isKindOfClass:NSString.class]&&[target hasPrefix:@"voice."])target=[@"voicebanks." stringByAppendingString:[target substringFromIndex:6]];
            if([target isKindOfClass:NSString.class]&&[target hasPrefix:@"export."])target=[@"exports." stringByAppendingString:[target substringFromIndex:7]];
            if([target isKindOfClass:NSString.class]&&!allowed(identifier,target.UTF8String))return respond(@{@"ok":@NO,@"error":@{@"code":@"FEATURE_DISABLED"}});
        }
        if([method hasPrefix:@"ui."])return respond(loader_plugin_ui_call(method,request[@"arguments"]));
        if([method hasPrefix:@"export."])return respond(loader_export_call(method,request[@"arguments"]));
        if([method hasPrefix:@"voice."])return respond(loader_voice_call(method,request[@"arguments"]));
        if([method hasPrefix:@"editor."])return respond(loader_lua_editor_call(method,request[@"arguments"]));
        if([method hasPrefix:@"call."])return respond(loader_lua_capability_call(method,request[@"arguments"]));
        if([method isEqualToString:@"runtime.capabilities"]){
            NSDictionary *snapshot=loader_capability_snapshot();NSMutableDictionary *result=[snapshot[@"capabilities"] mutableCopy];
            result[@"capabilityGeneration"]=snapshot[@"capabilityGeneration"];
            return respond(@{@"ok":@YES,@"result":result});
        }
        if([method isEqualToString:@"runtime.info"])return respond(@{@"ok":@YES,@"result":@{@"pid":@(getpid()),@"mainThread":@YES,@"apiVersion":@1,@"transport":@"in-process-call",@"usesEnvironmentState":@NO,@"usesFilesOrPipes":@NO,@"javascriptNativeAvailable":(loader_javascript_gate_ready()?@YES:@NO)}});
#ifdef LOADER_LUA_CALL_TESTING
        if([method isEqualToString:@"probe.echo"])return respond(@{@"ok":@YES,@"result":request[@"arguments"]?:@{}});
#endif
        return respond(@{@"ok":@NO,@"error":@{@"code":@"UNSUPPORTED_HOST_CAPABILITY"}});
    }
}
const char *loader_script_call_json(const char *body,size_t length){return loader_script_call_json_for_runtime(body,length,0);}
static char *native_getenv(const char *name) {
    if(!atomic_load(&enabled) || !name || strncmp(name,prefix,sizeof prefix-1))return getenv(name);
    const char *body=name+sizeof prefix-1;
    return (char*)loader_script_call_json(body,strnlen(body,1048577));
}
__attribute__((used)) static struct { const void *replacement;const void *replacee; } interpose_getenv
__attribute__((section("__DATA,__interpose")))={(const void*)native_getenv,(const void*)getenv};
#ifdef LOADER_LUA_CALL_TESTING
__attribute__((constructor)) static void test_install(void) {atomic_store(&enabled,1);fprintf(stderr,"LUA_CALL_GATE test_installed pid=%d\n",getpid());}
#endif
