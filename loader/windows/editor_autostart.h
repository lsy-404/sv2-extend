#pragma once
#include "editor_lua_service.h"
namespace editor_autostart {
struct Status {bool installed,scan_seen,run_requested;const char *initial_scan_reason;};
Status status();
bool root_ready();
int install(editor_lua::Service *service,const wchar_t *script_path);
bool request_initial_scan();
}
