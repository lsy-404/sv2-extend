#include "socket_gate.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/uio.h>
#include <sys/stat.h>
#include <libproc.h>
#include <sys/proc_info.h>
#include <pthread.h>
#include <stdatomic.h>
#include <unistd.h>
#include <errno.h>
#ifdef SOCKET_GATE_DIAGNOSTICS
#include <stdio.h>
#endif
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#define FD_LIMIT 65536
static struct { unsigned char ip,scope,connected,revoked;uint64_t identity; } descriptors[FD_LIMIT];
static pthread_mutex_t mutex=PTHREAD_MUTEX_INITIALIZER;
static _Atomic int blocked;
static _Atomic unsigned pending;
static _Atomic uint64_t revocation_epoch;
static _Atomic int discovery_failed;
static _Thread_local int current_scope;
int socket_gate_scope(int scope) { int previous=current_scope;current_scope=scope;return previous; }
int socket_gate_current_scope(void) { return current_scope; }
int socket_gate_is_blocked(void) { return atomic_load(&blocked); }
static int host_scope(int scope) { return scope==SOCKET_SCOPE_HOST || scope==SOCKET_SCOPE_HOST_RELAY; }
// Descriptor numbers can be reused after close$NOCANCEL / system-private
// closes. Validate the current kernel object before trusting cached policy.
// The kernel's opaque socket handle is never exposed outside this module.
static int socket_identity(int fd,int *family,uint64_t *identity) {
    int saved=errno;struct stat info;*identity=0;*family=AF_UNSPEC;
    if(fstat(fd,&info)!=0 || !S_ISSOCK(info.st_mode)){errno=saved;return 0;}
    struct socket_fdinfo socketInfo;int size=proc_pidfdinfo(getpid(),fd,PROC_PIDFDSOCKETINFO,&socketInfo,sizeof socketInfo);
    if(size==(int)sizeof socketInfo){*family=socketInfo.psi.soi_family;*identity=socketInfo.psi.soi_so;errno=saved;return 1;}
    struct sockaddr_storage address;socklen_t length=sizeof address;
    if(getsockname(fd,(struct sockaddr*)&address,&length)==0)*family=address.ss_family;
    errno=saved;return 1;
}
static void refresh_with_scope_locked(int fd,int fallback_scope) {
    if(fd<0 || fd>=FD_LIMIT)return;
    int family=AF_UNSPEC;uint64_t identity=0;
    if(!socket_identity(fd,&family,&identity) || (family!=AF_INET && family!=AF_INET6)){memset(&descriptors[fd],0,sizeof descriptors[fd]);return;}
    if(identity && descriptors[fd].ip && descriptors[fd].identity==identity)return;
    // Recover scope/revocation for an alias created through an unhooked fcntl.
    if(identity)for(int other=0;other<FD_LIMIT;other++)if(other!=fd && descriptors[other].ip && descriptors[other].identity==identity){int otherFamily;uint64_t otherIdentity=0;if(socket_identity(other,&otherFamily,&otherIdentity) && otherIdentity==identity){descriptors[fd]=descriptors[other];return;}memset(&descriptors[other],0,sizeof descriptors[other]);}
    memset(&descriptors[fd],0,sizeof descriptors[fd]);descriptors[fd].ip=1;descriptors[fd].scope=fallback_scope;descriptors[fd].identity=identity;
}
static void refresh_locked(int fd){refresh_with_scope_locked(fd,current_scope);}
static void remember_locked(int fd,int family){
 if(fd<0||fd>=FD_LIMIT)return;memset(&descriptors[fd],0,sizeof descriptors[fd]);descriptors[fd].ip=family==AF_INET||family==AF_INET6;descriptors[fd].scope=current_scope;
 if(descriptors[fd].ip){int actual;socket_identity(fd,&actual,&descriptors[fd].identity);}
}
static void remember(int fd,int family){pthread_mutex_lock(&mutex);remember_locked(fd,family);pthread_mutex_unlock(&mutex);}

