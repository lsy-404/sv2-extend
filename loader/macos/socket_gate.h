#ifndef SOCKET_GATE_H
#define SOCKET_GATE_H
#include <stdint.h>
#include <sys/socket.h>
enum { SOCKET_SCOPE_HOST=0, SOCKET_SCOPE_CONTROL=1, SOCKET_SCOPE_PLUGIN=2, SOCKET_SCOPE_HOST_RELAY=3 };
void socket_gate_adopt(int fd,int scope);
int socket_gate_open(int family,int type,int protocol);
int socket_gate_connect(int fd,const struct sockaddr *address,socklen_t length);
ssize_t socket_gate_send(int fd,const void *data,size_t length,int flags);
int socket_gate_close(int fd);
int socket_gate_scope(int scope);
int socket_gate_current_scope(void);
void socket_gate_set_blocked(int blocked);
int socket_gate_is_blocked(void);
int socket_gate_discovery_incomplete(void);
uint32_t socket_gate_pending_calls(void);
uint32_t socket_gate_live_host_connections(void);
#endif
