#include "loader_api.h"
#include <unistd.h>
extern void swift_probe_start(void);
int loader_plugin_init_v1(const loader_api_v1 *api){if(!api||api->abi_version!=LOADER_ABI_V1||api->host_pid!=getpid())return 1;swift_probe_start();return 0;}
