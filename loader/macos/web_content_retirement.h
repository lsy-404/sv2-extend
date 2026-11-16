#include "loader_api.h"
int loader_web_retirement_supported(void);
int loader_retire_host_web_renderers(uint64_t expected,loader_json_completion complete,void *context);
uint32_t loader_web_retirement_pending(void);

int loader_web_context_retirement_supported(void);
int loader_retire_host_web_contexts(uint64_t expected,loader_json_completion complete,void *context);
