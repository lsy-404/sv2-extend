#import <Foundation/Foundation.h>
#include "loader_api.h"
static loader_editor_dispatch handler;
int socket_gate_scope(int scope){(void)scope;return 0;}
int loader_register_capability_provider(const char *name,loader_editor_dispatch dispatch,void *context,const char *caps){(void)name;(void)context;(void)caps;handler=dispatch;return 0;}
extern int loader_http_capability_install(void);
static void receive(const char *bytes,size_t length,void *context){fwrite(bytes,1,length,stdout);fflush(stdout);dispatch_semaphore_signal((__bridge dispatch_semaphore_t)context);}
int main(int argc,char **argv){@autoreleasepool{if(argc!=3)return 2;loader_http_capability_install();dispatch_semaphore_t done=dispatch_semaphore_create(0);handler(argv[1],argv[2],receive,(__bridge void*)done,NULL);return dispatch_semaphore_wait(done,dispatch_time(DISPATCH_TIME_NOW,12*NSEC_PER_SEC))?3:0;}}
