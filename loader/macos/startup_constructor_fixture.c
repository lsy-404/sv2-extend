#include "startup_policy.h"
#include <arpa/inet.h>
#include <dlfcn.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <unistd.h>
/* Independent executable constructor: proves policy precedes host constructors,
   rather than observing only a later plugin callback. No SV2 identity claims. */
__attribute__((constructor)) static void host_constructor(void) {
    int (*get_policy)(loader_startup_network_state_v1*)=dlsym(RTLD_DEFAULT,"loader_get_startup_network_policy");
    int (*get_lease)(loader_profile_lease_state_v1*)=dlsym(RTLD_DEFAULT,"loader_get_profile_lease_state");
    loader_startup_network_state_v1 policy={.abi_version=1,.struct_size=sizeof policy};
    loader_profile_lease_state_v1 lease={.abi_version=1,.struct_size=sizeof lease};
    if(!get_policy||!get_lease)_exit(90);
    int p=get_policy(&policy),l=get_lease(&lease);
    struct sockaddr_in address={.sin_family=AF_INET,.sin_port=htons((unsigned short)atoi(getenv("EXTEND_TEST_PORT")))};
    inet_pton(AF_INET,"127.0.0.1",&address.sin_addr);
    int fd=socket(AF_INET,SOCK_STREAM,0);errno=0;int result=connect(fd,(void*)&address,sizeof address),error=errno;
    if(result==0)send(fd,"constructor-payload",19,0);if(fd>=0)close(fd);
    printf("{\"policyCode\":%d,\"leaseCode\":%d,\"blocked\":%u,\"valid\":%u,\"applied\":%u,\"leaseMode\":%u,\"connect\":%d,\"errno\":%d}\n",p,l,policy.requested_blocked,policy.policy_valid,policy.bootstrap_applied,lease.mode,result,error);fflush(stdout);
}
int main(void){puts("HOST_MAIN");return 0;}