static int enter_io(int fd,const struct sockaddr *address) {
    int ip=address && (address->sa_family==AF_INET || address->sa_family==AF_INET6);
    int scope=current_scope,revoked=0;
    pthread_mutex_lock(&mutex);
    refresh_locked(fd);
    if(fd>=0 && fd<FD_LIMIT && descriptors[fd].ip) { ip=1;scope=descriptors[fd].scope;revoked=descriptors[fd].revoked; }
    else if(!ip) { struct sockaddr_storage local;socklen_t size=sizeof local;if(getsockname(fd,(struct sockaddr*)&local,&size)==0)ip=local.ss_family==AF_INET || local.ss_family==AF_INET6; }
    if(ip && host_scope(scope) && (atomic_load(&blocked) || revoked)) {
#ifdef SOCKET_GATE_DIAGNOSTICS
        fprintf(stderr,"RAW_DENY fd=%d blocked=%d revoked=%d\n",fd,atomic_load(&blocked),revoked);
#endif
        pthread_mutex_unlock(&mutex);errno=ENETDOWN;return 0; }
    if(ip && host_scope(scope))atomic_fetch_add(&pending,1);
    pthread_mutex_unlock(&mutex);
    return ip && host_scope(scope) ? 2 : 1;
}
static void leave_io(int permit) { if(permit==2)atomic_fetch_sub(&pending,1); }
static void connected(int fd,int result,int error) {
    if(fd<0 || fd>=FD_LIMIT || !(result==0 || error==EINPROGRESS))return;
    pthread_mutex_lock(&mutex);descriptors[fd].connected=1;
    if(descriptors[fd].ip && host_scope(descriptors[fd].scope) && atomic_load(&blocked)) { shutdown(fd,SHUT_RDWR);descriptors[fd].connected=0; }
    pthread_mutex_unlock(&mutex);
}
// Native Network.framework descriptors may forbid duplication with EXC_GUARD,
// not an errno. Never duplicate discovered descriptors or alter their guards.
// Guarded sockets are left to their owning higher-level cancellation path, and
// the raw coverage gap remains explicit. Concurrent unhooked reuse is not proved
// atomic by this registry; do not infer whole-host isolation from this scan.
static void revoke_locked(int fd){
 if(fd<0||fd>=FD_LIMIT||!descriptors[fd].ip||!host_scope(descriptors[fd].scope))return;
 descriptors[fd].revoked=1;descriptors[fd].connected=0;
 struct socket_fdinfo info;
 if(proc_pidfdinfo(getpid(),fd,PROC_PIDFDSOCKETINFO,&info,sizeof info)!=(int)sizeof info){descriptors[fd].connected=1;atomic_store(&discovery_failed,1);return;}
 // Skipping a protected resource is not its termination. Keep it outstanding
 // until its owner closes it and kernel identity refresh observes that close.
 if((info.pfi.fi_status&PROC_FP_GUARDED)||info.pfi.fi_guardflags){descriptors[fd].connected=1;atomic_store(&discovery_failed,1);return;}
 if(info.psi.soi_so&&info.psi.soi_so==descriptors[fd].identity){if(shutdown(fd,SHUT_RDWR)&&errno!=ENOTCONN){descriptors[fd].connected=1;atomic_store(&discovery_failed,1);}}
 else{descriptors[fd].connected=1;atomic_store(&discovery_failed,1);}
}

