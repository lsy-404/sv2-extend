#ifndef SV2_EXTEND_BOOTSTRAP_API_H
#define SV2_EXTEND_BOOTSTRAP_API_H
#include "loader_api.h"
#define LOADER_BOOTSTRAP_ABI_V1 1u
#define LOADER_BOOTSTRAP_MAIN_THREAD (1u << 0)
#define LOADER_BOOTSTRAP_MANAGED_HOST (1u << 1)
#define LOADER_BOOTSTRAP_DATA_NAMESPACE (1u << 2)
/* Executable native initializer-list boundary, not a claim about Objective-C
   +load or every framework constructor. Consumers must prove their own target
   lifecycle starts after observation has been installed. */
#define LOADER_BOOTSTRAP_BEFORE_EXECUTABLE_INITIALIZERS (1u << 3)
/* Opt-in manifest: bootstrap:{abi:1,guardOwners:[...]}. macOS Core 0.9.5+
   dispatches this entry before executable initializer lists. Loader 0.9.4 and
   earlier, and the Windows development backend, do NOT dispatch it. Check the
   actual entry execution; header/symbol presence alone is not evidence. */
typedef struct {
    uint32_t abi_version,struct_size;
    int32_t host_pid;
    uint32_t flags;
    const char *host_executable;
    const char *loader_data_directory;
    const char *host_data_directory;
    loader_startup_network_state_v1 startup_policy;
    /* Pure Core policy lookup; nonnegative count, negative Loader error.
       No native account getter, journal inspection or network side effects. */
    int32_t (*owner_guard_count)(const char *owner_id);
    void (*log)(const char *message);
} loader_bootstrap_api_v1;
/* Only version-guarded observation/holds. No host getter/reader/auth, UI, editor,
   MCP, session mutation or async account work. Borrowed metadata lasts until host
   exit. Loader must retain the module after any attempted entry, including a
   failed return, because partial hooks may already refer to its code. */
typedef int (*loader_plugin_bootstrap_v1_fn)(const loader_bootstrap_api_v1 *api);
#endif
