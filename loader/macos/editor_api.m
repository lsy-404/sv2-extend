#import <Foundation/Foundation.h>
#include "loader_api.h"
#include <unistd.h>
@interface CapabilityProvider : NSObject
@property NSString *identifier;
@property NSDictionary *capabilities;
@property loader_editor_dispatch dispatch;
@property void *context;
@end
@implementation CapabilityProvider
@end
@interface CapabilityRequest : NSObject
@property CapabilityProvider *provider;
@property loader_json_completion completion;
@property void *context;
@end
@implementation CapabilityRequest
@end
static NSMutableDictionary<NSString*,CapabilityProvider*> *providers;
static NSMutableDictionary<NSString*,CapabilityProvider*> *routes;
static NSMutableDictionary<NSNumber*,CapabilityRequest*> *requests;
static uint64_t generation=1,nextToken=1;
static void initialize(void) {if(!providers){providers=[NSMutableDictionary dictionary];routes=[NSMutableDictionary dictionary];requests=[NSMutableDictionary dictionary];}}
static void respond(NSDictionary *value,loader_json_completion completion,void *context) {
    NSData *data=[NSJSONSerialization dataWithJSONObject:value options:0 error:NULL];completion(data.bytes,data.length,context);
}
static NSDictionary *errorValue(NSString *code) {return @{@"ok":@NO,@"error":@{@"code":code}};}
static BOOL privateMethod(NSString *name) {
    for(NSString *domain in @[@"account",@"session",@"license",@"private"])
        if([name isEqualToString:domain] || [name hasPrefix:[domain stringByAppendingString:@"."]])return YES;
    return NO;
}
static BOOL validName(NSString *value) {
    if(!value || value.length>96)return NO;
    return [value rangeOfString:@"^[A-Za-z][A-Za-z0-9_.-]*$" options:NSRegularExpressionSearch].location!=NSNotFound;
}
int loader_register_capability_provider(const char *identifier,loader_editor_dispatch dispatch,void *context,const char *json) {
    if(!NSThread.isMainThread || !dispatch || !json || !identifier || strnlen(json,65537)>65536 || strnlen(identifier,97)>96)return LOADER_ERR_INVALID;
    initialize();NSString *name=[NSString stringWithUTF8String:identifier];
    if(!validName(name) || providers[name] || providers.count>=32)return LOADER_ERR_INVALID;
    NSString *encoded=[NSString stringWithUTF8String:json];if(!encoded)return LOADER_ERR_INVALID;
    id value=[NSJSONSerialization JSONObjectWithData:[encoded dataUsingEncoding:NSUTF8StringEncoding] options:0 error:NULL];
    if(![value isKindOfClass:NSDictionary.class] || ![value[@"methods"] isKindOfClass:NSArray.class])return LOADER_ERR_INVALID;
    NSArray *methods=value[@"methods"];if(!methods.count || methods.count>256)return LOADER_ERR_INVALID;
    NSMutableSet *unique=[NSMutableSet set];
    for(id method in methods){
        if(![method isKindOfClass:NSString.class] || !validName(method) || privateMethod(method) || [method isEqualToString:@"capabilities"] || routes[method] || [unique containsObject:method])return LOADER_ERR_INVALID;
        [unique addObject:method];
    }
    if(value[@"tools"]){
        if(![value[@"tools"] isKindOfClass:NSArray.class] || [value[@"tools"] count]>256)return LOADER_ERR_INVALID;
        NSMutableSet *toolNames=[NSMutableSet set];
        for(CapabilityProvider *existing in providers.allValues)for(NSDictionary *tool in existing.capabilities[@"tools"])[toolNames addObject:tool[@"name"]];
        for(id tool in value[@"tools"]){
            if(![tool isKindOfClass:NSDictionary.class])return LOADER_ERR_INVALID;
            NSString *toolName=tool[@"name"];NSString *method=tool[@"method"];
            if(![toolName isKindOfClass:NSString.class] || ![toolName hasPrefix:@"svext_"] || !validName(toolName) || [toolNames containsObject:toolName] || ![method isKindOfClass:NSString.class] || ![unique containsObject:method] || ![tool[@"description"] isKindOfClass:NSString.class] || ![tool[@"inputSchema"] isKindOfClass:NSDictionary.class] || ![tool[@"inputSchema"][@"type"] isEqual:@"object"])return LOADER_ERR_INVALID;
            [toolNames addObject:toolName];
        }
    }
    CapabilityProvider *provider=[CapabilityProvider new];provider.identifier=name;provider.capabilities=value;provider.dispatch=dispatch;provider.context=context;
    providers[name]=provider;for(NSString *method in methods)routes[method]=provider;generation++;return LOADER_OK;
}
int loader_unregister_capability_provider(const char *identifier,void *context) {
    if(!NSThread.isMainThread || !identifier)return LOADER_ERR_INVALID;
    initialize();NSString *name=[NSString stringWithUTF8String:identifier];if(!name)return LOADER_ERR_INVALID;
    CapabilityProvider *provider=providers[name];if(!provider || provider.context!=context)return LOADER_ERR_INVALID;
    for(NSString *method in provider.capabilities[@"methods"])if(routes[method]==provider)[routes removeObjectForKey:method];
    [providers removeObjectForKey:name];generation++;
    NSArray *tokens=requests.allKeys;
    for(NSNumber *token in tokens){CapabilityRequest *request=requests[token];if(request.provider==provider){[requests removeObjectForKey:token];respond(errorValue(@"CAPABILITY_PROVIDER_REMOVED"),request.completion,request.context);}}
    return LOADER_OK;
}
int loader_register_editor_adapter(loader_editor_dispatch dispatch,void *context,const char *json) {
    return loader_register_capability_provider("editor.default",dispatch,context,json);
}
int loader_unregister_editor_adapter(loader_editor_dispatch dispatch,void *context) {
    if(!NSThread.isMainThread)return LOADER_ERR_INVALID;
    initialize();CapabilityProvider *provider=providers[@"editor.default"];
    if(!provider || provider.dispatch!=dispatch)return LOADER_ERR_INVALID;
    return loader_unregister_capability_provider("editor.default",context);
}
NSDictionary *loader_capability_snapshot(void) {
    if(!NSThread.isMainThread)return errorValue(@"MAIN_THREAD_REQUIRED");
    initialize();
    NSMutableArray *descriptors=[NSMutableArray array];NSMutableDictionary *caps=[NSMutableDictionary dictionary];
    for(NSString *name in [providers.allKeys sortedArrayUsingSelector:@selector(compare:)]){
        CapabilityProvider *provider=providers[name];NSMutableDictionary *item=[provider.capabilities mutableCopy];item[@"id"]=name;[descriptors addObject:item];
        if(providers.count==1)[caps addEntriesFromDictionary:provider.capabilities];
    }
    caps[@"methods"]=[routes.allKeys sortedArrayUsingSelector:@selector(compare:)];caps[@"providers"]=descriptors;
    return @{@"ok":@YES,@"hostPid":@(getpid()),@"editorReady":((providers[@"editor.default"] || routes[@"selection.read"])?@YES:@NO),@"capabilitiesReady":(routes.count?@YES:@NO),@"capabilities":caps,@"capabilityGeneration":@(generation),@"productAccountRequired":@NO};
}
static void providerComplete(const char *json,size_t length,void *opaque) {
    NSNumber *token=@((uintptr_t)opaque);
    NSData *data=(json && length<=1048576)?[NSData dataWithBytes:json length:length]:nil;
    dispatch_async(dispatch_get_main_queue(),^{
        CapabilityRequest *request=requests[token];if(!request)return;
        [requests removeObjectForKey:token];
        id value=data?[NSJSONSerialization JSONObjectWithData:data options:0 error:NULL]:nil;
        if(![value isKindOfClass:NSDictionary.class] || !value[@"ok"] || CFGetTypeID((__bridge CFTypeRef)value[@"ok"])!=CFBooleanGetTypeID())value=errorValue(@"INVALID_CAPABILITY_RESPONSE");
        respond(value,request.completion,request.context);
    });
}
void loader_editor_request(const char *method,const char *arguments,loader_json_completion completion,void *context) {
    if(!completion)return;
    if(!method || !arguments || strnlen(arguments,262145)>262144 || strnlen(method,97)>96){respond(errorValue(@"INVALID_ARGUMENT"),completion,context);return;}
    NSString *name=[NSString stringWithUTF8String:method];NSString *payload=[NSString stringWithUTF8String:arguments];
    if(!name || !payload){respond(errorValue(@"INVALID_ARGUMENT"),completion,context);return;}
    dispatch_async(dispatch_get_main_queue(),^{
        initialize();
        if([name isEqualToString:@"capabilities"]){respond(loader_capability_snapshot(),completion,context);return;}
        if(privateMethod(name)){respond(errorValue(@"METHOD_NOT_PUBLIC"),completion,context);return;}
        id values=[NSJSONSerialization JSONObjectWithData:[payload dataUsingEncoding:NSUTF8StringEncoding] options:0 error:NULL];
        if(![values isKindOfClass:NSDictionary.class]){respond(errorValue(@"INVALID_ARGUMENT"),completion,context);return;}
        CapabilityProvider *provider=routes[name];
        if(!provider){BOOL editorMethod=[name hasPrefix:@"notes."] || [name hasPrefix:@"pitch."] || [name hasPrefix:@"selection."];
            respond(errorValue((!routes.count || (editorMethod && !providers[@"editor.default"]))?@"EDITOR_ADAPTER_UNAVAILABLE":@"UNSUPPORTED_HOST_CAPABILITY"),completion,context);return;}
        if(requests.count>=128 || nextToken==UINT64_MAX){respond(errorValue(@"CAPABILITY_BUSY"),completion,context);return;}
        NSNumber *token=@(nextToken++);CapabilityRequest *request=[CapabilityRequest new];request.provider=provider;request.completion=completion;request.context=context;requests[token]=request;
        dispatch_after(dispatch_time(DISPATCH_TIME_NOW,6000000000),dispatch_get_main_queue(),^{CapabilityRequest *late=requests[token];if(late){[requests removeObjectForKey:token];respond(errorValue(@"CAPABILITY_TIMEOUT"),late.completion,late.context);}});
        provider.dispatch(name.UTF8String,payload.UTF8String,providerComplete,(void*)(uintptr_t)token.unsignedLongLongValue,provider.context);
    });
}
