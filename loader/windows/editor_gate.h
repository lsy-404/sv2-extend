#pragma once
#include "editor_lua_service.h"

namespace editor_gate {
bool version_matches();
int install(editor_lua::Service *service);
}
