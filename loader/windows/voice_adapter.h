#pragma once
#include <windows.h>
namespace voice_adapter {
using Observer=void(*)(void *reference,void *context);
int install(DWORD main_thread);
bool begin(Observer observer,void *context);
bool finish();
bool is_observing(void *reference);
}
