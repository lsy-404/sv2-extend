#include <arpa/inet.h>
#include <dlfcn.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <unistd.h>
int main(int argc,const char **argv){
 int bypass=argc>1&&atoi(argv[1]),control=argc>2&&atoi(argv[2]),ipv6=argc>3&&atoi(argv[3]);
 void(*policy)(int)=dlsym(RTLD_DEFAULT,"socket_gate_set_blocked");unsigned(*live)(void)=dlsym(RTLD_DEFAULT,"socket_gate_live_host_connections");if(!policy||!live)return 90;
 int(*scope)(int)=dlsym(RTLD_DEFAULT,"socket_gate_scope");if(control)scope(1);
 int listener=socket(ipv6?AF_INET6:AF_INET,SOCK_STREAM,0);struct sockaddr_storage address={0};socklen_t size;
 if(ipv6){struct sockaddr_in6 *a=(void*)&address;a->sin6_family=AF_INET6;a->sin6_addr=in6addr_loopback;size=sizeof *a;}
 else{struct sockaddr_in *a=(void*)&address;a->sin_family=AF_INET;a->sin_addr.s_addr=htonl(INADDR_LOOPBACK);size=sizeof *a;}
 if(listener<0||bind(listener,(void*)&address,size)||listen(listener,1)||getsockname(listener,(void*)&address,&size))return 91;
 printf("PORT %u\n",ntohs(ipv6?((struct sockaddr_in6*)&address)->sin6_port:((struct sockaddr_in*)&address)->sin_port));fflush(stdout);
 if(control)scope(0); // Accepted sockets inherit the listener, not this thread.
 int(*acceptFunction)(int,struct sockaddr*,socklen_t*)=bypass?(int(*)(int,struct sockaddr*,socklen_t*))dlsym(RTLD_DEFAULT,"accept$NOCANCEL"):accept;if(!acceptFunction)return 94;
 int child=acceptFunction(listener,NULL,NULL);if(child<0)return 92;
 unsigned before=live();policy(1);puts("BLOCKED");fflush(stdout);char command;if(read(STDIN_FILENO,&command,1)!=1)return 93;
 policy(0);errno=0;ssize_t sent=send(child,"revived-accepted",16,0);int error=errno;
 printf("RESULT {\"liveBeforeBlock\":%u,\"oldAcceptedSendAfterRestore\":%ld,\"sendErrno\":%d}\n",before,(long)sent,error);fflush(stdout);close(child);close(listener);return 0;
}
