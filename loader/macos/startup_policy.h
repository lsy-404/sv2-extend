#include "loader_api.h"
int loader_profile_lease_bootstrap(void);
int loader_startup_policy_should_block(void);
void loader_startup_policy_mark_applied(void);
int loader_set_startup_network_guard(const char*,const char*,int);
int loader_get_startup_network_policy(loader_startup_network_state_v1*);
void loader_list_startup_network_guards(const char*,loader_json_completion,void*);
/* Bootstrap-safe lookup: no native host getter or callback dispatch. Negative
   errors must never be interpreted as an owner having no recovery guards. */
int32_t loader_startup_owner_guard_count(const char*);
int loader_acquire_profile_exclusive(const char*,const char*);
int loader_release_profile_exclusive(const char*,const char*);
int loader_get_profile_lease_state(loader_profile_lease_state_v1*);
