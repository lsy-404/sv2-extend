#ifndef LOADER_API_H
#define LOADER_API_H
#include <stdint.h>
#include <stddef.h>
#define LOADER_CAP_HOST_URLSESSION_GATE (1ull << 0)
#define LOADER_CAP_PLUGIN_HTTP (1ull << 1)
#define LOADER_CAP_HOST_DATA_NAMESPACE (1ull << 3)
#define LOADER_CAP_PLUGIN_UI (1ull << 4)
#define LOADER_CAP_STARTUP_NETWORK_GUARD (1ull << 5)
#define LOADER_CAP_PROFILE_LEASE (1ull << 6)
#define LOADER_CAP_WEB_RENDERER_RETIREMENT (1ull << 7)
#define LOADER_CAP_WEB_CONTEXT_RETIREMENT (1ull << 8)
#define LOADER_CAP_SCOPED_STORAGE (1ull << 9)
#define LOADER_CAP_NETWORK_POLICY_SWITCH (1ull << 10)
/* Body is borrowed for this callback only; callback may run off the main thread. */
typedef void (*loader_http_completion)(int32_t status, int32_t error, const uint8_t *body, size_t length, void *context);
/* UI events are synchronous/main-thread, borrowed JSON. Never route account actions through MCP. */
typedef void (*loader_plugin_ui_event)(const char *event_json,void *owner_context);
typedef void (*loader_json_completion)(const char *json,size_t length,void *context);
typedef void (*loader_editor_dispatch)(const char *method,const char *arguments,loader_json_completion complete,void *completion_context,void *adapter_context);
typedef void (*loader_http_json_handler)(const char *body,size_t length,loader_json_completion complete,void *completion_context,void *handler_context);
typedef struct {uint32_t struct_size;uint16_t port;char token[128];} loader_http_endpoint_v1;
#define LOADER_ABI_V1 1u
#define LOADER_CAP_NETWORK_STATE (1ull << 2)
#define LOADER_NET_URLSESSION_RESUME (1ull << 0)
#define LOADER_NET_RAW_SOCKET (1ull << 1)
#define LOADER_NET_WEBKIT (1ull << 2)
#define LOADER_NET_NETWORK_FRAMEWORK (1ull << 3)
#define LOADER_NET_BACKGROUND_SESSION (1ull << 4)
/* Explicit remaining coverage gaps; do not interpret tracked drain as closure. */
#define LOADER_NET_SWIFT_NETWORK (1ull << 5)
#define LOADER_NET_GROUP_MULTIPLEX (1ull << 6)
#define LOADER_NET_WINHTTP (1ull << 7)
#define LOADER_NET_WININET (1ull << 8)
#define LOADER_NET_WINSOCK (1ull << 9)
#define LOADER_NET_WEBVIEW2 (1ull << 10)
#define LOADER_NET_WEBRTC (1ull << 11)
#define LOADER_OK 0
#define LOADER_ERR_INVALID -1
#define LOADER_ERR_NETWORK_UNVERIFIED -2
#define LOADER_ERR_STALE_GENERATION -3
#define LOADER_ERR_STORAGE_NOT_CONFIGURED -4
#define LOADER_ERR_PROFILE_BUSY -5
#define LOADER_ERR_POLICY_INVALID -6
#define LOADER_ERR_NOT_SUPPORTED -7
#define LOADER_ERR_NOT_FOUND -8
#define LOADER_ERR_BUFFER_TOO_SMALL -9
#define LOADER_ERR_STORAGE_IO -10
typedef struct {
    uint32_t abi_version;
    uint32_t struct_size;
    uint64_t generation;
    uint64_t intercepted_paths;
    uint64_t unverified_paths;
    uint32_t requested_blocked;
    uint32_t tracked_host_tasks_active;
    uint32_t tracked_tasks_drained;
    uint32_t whole_host_verified;
} loader_network_state_v1;
typedef struct {
    uint32_t abi_version,struct_size;
    uint64_t generation;
    uint32_t requested_blocked,applied_blocked,pending;
    int32_t error;
} loader_network_policy_state_v1;
/* Persistent guards control the next startup independently of this process's
   temporary network policy. Opaque operation IDs carry no account/session data. */
