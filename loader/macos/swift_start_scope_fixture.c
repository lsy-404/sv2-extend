#include <stdint.h>
static _Thread_local int32_t current_scope;
int32_t socket_gate_current_scope(void){return current_scope;}
void extend_test_scope(int32_t value){current_scope=value;}
