#pragma once
#include <cstddef>
namespace javascript_gate {
using Router = const char *(*)(const char *,size_t,bool);
int install(Router router,const char *module);
bool ready();
}
