/* Read-only permission probe. Obtains/releases a port; never reads/writes target memory. */
#include <mach/mach.h>
#include <stdio.h>
#include <stdlib.h>
int main(int argc,char **argv) {
    if(argc!=2)return 64;
    char *end=NULL;long pid=strtol(argv[1],&end,10);if(!end || *end || pid<=0)return 64;
    mach_port_t port=MACH_PORT_NULL;
    kern_return_t result=task_for_pid(mach_task_self(),(int)pid,&port);
    printf("{\"pid\":%ld,\"kern_result\":%d,\"description\":\"%s\",\"port_granted\":%s}\n",pid,result,mach_error_string(result),port!=MACH_PORT_NULL?"true":"false");
    if(port!=MACH_PORT_NULL)mach_port_deallocate(mach_task_self(),port);
    return 0;
}
