#import <Foundation/Foundation.h>
#import <objc/runtime.h>
#include "loader_api.h"
#include "socket_gate.h"
#include <curl/curl.h>
extern int loader_urlsession_callbacks_install(void);
extern void loader_urlsession_callback_resumed(NSURLSessionTask*);
extern uint32_t loader_urlsession_callbacks_pending(void);
extern void network_framework_gate_set_blocked(int);
extern uint32_t network_framework_gate_active(void);
extern int webkit_gate_install(void);
extern void webkit_gate_update_policy(void);
extern uint32_t webkit_gate_pending_updates(void);
extern uint32_t webkit_gate_active_relays(void);
extern uint32_t loader_web_retirement_pending(void);
#include <stdio.h>
#include <unistd.h>
static NSLock *lock;
static NSRecursiveLock *transitionLock;
static BOOL transitioning;
static NSHashTable<NSURLSessionTask *> *tasks;
static NSHashTable<NSURLSessionTask *> *pluginTasks;
static BOOL blocked;
static BOOL webkitInstalled;
static uint64_t generation;
static BOOL isTaskClass(Class cls) {
    for (Class c=cls; c; c=class_getSuperclass(c)) if(c==[NSURLSessionTask class]) return YES;
    return NO;
}
int network_gate_install(void) {
    if(lock) return 0;
    webkitInstalled=webkit_gate_install()==0;
    lock=[NSLock new]; transitionLock=[NSRecursiveLock new]; tasks=[NSHashTable weakObjectsHashTable]; pluginTasks=[NSHashTable weakObjectsHashTable];
    // Instantiate Foundation's ordinary data-task implementation before inspecting classes.
    NSURLSession *warm=[NSURLSession sessionWithConfiguration:[NSURLSessionConfiguration ephemeralSessionConfiguration]];
    [[warm dataTaskWithURL:[NSURL URLWithString:@"http://127.0.0.1/"]] cancel];
    [warm invalidateAndCancel];
    loader_urlsession_callbacks_install();
    unsigned count=0, hooked=0;
    Class *classes=objc_copyClassList(&count);
    for(unsigned i=0;i<count;i++) {
        Class cls=classes[i]; if(!isTaskClass(cls)) continue;
        unsigned methodsCount=0; Method *methods=class_copyMethodList(cls,&methodsCount);
        for(unsigned j=0;j<methodsCount;j++) {
            Method method=methods[j]; if(method_getName(method)!=@selector(resume)) continue;
            IMP original=method_getImplementation(method);
            IMP replacement=imp_implementationWithBlock(^(NSURLSessionTask *task) {
                loader_urlsession_callback_resumed(task);
                [lock lock]; [tasks addObject:task];
                BOOL exempt=[pluginTasks containsObject:task];
                BOOL deny=blocked && !exempt; [lock unlock];
                fprintf(stderr,"NETWORK_GATE pid=%d source=%s destination=%s deny=%d\n",getpid(),exempt?"plugin":"host",(task.originalRequest.URL.host ?: @"unknown").UTF8String,deny); fflush(stderr);
                if(deny) { fprintf(stderr,"NETWORK_GATE pid=%d deny-resume\n",getpid()); [task cancel]; }
                ((void(*)(id,SEL))original)(task,@selector(resume));
            });
            method_setImplementation(method,replacement); hooked++;
        }
        free(methods);
    }
    free(classes);
    fprintf(stderr,"NETWORK_GATE pid=%d hooked-resume-methods=%u scope=NSURLSessionTask\n",getpid(),hooked);
    return hooked ? 0 : -1;
}
int network_gate_set(int value) {
    if(!lock) return -1;
    [transitionLock lock];
    if(transitioning){[transitionLock unlock];return LOADER_ERR_INVALID;}
    transitioning=YES;
    @try {
    socket_gate_set_blocked(value);
    network_framework_gate_set_blocked(value);
    webkit_gate_update_policy();
    [lock lock]; if(blocked!=(value!=0))generation++; blocked=value!=0;
    NSArray *cancel=blocked ? [tasks allObjects] : @[];
    NSHashTable *exempt=[NSHashTable weakObjectsHashTable];
    for(NSURLSessionTask *task in [pluginTasks allObjects]) [exempt addObject:task];
    [lock unlock];
    // The whole transition is serialized, including native cancellation. Do not
    // hold the task-table lock across foreign SDK methods or their callbacks.
    for(NSURLSessionTask *task in cancel) if(![exempt containsObject:task] && task.state!=NSURLSessionTaskStateCompleted) [task cancel];
    fprintf(stderr,"NETWORK_GATE pid=%d blocked=%d\n",getpid(),value!=0);
    return 0;
    } @finally {transitioning=NO;[transitionLock unlock];}
}
int network_gate_get(void) { [lock lock]; int result=blocked; [lock unlock]; return result; }

int network_gate_state(loader_network_state_v1 *state) {
    if(!state || state->abi_version!=LOADER_ABI_V1 || state->struct_size<sizeof(*state) || !lock) return LOADER_ERR_INVALID;
    [transitionLock lock];
    if(transitioning){[transitionLock unlock];return LOADER_ERR_INVALID;}
    @try {
    [lock lock];
    uint32_t active=0;
    for(NSURLSessionTask *task in [tasks allObjects])
        if(![pluginTasks containsObject:task] && task.state!=NSURLSessionTaskStateCompleted) active++;
    uint64_t currentGeneration=generation;BOOL currentBlocked=blocked;[lock unlock];
    *state=(loader_network_state_v1){ .abi_version=LOADER_ABI_V1, .struct_size=sizeof(*state), .generation=currentGeneration,
        .intercepted_paths=LOADER_NET_URLSESSION_RESUME|LOADER_NET_RAW_SOCKET|LOADER_NET_NETWORK_FRAMEWORK|(webkitInstalled?LOADER_NET_WEBKIT:0),
        .unverified_paths=(socket_gate_discovery_incomplete()?LOADER_NET_RAW_SOCKET:0)|LOADER_NET_WEBKIT|LOADER_NET_WEBRTC|LOADER_NET_NETWORK_FRAMEWORK|LOADER_NET_BACKGROUND_SESSION|LOADER_NET_SWIFT_NETWORK|LOADER_NET_GROUP_MULTIPLEX,
        .requested_blocked=currentBlocked, .tracked_host_tasks_active=active, .tracked_tasks_drained=active==0 && socket_gate_pending_calls()==0 && socket_gate_live_host_connections()==0 && webkit_gate_active_relays()==0 && webkit_gate_pending_updates()==0 && network_framework_gate_active()==0 && loader_urlsession_callbacks_pending()==0 && loader_web_retirement_pending()==0, .whole_host_verified=0 };
    return LOADER_OK;
    } @finally {[transitionLock unlock];}
}
int network_gate_verify_quiescence(uint64_t expected,loader_network_state_v1 *state) {
    int result=network_gate_state(state); if(result!=LOADER_OK)return result;
    if(state->generation!=expected)return LOADER_ERR_STALE_GENERATION;
    // Explicitly reject readiness: untracked transports can still send traffic.
    return LOADER_ERR_NETWORK_UNVERIFIED;
}