typedef struct {uint32_t abi_version,struct_size;uint64_t generation;uint32_t requested_blocked,active_guards,policy_valid,bootstrap_applied;} loader_startup_network_state_v1;
#define LOADER_PROFILE_LEASE_NONE 0u
#define LOADER_PROFILE_LEASE_SHARED 1u
#define LOADER_PROFILE_LEASE_EXCLUSIVE 2u
typedef struct {uint32_t abi_version,struct_size;int32_t host_pid;uint32_t mode;uint64_t generation;} loader_profile_lease_state_v1;
typedef struct {
    uint32_t abi_version;
    uint32_t struct_size;
    uint64_t capabilities;
    int32_t host_pid;
    uint32_t on_main_thread;
    const char *host_executable; /* borrowed; valid for plugin lifetime */
    void (*log)(const char *message);
    int (*set_host_network_blocked)(int blocked);
    int (*host_network_blocked)(void); /* Requested policy only; never a verification receipt. */
    int (*plugin_http_get)(const char *url, loader_http_completion completion, void *context);
    int (*get_host_network_state)(loader_network_state_v1 *state);
    /* Must succeed before a consumer treats host network as quiescent.
       Current experimental backend always returns NETWORK_UNVERIFIED. */
    int (*verify_host_network_quiescence)(uint64_t expected_generation, loader_network_state_v1 *state);
    /* Legacy prototype storage in network-probe/data; preserved for old plugins.
       New plugins use scoped storage below. */
    int (*plugin_storage_write)(const char *key, const uint8_t *data, size_t length);
    /* Editor adapter starts on the main thread; it must complete once, and may use resident asynchronous transport. */
    int (*register_editor_adapter)(loader_editor_dispatch dispatch,void *adapter_context,const char *capabilities_json);
    void (*editor_request_json)(const char *method,const char *arguments,loader_json_completion complete,void *context);
    /* Main-thread lifecycle; request handlers run on a background queue. */
    int (*listen_local_json)(const char *name,loader_http_json_handler handler,void *handler_context,loader_http_endpoint_v1 *endpoint);
    /* Owner must stop/complete outstanding calls before unregistering. Main thread only. */
    int (*unregister_editor_adapter)(loader_editor_dispatch dispatch,void *adapter_context);
    /* Public engineering capabilities. Main-thread registration. Methods are
       unique across providers; account, session, license and private domains are reserved.
       Requests complete at most once, with removal and six-second timeout errors.
       Late replies are ignored. Contexts remain provider-owned and must survive
       provider work; unregister does not itself stop provider background work. */
    int (*register_capability_provider)(const char *provider_id,loader_editor_dispatch dispatch,void *provider_context,const char *capabilities_json);
    int (*unregister_capability_provider)(const char *provider_id,void *provider_context);
    /* Preferred generic name for editor_request_json; same resident registry. */
    void (*request_capability_json)(const char *method,const char *arguments,loader_json_completion complete,void *context);
    /* Append-only: borrowed native application data directory, NULL in legacy mode.
       Generic filesystem namespace only; consumers own all account business. */
    const char *(*host_data_directory)(void);
    /* Append-only plugin UI registry. Descriptor schema1: id/name/version/status,
       sidebar{title,rows[label|button]}, settings[boolean|choice|integer|text].
       Each registered id owns an independent right-toolbar panel. Extend is
       a separate management category below host Settings, not a Scripts section.
       Hide controls change presentation only, never plugin execution.
       Unregister before freeing owner_context; no callbacks occur after removal. */
    int (*register_plugin_ui)(const char *descriptor_json,loader_plugin_ui_event event,void *owner_context);
    int (*unregister_plugin_ui)(const char *plugin_id,void *owner_context);
    int (*update_plugin_ui)(const char *plugin_id,const char *state_json,void *owner_context);
    /* Append-only, main-thread: stop only the matching resident listener. Accepted work may
       finish; new connections/requests are rejected and discovery is removed. */
    int (*stop_local_json)(const char *name,loader_http_json_handler handler,void *context);
    /* Main-thread, append-only generic recovery safeguards. Each owner+guard
       is independently persisted and removed; invalid policy fails closed. */
    int (*set_startup_network_guard)(const char *owner_id,const char *guard_id,int enabled);
    int (*get_startup_network_policy)(loader_startup_network_state_v1 *state);
    void (*list_startup_network_guards)(const char *owner_id,loader_json_completion complete,void *context);
    /* Core holds a shared kernel lease before host main. Upgrade is atomic;
       conflicting hosts/operations are refused, not discovered by ps polling. */
    int (*acquire_profile_exclusive)(const char *owner_id,const char *operation_id);
    int (*release_profile_exclusive)(const char *owner_id,const char *operation_id);
    int (*get_profile_lease_state)(loader_profile_lease_state_v1 *state);
    /* Explicit main-thread action, not an ordinary offline toggle: terminates
       this host's WebKit renderer contexts, losing their live document state.
       Requires requested-blocked policy at expected_generation and an exclusive
       profile lease held until completion. Does not delete persistent web data.
       Accepted (OK) calls complete exactly once on main; immediate errors do not
       call complete. A successful renderer receipt is NOT network quiescence.
       Callback execution itself stays counted until it returns. */
    int (*retire_host_web_renderers)(uint64_t expected_generation,loader_json_completion complete,void *context);
    /* Separate destructive action: renderers AND observed WebKit network
       contexts. Same main-thread/generation/exclusive-lease/completion contract
       as renderer retirement. Refuses unknown/unobserved processes. Persistent
       website data remains. Success is not whole-host network quiescence. */
    int (*retire_host_web_contexts)(uint64_t expected_generation,loader_json_completion complete,void *context);
    /* Synchronous, <=1 MiB, plugin-data/<plugin_id>/<key>. Native plugins are
       trusted; IDs provide namespaces, not a native-code sandbox. Writes use
       a private temp + fsync + rename + directory fsync. An I/O error after
       rename can leave a complete committed value; callers must reconcile.
       Read: length is capacity/result size; NULL data queries required size.
       BUFFER_TOO_SMALL updates length without modifying the caller buffer. */
    int (*plugin_storage_write_scoped)(const char *plugin_id,const char *key,const uint8_t *data,size_t length);
    int (*plugin_storage_read_scoped)(const char *plugin_id,const char *key,uint8_t *data,size_t *length);
    /* Applied policy is separate from tracked-task drainage and whole-host isolation. */
    int (*get_host_network_policy)(loader_network_policy_state_v1 *state);
} loader_api_v1;
/* Return zero on success. Loader owns dlopen handle until host shutdown. */
typedef int (*loader_plugin_init_v1_fn)(const loader_api_v1 *api);
#endif