static int inherited_listener_scope_locked(int fd){
#ifdef SOCKET_GATE_DIAGNOSTICS
 fprintf(stderr,"INHERIT child=%d\n",fd);
#endif
 struct sockaddr_storage local,peer;socklen_t length=sizeof local,peerLength=sizeof peer;
 if(getsockname(fd,(void*)&local,&length)||getpeername(fd,(void*)&peer,&peerLength))return SOCKET_SCOPE_HOST;
 for(int candidate=0;candidate<FD_LIMIT;candidate++)if(candidate!=fd&&descriptors[candidate].ip&&!host_scope(descriptors[candidate].scope)){
  refresh_with_scope_locked(candidate,SOCKET_SCOPE_HOST);if(!descriptors[candidate].ip||host_scope(descriptors[candidate].scope))continue;
#ifdef SOCKET_GATE_DIAGNOSTICS
 fprintf(stderr,"INHERIT candidate=%d scope=%d\n",candidate,descriptors[candidate].scope);
#endif
  struct socket_fdinfo listenerInfo;if(proc_pidfdinfo(getpid(),candidate,PROC_PIDFDSOCKETINFO,&listenerInfo,sizeof listenerInfo)!=(int)sizeof listenerInfo||listenerInfo.psi.soi_kind!=SOCKINFO_TCP||listenerInfo.psi.soi_proto.pri_tcp.tcpsi_state!=TSI_S_LISTEN||!descriptors[candidate].identity||listenerInfo.psi.soi_so!=descriptors[candidate].identity)continue;
  struct sockaddr_storage bound; socklen_t boundLength=sizeof bound;
  if(getsockname(candidate,(void*)&bound,&boundLength)||length!=boundLength||local.ss_family!=bound.ss_family)continue;
  // Exact address/port matching, including IPv6 scope. Do not grant an exemption
  // based on a peer-selected remote port or an arbitrary bound client socket.
#ifdef SOCKET_GATE_DIAGNOSTICS
 fprintf(stderr,"INHERIT addresses family=%d localport=%u boundport=%u\n",local.ss_family,((struct sockaddr_in*)&local)->sin_port,((struct sockaddr_in*)&bound)->sin_port);
#endif
  if(local.ss_family==AF_INET){struct sockaddr_in *a=(void*)&local,*b=(void*)&bound;if(a->sin_port==b->sin_port&&a->sin_addr.s_addr==b->sin_addr.s_addr)return descriptors[candidate].scope;}
  else if(local.ss_family==AF_INET6){struct sockaddr_in6 *a=(void*)&local,*b=(void*)&bound;if(a->sin6_port==b->sin6_port&&a->sin6_scope_id==b->sin6_scope_id&&!memcmp(&a->sin6_addr,&b->sin6_addr,sizeof a->sin6_addr))return descriptors[candidate].scope;}
 }
 return SOCKET_SCOPE_HOST;
}
static void discover_locked(void){
 int bytes=proc_pidinfo(getpid(),PROC_PIDLISTFDS,0,NULL,0);
 if(bytes<=0||bytes>(int)(FD_LIMIT*sizeof(struct proc_fdinfo))){atomic_store(&discovery_failed,1);return;}
 size_t capacity=(size_t)bytes+32*sizeof(struct proc_fdinfo);
 struct proc_fdinfo *fds=malloc(capacity);if(!fds){atomic_store(&discovery_failed,1);return;}
 int used=proc_pidinfo(getpid(),PROC_PIDLISTFDS,0,fds,(int)capacity);
 if(used<=0||(size_t)used>=capacity||used%(int)sizeof(*fds)){atomic_store(&discovery_failed,1);free(fds);return;}
 for(size_t i=0;i<(size_t)used/sizeof(*fds);i++)if(fds[i].proc_fdtype==PROX_FDTYPE_SOCKET){
  int fd=(int)fds[i].proc_fd;if(fd<0||fd>=FD_LIMIT){atomic_store(&discovery_failed,1);continue;}
  // An unknown kernel socket is host-owned, never implicitly exempt because
  // the plugin/control thread happened to request the policy transition.
  uint64_t priorIdentity=descriptors[fd].ip?descriptors[fd].identity:0;refresh_with_scope_locked(fd,SOCKET_SCOPE_HOST);
  if((!priorIdentity||descriptors[fd].identity!=priorIdentity)&&descriptors[fd].ip&&host_scope(descriptors[fd].scope))descriptors[fd].scope=inherited_listener_scope_locked(fd);
 }
 free(fds);
}
void socket_gate_set_blocked(int value) {
 pthread_mutex_lock(&mutex);atomic_store(&blocked,value!=0);
 if(value){atomic_fetch_add(&revocation_epoch,1);discover_locked();for(int fd=0;fd<FD_LIMIT;fd++)if(descriptors[fd].ip){refresh_with_scope_locked(fd,SOCKET_SCOPE_HOST);revoke_locked(fd);}}
 pthread_mutex_unlock(&mutex);
}
int socket_gate_discovery_incomplete(void){return atomic_load(&discovery_failed);}

