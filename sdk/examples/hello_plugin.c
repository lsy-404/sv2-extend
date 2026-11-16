#include "loader_api.h"
#include <stddef.h>
#ifdef _WIN32
#define PLUGIN_EXPORT __declspec(dllexport)
#else
#define PLUGIN_EXPORT __attribute__((visibility("default")))
#endif
static const loader_api_v1 *loader;
static void dispatch(const char *method,const char *arguments,loader_json_completion done,void *context,void *owner){(void)method;(void)arguments;(void)owner;const char value[]="{\"ok\":true,\"result\":{\"message\":\"Hello from the resident plugin\"}}";done(value,sizeof value-1,context);}
PLUGIN_EXPORT int loader_plugin_init_v1(const loader_api_v1 *api){if(!api||api->abi_version!=LOADER_ABI_V1||api->struct_size<offsetof(loader_api_v1,register_capability_provider)+sizeof api->register_capability_provider||api->host_pid<=0||!api->on_main_thread)return LOADER_ERR_INVALID;loader=api;loader->log("hello-plugin-resident-initialized");return loader->register_capability_provider("example.hello",dispatch,0,"{\"methods\":[\"hello.greet\"],\"tools\":[{\"name\":\"svext_hello_greet\",\"method\":\"hello.greet\",\"description\":\"Read a greeting from the in-process plugin.\",\"inputSchema\":{\"type\":\"object\",\"properties\":{},\"additionalProperties\":false}}]}");}
