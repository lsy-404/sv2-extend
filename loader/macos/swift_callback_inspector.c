// Read-only diagnostic. Never duplicates or changes an fd guard.
#include "swift_callback_fixture.h"
#include <libproc.h>
#include <sys/proc_info.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <unistd.h>
uint32_t swift_probe_guarded_inet_count(void){
 int length=proc_pidinfo(getpid(),PROC_PIDLISTFDS,0,NULL,0);if(length<=0||length>1048576)return 0;
 length+=256;struct proc_fdinfo *fds=malloc((size_t)length);if(!fds)return 0;
 int used=proc_pidinfo(getpid(),PROC_PIDLISTFDS,0,fds,length);uint32_t count=0;
 if(used>0&&used<length)for(size_t i=0;i<(size_t)used/sizeof(*fds);i++)if(fds[i].proc_fdtype==PROX_FDTYPE_SOCKET){
  struct socket_fdinfo info;if(proc_pidfdinfo(getpid(),(int)fds[i].proc_fd,PROC_PIDFDSOCKETINFO,&info,sizeof info)!=(int)sizeof info)continue;
  if((info.psi.soi_family==AF_INET||info.psi.soi_family==AF_INET6)&&((info.pfi.fi_status&PROC_FP_GUARDED)||info.pfi.fi_guardflags))count++;
 }
 free(fds);return count;
}
