// Diagnostic owns this socket and its guard ID. Never touches a system-owned fd.
#include <arpa/inet.h>
#include <dlfcn.h>
#include <libproc.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/proc_info.h>
#include <sys/socket.h>
#include <unistd.h>
typedef int(*change_guard_fn)(int,const uint64_t*,unsigned,const uint64_t*,unsigned,int*);
int main(void){
 void(*policy)(int)=dlsym(RTLD_DEFAULT,"socket_gate_set_blocked");unsigned(*live)(void)=dlsym(RTLD_DEFAULT,"socket_gate_live_host_connections");int(*incomplete)(void)=dlsym(RTLD_DEFAULT,"socket_gate_discovery_incomplete");change_guard_fn change=(change_guard_fn)dlsym(RTLD_DEFAULT,"change_fdguard_np");if(!policy||!live||!incomplete||!change)return 90;
 int listener=socket(AF_INET,SOCK_STREAM,0);struct sockaddr_in address={.sin_family=AF_INET,.sin_addr.s_addr=htonl(INADDR_LOOPBACK)};socklen_t size=sizeof address;
 if(listener<0||bind(listener,(void*)&address,sizeof address)||listen(listener,1)||getsockname(listener,(void*)&address,&size))return 91;
 printf("PORT %u\n",ntohs(address.sin_port));fflush(stdout);int child=accept(listener,NULL,NULL);if(child<0)return 92;
 uint64_t guard=0x455854454e445052ULL;if(change(child,NULL,0,&guard,PROC_FI_GUARD_DUP,NULL))return 93;
 policy(1);struct socket_fdinfo info;int observed=proc_pidfdinfo(getpid(),child,PROC_PIDFDSOCKETINFO,&info,sizeof info)==sizeof info;
 unsigned held=live();int gap=incomplete(),guardKept=observed&&(info.pfi.fi_guardflags&PROC_FI_GUARD_DUP)!=0;
 // Only DUP is guarded; normal close of our own diagnostic socket is allowed.
 close(child);unsigned after=live();close(listener);
 printf("RESULT {\"guardPreserved\":%d,\"rawCoverageUnverified\":%d,\"activeWhileGuarded\":%u,\"activeAfterOwnerClose\":%u}\n",guardKept,gap,held,after);fflush(stdout);return 0;
}