uint32_t socket_gate_pending_calls(void) { return atomic_load(&pending); }
uint32_t socket_gate_live_host_connections(void) {
    uint32_t count=0;pthread_mutex_lock(&mutex);
    for(int fd=0;fd<FD_LIMIT;fd++)if(descriptors[fd].ip){refresh_locked(fd);if(descriptors[fd].ip && host_scope(descriptors[fd].scope) && descriptors[fd].connected)count++;}
    pthread_mutex_unlock(&mutex);return count;
}
static int gated_socket(int family,int type,int protocol) {
 // The syscall allocates only a descriptor; hold the registry lock until scope
 // is recorded so a concurrent discovery cannot mistake a control fd for host.
 pthread_mutex_lock(&mutex);
 if((family==AF_INET||family==AF_INET6)&&host_scope(current_scope)&&atomic_load(&blocked)){pthread_mutex_unlock(&mutex);errno=ENETDOWN;return -1;}
 int fd=socket(family,type,protocol),error=errno;remember_locked(fd,family);pthread_mutex_unlock(&mutex);errno=error;return fd;
}

static int gated_accept(int listener,struct sockaddr *address,socklen_t *length){
 uint64_t epoch=atomic_load(&revocation_epoch);int permit=enter_io(listener,NULL);if(!permit)return -1;
 pthread_mutex_lock(&mutex);refresh_locked(listener);int scope=listener>=0&&listener<FD_LIMIT&&descriptors[listener].ip?descriptors[listener].scope:current_scope;pthread_mutex_unlock(&mutex);
 int child=accept(listener,address,length),error=errno;
 if(child>=0){pthread_mutex_lock(&mutex);refresh_with_scope_locked(child,scope);
  if(child<FD_LIMIT&&descriptors[child].ip){descriptors[child].scope=scope;descriptors[child].connected=1;if(host_scope(scope)&&(atomic_load(&blocked)||atomic_load(&revocation_epoch)!=epoch))revoke_locked(child);}
  else if(child>=FD_LIMIT)atomic_store(&discovery_failed,1);
  pthread_mutex_unlock(&mutex);
 }
 leave_io(permit);errno=error;return child;
}
static int gated_connect(int fd,const struct sockaddr *address,socklen_t length) {
    int permit=enter_io(fd,address);if(!permit)return -1;
    int result=connect(fd,address,length),error=errno;connected(fd,result,error);leave_io(permit);errno=error;return result;
}
static int gated_connectx(int fd,const sa_endpoints_t *endpoints,sae_associd_t assoc,unsigned flags,const struct iovec *iov,unsigned count,size_t *length,sae_connid_t *connection) {
    int permit=enter_io(fd,endpoints?endpoints->sae_dstaddr:NULL);if(!permit)return -1;
    int result=connectx(fd,endpoints,assoc,flags,iov,count,length,connection),error=errno;connected(fd,result,error);leave_io(permit);errno=error;return result;
}
static ssize_t gated_send(int fd,const void *data,size_t length,int flags) { int p=enter_io(fd,NULL);if(!p)return -1;ssize_t r=send(fd,data,length,flags);int e=errno;leave_io(p);errno=e;return r; }
static ssize_t gated_sendto(int fd,const void *data,size_t length,int flags,const struct sockaddr *address,socklen_t size) { int p=enter_io(fd,address);if(!p)return -1;ssize_t r=sendto(fd,data,length,flags,address,size);int e=errno;leave_io(p);errno=e;return r; }
static ssize_t gated_sendmsg(int fd,const struct msghdr *message,int flags) { int p=enter_io(fd,message?message->msg_name:NULL);if(!p)return -1;ssize_t r=sendmsg(fd,message,flags);int e=errno;leave_io(p);errno=e;return r; }
static ssize_t gated_write(int fd,const void *data,size_t length) { int p=enter_io(fd,NULL);if(!p)return -1;ssize_t r=write(fd,data,length);int e=errno;leave_io(p);errno=e;return r; }
static ssize_t gated_writev(int fd,const struct iovec *data,int count) { int p=enter_io(fd,NULL);if(!p)return -1;ssize_t r=writev(fd,data,count);int e=errno;leave_io(p);errno=e;return r; }
static ssize_t gated_recv(int fd,void *data,size_t length,int flags) { int p=enter_io(fd,NULL);if(!p)return -1;ssize_t r=recv(fd,data,length,flags);int e=errno;leave_io(p);errno=e;return r; }
static ssize_t gated_recvfrom(int fd,void *data,size_t length,int flags,struct sockaddr *address,socklen_t *size) { int p=enter_io(fd,NULL);if(!p)return -1;ssize_t r=recvfrom(fd,data,length,flags,address,size);int e=errno;leave_io(p);errno=e;return r; }
static ssize_t gated_recvmsg(int fd,struct msghdr *message,int flags) { int p=enter_io(fd,NULL);if(!p)return -1;ssize_t r=recvmsg(fd,message,flags);int e=errno;leave_io(p);errno=e;return r; }
static ssize_t gated_read(int fd,void *data,size_t length) { int p=enter_io(fd,NULL);if(!p)return -1;ssize_t r=read(fd,data,length);int e=errno;leave_io(p);errno=e;return r; }
static ssize_t gated_readv(int fd,const struct iovec *data,int count) { int p=enter_io(fd,NULL);if(!p)return -1;ssize_t r=readv(fd,data,count);int e=errno;leave_io(p);errno=e;return r; }
static int gated_sendfile(int source,int target,off_t offset,off_t *length,struct sf_hdtr *headers,int flags) { int p=enter_io(target,NULL);if(!p)return -1;int r=sendfile(source,target,offset,length,headers,flags),e=errno;leave_io(p);errno=e;return r; }
static int gated_dup(int fd) {pthread_mutex_lock(&mutex);refresh_locked(fd);int result=dup(fd);if(result>=0 && result<FD_LIMIT && fd>=0 && fd<FD_LIMIT)descriptors[result]=descriptors[fd];pthread_mutex_unlock(&mutex);return result;}
static int gated_dup2(int fd,int target) {pthread_mutex_lock(&mutex);refresh_locked(fd);int result=dup2(fd,target);if(result>=0 && result<FD_LIMIT){if(fd>=0 && fd<FD_LIMIT)descriptors[result]=descriptors[fd];else memset(&descriptors[result],0,sizeof descriptors[result]);}pthread_mutex_unlock(&mutex);return result;}
static int gated_close(int fd) { pthread_mutex_lock(&mutex);if(fd>=0 && fd<FD_LIMIT)memset(&descriptors[fd],0,sizeof descriptors[fd]);int result=close(fd),error=errno;pthread_mutex_unlock(&mutex);errno=error;return result; }
#define INTERPOSE(replacement,original) __attribute__((used)) static const struct { const void *new_function; const void *old_function; } pair_##original __attribute__((section("__DATA,__interpose"))) = {(const void*)(replacement),(const void*)(original)};
INTERPOSE(gated_socket,socket)
INTERPOSE(gated_accept,accept)
INTERPOSE(gated_connect,connect)
INTERPOSE(gated_connectx,connectx)
INTERPOSE(gated_send,send)
INTERPOSE(gated_sendto,sendto)
INTERPOSE(gated_sendmsg,sendmsg)
INTERPOSE(gated_write,write)
INTERPOSE(gated_writev,writev)
INTERPOSE(gated_close,close)
INTERPOSE(gated_recv,recv)
INTERPOSE(gated_recvfrom,recvfrom)
INTERPOSE(gated_recvmsg,recvmsg)
INTERPOSE(gated_read,read)
INTERPOSE(gated_readv,readv)
INTERPOSE(gated_sendfile,sendfile)
INTERPOSE(gated_dup,dup)
INTERPOSE(gated_dup2,dup2)

void socket_gate_adopt(int fd,int scope) {int old=socket_gate_scope(scope);struct sockaddr_storage a;socklen_t n=sizeof a;if(getsockname(fd,(struct sockaddr*)&a,&n)==0)remember(fd,a.ss_family);int one=1;setsockopt(fd,SOL_SOCKET,SO_NOSIGPIPE,&one,sizeof one);socket_gate_scope(old);}
int socket_gate_open(int family,int type,int protocol) {int fd=gated_socket(family,type,protocol);if(fd>=0){int one=1;setsockopt(fd,SOL_SOCKET,SO_NOSIGPIPE,&one,sizeof one);}return fd;}
int socket_gate_connect(int fd,const struct sockaddr *address,socklen_t length) {return gated_connect(fd,address,length);}
ssize_t socket_gate_send(int fd,const void *data,size_t length,int flags) {return gated_send(fd,data,length,flags);}
int socket_gate_close(int fd) {return gated_close(fd);}
