// Lua calls use the same provider registry as MCP. Tickets are resident memory,
// bounded and expiring; polling them never touches files, pipes or sockets.
#import <Foundation/Foundation.h>
#include "loader_api.h"
extern void loader_editor_request(const char*,const char*,loader_json_completion,void*);
extern NSDictionary *loader_capability_snapshot(void);
@interface LuaCapabilityTicket : NSObject
@property NSDictionary *response;
@end
@implementation LuaCapabilityTicket
@end
static NSMutableDictionary<NSNumber*,LuaCapabilityTicket*> *tickets;
static uint64_t nextTicket=1;
static NSDictionary *error(NSString *code) {return @{@"ok":@NO,@"error":@{@"code":code}};}
static void completed(const char *json,size_t length,void *context) {
    // Loader guarantees delivery on the main thread for valid queued requests.
    LuaCapabilityTicket *ticket=tickets[@((uintptr_t)context)];if(!ticket)return;
    id response=(json && length<=1048576)?[NSJSONSerialization JSONObjectWithData:[NSData dataWithBytes:json length:length] options:0 error:NULL]:nil;
    ticket.response=[response isKindOfClass:NSDictionary.class]?response:error(@"INVALID_NATIVE_RESPONSE");
}
NSDictionary *loader_lua_capability_call(NSString *method,NSDictionary *arguments) {
    if(!NSThread.isMainThread)return error(@"MAIN_THREAD_REQUIRED");
    if(![arguments isKindOfClass:NSDictionary.class])return error(@"INVALID_ARGUMENT");
    if(!tickets)tickets=[NSMutableDictionary dictionary];
    if([method isEqualToString:@"call.submit"]) {
        NSString *name=arguments[@"method"];id payload=arguments[@"arguments"]?:@{};
        if(![name isKindOfClass:NSString.class] || name.length>96 || ![payload isKindOfClass:NSDictionary.class])return error(@"INVALID_ARGUMENT");
        if(tickets.count>=64 || nextTicket==UINT64_MAX)return error(@"CAPABILITY_BUSY");
        NSData *data=[NSJSONSerialization dataWithJSONObject:payload options:0 error:NULL];if(!data || data.length>262144)return error(@"REQUEST_TOO_LARGE");
        NSNumber *identifier=@(nextTicket++);tickets[identifier]=[LuaCapabilityTicket new];
        NSString *encoded=[[NSString alloc] initWithData:data encoding:NSUTF8StringEncoding];
        loader_editor_request(name.UTF8String,encoded.UTF8String,completed,(void*)(uintptr_t)identifier.unsignedLongLongValue);
        dispatch_after(dispatch_time(DISPATCH_TIME_NOW,15000000000),dispatch_get_main_queue(),^{[tickets removeObjectForKey:identifier];});
        return @{@"ok":@YES,@"result":@{@"ticket":identifier}};
    }
    if([method isEqualToString:@"call.result"]) {
        id identifier=arguments[@"ticket"];
        if(![identifier isKindOfClass:NSNumber.class] || CFGetTypeID((__bridge CFTypeRef)identifier)==CFBooleanGetTypeID() || [identifier doubleValue] != [identifier unsignedLongLongValue])return error(@"INVALID_ARGUMENT");
        LuaCapabilityTicket *ticket=tickets[identifier];if(!ticket)return error(@"CALL_TICKET_EXPIRED");
        if(!ticket.response)return @{@"ok":@YES,@"result":@{@"pending":@YES}};
        NSDictionary *response=ticket.response;[tickets removeObjectForKey:identifier];
        return @{@"ok":@YES,@"result":@{@"pending":@NO,@"response":response}};
    }
    return error(@"UNSUPPORTED_HOST_CAPABILITY");
}
