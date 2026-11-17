#include "loader_api.h"
#include "../../sdk/include/loader_bootstrap_api.h"
#include "startup_policy.h"
#include "web_content_retirement.h"
#include <dispatch/dispatch.h>
#include <dlfcn.h>
#include <libproc.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
extern int network_gate_install(void);
extern int loader_plugin_ui_install(void);
extern int loader_host_navigation_install(void);
extern int loader_register_host_navigation_handler(const char*,loader_host_navigation_handler,void*);
extern int loader_unregister_host_navigation_handler(const char*,loader_host_navigation_handler,void*);
extern void loader_load_plugin_packages(const loader_api_v1*);
extern int loader_bootstrap_plugin_packages(const loader_bootstrap_api_v1*);
extern int loader_register_plugin_ui(const char*,loader_plugin_ui_event,void*);
extern int loader_unregister_plugin_ui(const char*,void*);
extern int loader_update_plugin_ui(const char*,const char*,void*);
extern void loader_plugin_ui_register_runtimes(int,int);
extern int loader_host_data_install_from_environment(void);
extern const char *loader_host_data_directory(void);
extern void loader_apply_pending_plugin_updates(void);
extern int loader_lua_call_gate_install(void);
extern int loader_javascript_gate_install(void);
extern int loader_voice_host_install(void);
extern int loader_http_capability_install(void);
extern int loader_export_host_install(void);
extern void loader_lua_editor_autostart(void);
extern int network_gate_set(int blocked);
extern int network_gate_get(void);
extern int network_gate_plugin_get(const char *url, loader_http_completion completion, void *context);
extern int network_gate_state(loader_network_state_v1 *state);
extern int network_gate_policy(loader_network_policy_state_v1 *state);
extern int network_gate_verify_quiescence(uint64_t expected,loader_network_state_v1 *state);
extern int loader_plugin_storage_write_scoped(const char*,const char*,const uint8_t*,size_t);
extern int loader_plugin_storage_read_scoped(const char*,const char*,uint8_t*,size_t*);
extern int loader_plugin_storage_write(const char *key, const uint8_t *data, size_t length);
#ifdef LOADER_EXPERIMENTAL_EDITOR_PIPE
extern int native_editor_pipe_install(void);
#endif
extern int loader_register_capability_provider(const char*,loader_editor_dispatch,void*,const char*);
extern int loader_unregister_capability_provider(const char*,void*);
extern int loader_register_editor_adapter(loader_editor_dispatch dispatch,void *context,const char *json);
extern int loader_unregister_editor_adapter(loader_editor_dispatch,void*);
extern void loader_editor_request(const char *method,const char *arguments,loader_json_completion complete,void *context);
extern int loader_listen_local_json(const char *name,loader_http_json_handler handler,void *context,loader_http_endpoint_v1 *endpoint);
int loader_stop_local_json(const char*,loader_http_json_handler,void*);
static char executable[PROC_PIDPATHINFO_MAXSIZE];
static loader_api_v1 api;
static loader_bootstrap_api_v1 bootstrapApi;
static void *plugin;
static void log_message(const char *message) {
    fprintf(stderr, "LOADER_PROBE pid=%d main=%d %s\n", getpid(), pthread_main_np(), message);
    fflush(stderr);
}
static void initialize(void *context) {
    (void)context;
    proc_pidpath(getpid(), executable, sizeof executable);
    if (network_gate_install() != 0) { log_message("network-gate-install-failed"); return; }
    if(getenv("LOADER_PROBE_BLOCK_HOST_AT_START") && !strcmp(getenv("LOADER_PROBE_BLOCK_HOST_AT_START"),"1"))network_gate_set(1);
    api = (loader_api_v1){ .abi_version=LOADER_ABI_V1, .struct_size=sizeof api, .capabilities=LOADER_CAP_HOST_URLSESSION_GATE|LOADER_CAP_PLUGIN_HTTP|LOADER_CAP_NETWORK_STATE, .host_pid=getpid(), .on_main_thread=pthread_main_np(), .host_executable=executable, .log=log_message, .set_host_network_blocked=network_gate_set, .host_network_blocked=network_gate_get, .plugin_http_get=network_gate_plugin_get, .get_host_network_state=network_gate_state, .verify_host_network_quiescence=network_gate_verify_quiescence, .plugin_storage_write=loader_plugin_storage_write, .register_editor_adapter=loader_register_editor_adapter, .editor_request_json=loader_editor_request, .listen_local_json=loader_listen_local_json, .unregister_editor_adapter=loader_unregister_editor_adapter, .register_capability_provider=loader_register_capability_provider, .unregister_capability_provider=loader_unregister_capability_provider, .request_capability_json=loader_editor_request, .host_data_directory=loader_host_data_directory, .register_plugin_ui=loader_register_plugin_ui, .unregister_plugin_ui=loader_unregister_plugin_ui, .update_plugin_ui=loader_update_plugin_ui, .stop_local_json=loader_stop_local_json, .set_startup_network_guard=loader_set_startup_network_guard, .get_startup_network_policy=loader_get_startup_network_policy, .list_startup_network_guards=loader_list_startup_network_guards, .acquire_profile_exclusive=loader_acquire_profile_exclusive, .release_profile_exclusive=loader_release_profile_exclusive, .get_profile_lease_state=loader_get_profile_lease_state, .retire_host_web_renderers=loader_retire_host_web_renderers, .retire_host_web_contexts=loader_retire_host_web_contexts, .plugin_storage_write_scoped=loader_plugin_storage_write_scoped, .plugin_storage_read_scoped=loader_plugin_storage_read_scoped };
    api.capabilities|=LOADER_CAP_SCOPED_STORAGE|LOADER_CAP_NETWORK_POLICY_SWITCH;api.get_host_network_policy=network_gate_policy;
    if(loader_host_navigation_install()==LOADER_OK){api.capabilities|=LOADER_CAP_HOST_NAVIGATION;api.register_host_navigation_handler=loader_register_host_navigation_handler;api.unregister_host_navigation_handler=loader_unregister_host_navigation_handler;}
    if(loader_host_data_directory())api.capabilities|=LOADER_CAP_HOST_DATA_NAMESPACE;
    api.capabilities|=LOADER_CAP_STARTUP_NETWORK_GUARD;loader_profile_lease_state_v1 lease={.abi_version=LOADER_ABI_V1,.struct_size=sizeof lease};if(loader_get_profile_lease_state(&lease)==LOADER_OK)api.capabilities|=LOADER_CAP_PROFILE_LEASE;
    if(loader_web_retirement_supported())api.capabilities|=LOADER_CAP_WEB_RENDERER_RETIREMENT;
    if(loader_web_context_retirement_supported())api.capabilities|=LOADER_CAP_WEB_CONTEXT_RETIREMENT;
    if(loader_plugin_ui_install()==0)api.capabilities|=LOADER_CAP_PLUGIN_UI;
    log_message(executable);
    log_message(loader_http_capability_install()==0?"http-capability-ready":"http-capability-failed");
    int luaReady=loader_lua_call_gate_install()==0;log_message(luaReady?"lua-native-call-ready":"lua-native-call-failed");
    int jsReady=0;
#ifdef LOADER_EXPERIMENTAL_EDITOR_PIPE
    if(getenv("OPEN_SV2_ENABLE_EDITOR_PIPE"))log_message(native_editor_pipe_install()==0?"editor-pipe-installed":"editor-pipe-install-failed");
#endif
    if(getenv("OPEN_SV2_ENABLE_JS_NATIVE")){jsReady=loader_javascript_gate_install()==0;log_message(jsReady?"js-native-call-ready":"js-native-call-unavailable");}
    loader_plugin_ui_register_runtimes(luaReady,jsReady);
    log_message(loader_voice_host_install()==0?"voice-native-identity-ready":"voice-native-identity-unavailable");
    log_message(loader_export_host_install()==0?"export-native-ready":"export-native-unavailable");
    const char *mcpPath=getenv("OPEN_SV2_MCP_PLUGIN");
    if(mcpPath) {
        void *module=dlopen(mcpPath,RTLD_NOW|RTLD_LOCAL);
        if(module){loader_plugin_init_v1_fn entry=(loader_plugin_init_v1_fn)dlsym(module,"loader_plugin_init_v1");log_message(entry && entry(&api)==0?"sv2-mcp-plugin-ready":"sv2-mcp-plugin-failed");}
        else log_message("sv2-mcp-module-load-failed");
    }
    loader_load_plugin_packages(&api);
    loader_lua_editor_autostart();
    const char *path = getenv("LOADER_PROBE_PLUGIN");
    if (!path) { log_message("plugin-path-absent"); return; }
    plugin = dlopen(path, RTLD_NOW | RTLD_LOCAL);
    if (!plugin) { log_message(dlerror()); return; }
    loader_plugin_init_v1_fn init = (loader_plugin_init_v1_fn)dlsym(plugin, "loader_plugin_init_v1");
    if (!init) { log_message(dlerror()); return; }
    log_message(init(&api) == 0 ? "plugin-init-ok" : "plugin-init-failed");
}
__attribute__((constructor)) static void load_probe(void) {
    int lease=loader_profile_lease_bootstrap();if(lease==LOADER_ERR_PROFILE_BUSY){fputs("PROFILE_LEASE startup-refused=busy\n",stderr);_exit(73);}if(lease!=LOADER_OK)fprintf(stderr,"PROFILE_LEASE unavailable=%d\n",lease);

    if(loader_host_data_install_from_environment()!=0){fputs("HOST_DATA_NAMESPACE required-install-failed=1\n",stderr);_exit(78);}
    // Observe online startup too. Installing only after dispatching initialize
    // misses tasks and completion blocks created by executable constructors.
    if(network_gate_install()!=0){fputs("STARTUP_NETWORK_GATE install-failed=1\n",stderr);_exit(78);}
    if(loader_startup_policy_should_block() || (getenv("LOADER_PROBE_BLOCK_HOST_AT_START")&&!strcmp(getenv("LOADER_PROBE_BLOCK_HOST_AT_START"),"1"))){if(network_gate_set(1)!=0){fputs("STARTUP_NETWORK_GUARD apply-failed=1\n",stderr);_exit(78);}loader_startup_policy_mark_applied();fputs("STARTUP_NETWORK_GUARD applied-before-main=1\n",stderr);}
    proc_pidpath(getpid(),executable,sizeof executable);
    const char *data=getenv("OPEN_SV2_LOADER_DATA_DIR");
    bootstrapApi=(loader_bootstrap_api_v1){.abi_version=LOADER_BOOTSTRAP_ABI_V1,.struct_size=sizeof bootstrapApi,.host_pid=getpid(),.flags=LOADER_BOOTSTRAP_BEFORE_EXECUTABLE_INITIALIZERS,.host_executable=executable,.loader_data_directory=data?strdup(data):NULL,.host_data_directory=loader_host_data_directory(),.startup_policy={.abi_version=LOADER_ABI_V1,.struct_size=sizeof(loader_startup_network_state_v1)},.owner_guard_count=loader_startup_owner_guard_count,.log=log_message};
    if(pthread_main_np())bootstrapApi.flags|=LOADER_BOOTSTRAP_MAIN_THREAD;
    if(bootstrapApi.host_data_directory)bootstrapApi.flags|=LOADER_BOOTSTRAP_DATA_NAMESPACE;
    const char *basename=strrchr(executable,'/');
    if(basename&&!strcmp(basename+1,"synthv-studio-original"))bootstrapApi.flags|=LOADER_BOOTSTRAP_MANAGED_HOST;
    if(loader_get_startup_network_policy(&bootstrapApi.startup_policy)!=LOADER_OK&&data){fputs("PLUGIN_BOOTSTRAP policy-unavailable=1\n",stderr);_exit(78);}
    // Package transactions must finish before their code can be retained by an
    // early entry. Normal initialization never swaps an already mapped module.
    loader_apply_pending_plugin_updates();
    if(loader_bootstrap_plugin_packages(&bootstrapApi)!=LOADER_OK){fputs("PLUGIN_BOOTSTRAP host-initialization-refused=1\n",stderr);_exit(78);}
    log_message("constructor-entered");
    dispatch_async_f(dispatch_get_main_queue(), NULL, initialize);
}
