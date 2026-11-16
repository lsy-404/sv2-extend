#include "socket_gate.h"
#include <stdio.h>
#include <stdlib.h>
#include <dlfcn.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <sys/uio.h>
#include <arpa/inet.h>
static int (*makeSocket)(int,int,int),(*scope)(int),(*nativeClose)(int);
static void (*policy)(int);
static int arm(void){scope(SOCKET_SCOPE_HOST);policy(0);int fd=makeSocket(AF_INET,SOCK_DGRAM,0);if(fd<0)exit(2);policy(1);if(nativeClose(fd))exit(3);return fd;}
int main(int argc,char **argv){
 if(argc!=2)return 64;makeSocket=dlsym(RTLD_DEFAULT,"socket_gate_open");scope=dlsym(RTLD_DEFAULT,"socket_gate_scope");policy=dlsym(RTLD_DEFAULT,"socket_gate_set_blocked");nativeClose=dlsym(RTLD_DEFAULT,"close$NOCANCEL");if(!makeSocket||!scope||!policy||!nativeClose)return 65;
 int wanted=arm();int file=open(argv[1],O_CREAT|O_EXCL|O_RDWR,0600);if(file!=wanted)return 66;errno=0;ssize_t writeResult=write(file,"saved",5);int writeError=errno;int blockedFileWrite=writeResult==5;lseek(file,0,SEEK_SET);char bytes[8]={0};int blockedFileRead=read(file,bytes,5)==5&&!memcmp(bytes,"saved",5);policy(0);lseek(file,0,SEEK_END);struct iovec vectors[]={{.iov_base="-",.iov_len=1},{.iov_base="ok",.iov_len=2}};int restoredFileWritev=writev(file,vectors,2)==3;close(file);
 wanted=arm();int pair[2];if(socketpair(AF_UNIX,SOCK_STREAM,0,pair)||pair[0]!=wanted)return 67;int localWrite=write(pair[0],"L",1)==1;int localRead=0;if(localWrite)localRead=read(pair[1],bytes,1)==1&&bytes[0]=='L';close(pair[0]);close(pair[1]);
 wanted=arm();int pipes[2];if(pipe(pipes)||pipes[0]!=wanted)return 68;int pipeWrite=write(pipes[1],"P",1)==1;int pipeRead=pipeWrite&&read(pipes[0],bytes,1)==1&&bytes[0]=='P';close(pipes[0]);close(pipes[1]);policy(0);
 scope(SOCKET_SCOPE_CONTROL);int receiver=makeSocket(AF_INET,SOCK_DGRAM,0);struct sockaddr_in address={.sin_family=AF_INET,.sin_addr.s_addr=htonl(INADDR_LOOPBACK)};if(bind(receiver,(struct sockaddr*)&address,sizeof address))return 69;socklen_t size=sizeof address;getsockname(receiver,(struct sockaddr*)&address,&size);scope(SOCKET_SCOPE_HOST);int original=makeSocket(AF_INET,SOCK_DGRAM,0);policy(1);policy(0);int alias=fcntl(original,F_DUPFD,original+1);errno=0;ssize_t sent=sendto(alias,"X",1,0,(struct sockaddr*)&address,sizeof address);int aliasError=errno;int revokedAliasRejected=sent==-1&&aliasError==ENETDOWN;close(alias);close(original);close(receiver);
 printf("{\"blockedFileWrite\":%s,\"blockedFileRead\":%s,\"restoredFileWritev\":%s,\"localSocketWrite\":%s,\"localSocketRead\":%s,\"pipeRead\":%s,\"revokedFcntlAliasRejected\":%s,\"writeError\":%d,\"aliasSendResult\":%zd,\"aliasError\":%d}\n",blockedFileWrite?"true":"false",blockedFileRead?"true":"false",restoredFileWritev?"true":"false",localWrite?"true":"false",localRead?"true":"false",pipeRead?"true":"false",revokedAliasRejected?"true":"false",writeError,sent,aliasError);return 0;
}
