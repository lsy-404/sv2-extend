// Diagnostic only: temporary policy transitions in one disposable profile.
#import <Foundation/Foundation.h>
#include "loader_api.h"
#include <fcntl.h>
#include <unistd.h>
#include <dlfcn.h>
static const loader_api_v1 *api;
static void change(int blocked,const char *phase){
 int code=api->set_host_network_blocked(blocked);loader_network_state_v1 state={.abi_version=1,.struct_size=sizeof state};int snapshot=api->get_host_network_state(&state);
 const char *raw=getenv("OPEN_SV2_LOADER_DATA_DIR");NSString *path=[@(raw) stringByAppendingPathComponent:@"network-control-probe.json"];
 char text[512];int length=snprintf(text,sizeof text,"{\"phase\":\"%s\",\"pid\":%d,\"code\":%d,\"snapshot\":%d,\"blocked\":%u,\"wholeHostVerified\":%u}",phase,getpid(),code,snapshot,state.requested_blocked,state.whole_host_verified);
 int fd=open(path.fileSystemRepresentation,O_WRONLY|O_CREAT|O_TRUNC|O_NOFOLLOW|O_CLOEXEC,0600);if(fd>=0){ssize_t result=write(fd,text,(size_t)length);(void)result;close(fd);}
}
static void waitReady(unsigned attempt){
 BOOL(*online)(void)=dlsym(RTLD_DEFAULT,"loader_lua_editor_online");
 if(!online||!online()){if(attempt<300)dispatch_after(dispatch_time(DISPATCH_TIME_NOW,NSEC_PER_SEC/10),dispatch_get_main_queue(),^{waitReady(attempt+1);});return;}
 change(1,"blocked");dispatch_after(dispatch_time(DISPATCH_TIME_NOW,5*NSEC_PER_SEC),dispatch_get_main_queue(),^{change(0,"restored");});
}
int loader_plugin_init_v1(const loader_api_v1 *value){api=value;if(!api||api->host_pid!=getpid())return -1;
 dispatch_async(dispatch_get_main_queue(),^{waitReady(0);});return 0;
}
