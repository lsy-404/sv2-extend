// Source-only diagnostic, never shipped with the Loader.
#import <Foundation/Foundation.h>
#include "../../sdk/include/loader_bootstrap_api.h"
#include <stdio.h>
#include <stdlib.h>
static unsigned constructions,bootCalls,normalCalls;
static const loader_bootstrap_api_v1 *borrowed;
__attribute__((constructor))static void constructed(void){constructions++;}
static void emit(const char *phase){
 printf("BOOTSTRAP_TEST {\"phase\":\"%s\",\"constructions\":%u,\"bootCalls\":%u,\"normalCalls\":%u,\"metadataRetained\":%s}\n",phase,constructions,bootCalls,normalCalls,borrowed&&borrowed->host_executable&&borrowed->loader_data_directory?"true":"false");fflush(stdout);
}
int loader_plugin_bootstrap_v1(const loader_bootstrap_api_v1 *api){
 borrowed=api;bootCalls++;
 if(!api||api->abi_version!=1||api->struct_size<sizeof *api||!(api->flags&LOADER_BOOTSTRAP_BEFORE_EXECUTABLE_INITIALIZERS)||!(api->flags&LOADER_BOOTSTRAP_MAIN_THREAD)||api->host_pid!=getpid()||!api->owner_guard_count)return -1;
 printf("BOOTSTRAP_TEST {\"phase\":\"early\",\"ownerCount\":%d,\"blocked\":%u,\"policyValid\":%u,\"applied\":%u}\n",api->owner_guard_count("test.early"),api->startup_policy.requested_blocked,api->startup_policy.policy_valid,api->startup_policy.bootstrap_applied);fflush(stdout);
 return getenv("EXTEND_TEST_BOOTSTRAP_FAIL")?-1:0;
}
int loader_plugin_init_v1(const loader_api_v1 *api){(void)api;normalCalls++;emit("normal");return 0;}
void extend_test_executable_constructor(void){emit("executable-constructor");}
