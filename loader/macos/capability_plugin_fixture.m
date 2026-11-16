// Test-only plugin. Never packaged with Extend Loader.
#import <Foundation/Foundation.h>
#include "loader_api.h"
static const loader_api_v1 *loader;
static void invoke(const char *method,const char *args,loader_json_completion done,void *context,void *owner) {
    (void)owner;
    if(!strcmp(method,"fixture.network_block") || !strcmp(method,"fixture.network_allow")){
        int blocked=!strcmp(method,"fixture.network_block");int changed=loader->set_host_network_blocked(blocked);
        NSDictionary *reply=@{@"ok":(changed==0?@YES:@NO),@"result":@{@"blocked":@(loader->host_network_blocked())}};
        NSData *encoded=[NSJSONSerialization dataWithJSONObject:reply options:0 error:NULL];done(encoded.bytes,encoded.length,context);return;
    }
    if(!strcmp(method,"fixture.http_get")){loader->request_capability_json("http.get",args,done,context);return;}
    id value=[NSJSONSerialization JSONObjectWithData:[[NSString stringWithUTF8String:args] dataUsingEncoding:NSUTF8StringEncoding] options:0 error:NULL];
    NSDictionary *result=@{@"ok":@YES,@"result":@{@"echo":value?:@{},@"hostPid":@(loader->host_pid),@"mainThread":(NSThread.isMainThread?@YES:@NO)}};
    NSData *data=[NSJSONSerialization dataWithJSONObject:result options:0 error:NULL];done(data.bytes,data.length,context);
    if(!strcmp(method,"fixture.remove"))dispatch_after(dispatch_time(DISPATCH_TIME_NOW,100000000),dispatch_get_main_queue(),^{loader->unregister_capability_provider("fixture.echo",NULL);});
}
int loader_plugin_init_v1(const loader_api_v1 *api) {
    if(!api || api->abi_version!=LOADER_ABI_V1 || api->struct_size<offsetof(loader_api_v1,request_capability_json)+sizeof api->request_capability_json)return LOADER_ERR_INVALID;
    loader=api;
    const char *descriptor="{\"methods\":[\"fixture.echo\",\"fixture.remove\",\"fixture.http_get\",\"fixture.network_block\",\"fixture.network_allow\"],\"tools\":[{\"name\":\"svext_fixture_http_get\",\"method\":\"fixture.http_get\",\"description\":\"Test-only call to the real shared HTTP capability\",\"inputSchema\":{\"type\":\"object\"}},{\"name\":\"svext_fixture_echo\",\"method\":\"fixture.echo\",\"description\":\"Test fixture echo; not a product capability\",\"inputSchema\":{\"type\":\"object\"}},{\"name\":\"svext_fixture_remove\",\"method\":\"fixture.remove\",\"description\":\"Remove only this test provider\",\"inputSchema\":{\"type\":\"object\"}}]}";
    return api->register_capability_provider("fixture.echo",invoke,NULL,descriptor);
}
