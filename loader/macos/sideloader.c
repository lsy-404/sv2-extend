/* No-patch launch probe: only sets child environment, then execs official host. */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
int main(int argc,char **argv) {
    if(argc!=4) { fputs("usage: sideloader HOST_EXECUTABLE LOADER_DYLIB PLUGIN_DYLIB\n",stderr); return 64; }
    if(setenv("DYLD_INSERT_LIBRARIES",argv[2],1)!=0 || setenv("LOADER_PROBE_PLUGIN",argv[3],1)!=0) return 70;
    fprintf(stderr,"SIDELOADER pid=%d exec-official-host\n",getpid()); fflush(stderr);
    char *hostArgs[]={argv[1],NULL};
    execv(argv[1],hostArgs);perror("execv");return 71;
}
