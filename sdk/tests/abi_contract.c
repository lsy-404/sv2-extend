#include "loader_api.h"
#include "extend_platform.h"
#include "loader_bootstrap_api.h"
#include <stddef.h>
_Static_assert(sizeof(void*)==8,"Only 64-bit hosts are supported");
_Static_assert(sizeof(loader_network_state_v1)==48,"network ABI drift");
_Static_assert(offsetof(loader_api_v1,host_pid)==16,"pid ABI drift");
_Static_assert(offsetof(loader_api_v1,host_executable)==24,"pointer ABI drift");
_Static_assert(offsetof(loader_api_v1,editor_request_json)==96,"editor ABI drift");
_Static_assert(offsetof(loader_api_v1,register_plugin_ui)==152,"UI ABI drift");
_Static_assert(offsetof(loader_api_v1,stop_local_json)==176,"listener ABI drift");
_Static_assert(offsetof(loader_api_v1,retire_host_web_renderers)==232,"renderer retirement ABI drift");
_Static_assert(offsetof(loader_api_v1,retire_host_web_contexts)==240,"web context ABI drift");
_Static_assert(offsetof(loader_api_v1,plugin_storage_write_scoped)==248,"scoped write ABI drift");
_Static_assert(offsetof(loader_api_v1,plugin_storage_read_scoped)==256,"scoped read ABI drift");
_Static_assert(offsetof(loader_api_v1,get_host_network_policy)==264,"network policy ABI drift");
_Static_assert(sizeof(loader_network_policy_state_v1)==32,"network policy state ABI drift");
_Static_assert(sizeof(loader_api_v1)==272,"append-only API drift: update compatibility test intentionally");
const char *extend_abi_platform(void){return loader_platform_key();}

_Static_assert(sizeof(loader_startup_network_state_v1)==32,"startup policy ABI drift");
_Static_assert(sizeof(loader_profile_lease_state_v1)==24,"profile lease ABI drift");
_Static_assert(sizeof(loader_bootstrap_api_v1)==88,"bootstrap metadata ABI drift");
