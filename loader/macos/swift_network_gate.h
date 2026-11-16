#ifndef EXTEND_SWIFT_NETWORK_GATE_H
#define EXTEND_SWIFT_NETWORK_GATE_H
#include <stdint.h>
int socket_gate_current_scope(void);
void loader_swift_network_set_blocked(int blocked);
uint32_t loader_swift_network_outstanding(void);
#endif
