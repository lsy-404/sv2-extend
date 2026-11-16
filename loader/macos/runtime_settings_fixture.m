// Diagnostic package only: expose the same resident script boundary to tests.
#import <Foundation/Foundation.h>
#include "loader_api.h"
#include <dlfcn.h>
#include <unistd.h>
static void handle(const char *body,size_t length,loader_json_completion complete,void *context,void *owner){
 (void)owner;NSData *data=[NSData dataWithBytes:body length:length];
 dispatch_async(dispatch_get_main_queue(),^{
  id request=[NSJSONSerialization JSONObjectWithData:data options:0 error:NULL];
  const char *(*call)(const char*,size_t,int)=dlsym(RTLD_DEFAULT,"loader_script_call_json_for_runtime");
  id value=[request isKindOfClass:NSDictionary.class]?request:@{};NSData *json=[NSJSONSerialization dataWithJSONObject:value options:0 error:NULL];
  const char *response=call?call(json.bytes,json.length,[value[@"javascript"] boolValue]):NULL;
  complete(response,response?strlen(response):0,context);
 });
}
int loader_plugin_init_v1(const loader_api_v1 *api){if(!api||api->abi_version!=LOADER_ABI_V1||api->host_pid!=getpid())return LOADER_ERR_INVALID;loader_http_endpoint_v1 endpoint={.struct_size=sizeof endpoint};return api->listen_local_json("settings-probe",handle,NULL,&endpoint);}
