#pragma once
#include "editor_lua_service.h"

namespace editor_gate {
bool version_matches();
using Router=editor_lua::json (*)(const std::string&,const editor_lua::json&,bool);
int install(editor_lua::Service *service);
void set_router(Router router);
const char *call_json(const char *body,size_t length,bool javascript);
}
