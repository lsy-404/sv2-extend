#include "loader_api.h"
#include <stddef.h>
#include <pthread.h>
#include <unistd.h>
__attribute__((visibility("default")))
int loader_plugin_init_v1(const loader_api_v1 *api) {
    if (!api || api->abi_version != LOADER_ABI_V1 || api->struct_size < sizeof *api ||
        api->host_pid != getpid() || !api->on_main_thread || !pthread_main_np()) return 1;
    api->log("sample-plugin-api-ok");
    return 0;
}
