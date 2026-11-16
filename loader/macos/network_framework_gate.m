// Public Network.framework entry points. Private entry points, background
// daemons and further private/multiplex boundaries remain unverified.
#import <Foundation/Foundation.h>
#import <Network/Network.h>
#include "socket_gate.h"
#include <sys/socket.h>
#include <dlfcn.h>
#include <string.h>
#include <stdio.h>
// Optional lookup keeps standalone C diagnostics usable without Swift.
static NSRecursiveLock *gateLock;
static NSMapTable *connections,*passiveConnections,*groups,*passiveGroups;
static BOOL denied;
static uint64_t deniedStarts,deniedSends,hostCallbacks,groupSequence;
static void gated_handler(nw_connection_t,nw_connection_state_changed_handler_t);
static void gated_group_handler(nw_connection_group_t,nw_connection_group_state_changed_handler_t);
static void initialize(void){static dispatch_once_t once;dispatch_once(&once,^{gateLock=[NSRecursiveLock new];connections=[NSMapTable strongToStrongObjectsMapTable];passiveConnections=[NSMapTable weakToStrongObjectsMapTable];groups=[NSMapTable strongToStrongObjectsMapTable];passiveGroups=[NSMapTable weakToStrongObjectsMapTable];});}
static BOOL host(NSNumber *scope){return !scope || scope.intValue==SOCKET_SCOPE_HOST || scope.intValue==SOCKET_SCOPE_HOST_RELAY;}
static NSMutableDictionary *entry(nw_connection_t connection,BOOL create){
 id object=(id)connection;NSMutableDictionary *value=[connections objectForKey:object]?:[passiveConnections objectForKey:object];
 if(!value && create && connections.count<4096){value=[@{@"scope":@(socket_gate_current_scope()),@"active":@NO,@"revoked":@(denied&&host(@(socket_gate_current_scope())))} mutableCopy];[passiveConnections setObject:value forKey:object];}
 return value;
}
// NSURLSession/WebKit own their underlying connections and must receive their
// cancellation through their higher-level gates. Forcing their private NW state
// independently can violate CFNetwork's transport state machine.
static BOOL frameworkCaller(void *address){Dl_info info={0};if(!dladdr(address,&info)||!info.dli_fname)return NO;const char *path=info.dli_fname;if(strncmp(path,"/System/Library/",16) && strncmp(path,"/usr/lib/",9))return NO;return strstr(path,"/CFNetwork.framework/") || strstr(path,"/Foundation.framework/") || strstr(path,"/WebKit.framework/") || strstr(path,"/Network.framework/") || strstr(path,"/libnetwork.dylib");}
#ifdef NW_GATE_DIAGNOSTICS
static void groupCall(const char *name,void *address){Dl_info info={0};dladdr(address,&info);fprintf(stderr,"GROUP_CALL %s %s %s\n",name,info.dli_fname?:"?",info.dli_sname?:"?");}
#define GROUP_CALL(name) groupCall(name,__builtin_return_address(0))
#else
#define GROUP_CALL(name) ((void)0)
#endif
static BOOL permit(nw_connection_t connection,BOOL start,BOOL managed){
 if(managed && !entry(connection,NO))return YES;
 NSMutableDictionary *value=entry(connection,YES);if(value&&[value[@"terminal"] boolValue]&&!host(value[@"scope"]))return YES;BOOL reject=!value || (host(value[@"scope"]) && (denied || [value[@"revoked"] boolValue]));
 if(reject){if(start)deniedStarts++;else deniedSends++;if(value){value[@"revoked"]=@YES;if(![value[@"terminal"] boolValue]){value[@"cancelling"]=@YES;[connections setObject:value forKey:(id)connection];[passiveConnections removeObjectForKey:(id)connection];}}}
 else {value[@"active"]=@YES;[connections setObject:value forKey:(id)connection];[passiveConnections removeObjectForKey:(id)connection];}
 return !reject;
}
void network_framework_gate_set_blocked(int blocked){
 void (*swiftPolicy)(int)=dlsym(RTLD_DEFAULT,"loader_swift_network_set_blocked");if(swiftPolicy)swiftPolicy(blocked);
 initialize();[gateLock lock];denied=blocked!=0;
 if(denied){for(id object in passiveConnections.keyEnumerator.allObjects){NSMutableDictionary *value=[passiveConnections objectForKey:object];if(host(value[@"scope"]))value[@"revoked"]=@YES;}for(id object in passiveGroups.keyEnumerator.allObjects){NSMutableDictionary *value=[passiveGroups objectForKey:object];if(host(value[@"scope"]))value[@"revoked"]=@YES;}for(id object in connections.keyEnumerator.allObjects){NSMutableDictionary *value=[connections objectForKey:object];if(host(value[@"scope"])) {value[@"revoked"]=@YES;value[@"cancelling"]=@YES;nw_connection_force_cancel((nw_connection_t)object);}}for(id object in groups.keyEnumerator.allObjects){NSMutableDictionary *value=[groups objectForKey:object];if(host(value[@"scope"])){value[@"revoked"]=@YES;value[@"cancelling"]=@YES;nw_connection_group_cancel((nw_connection_group_t)object);}}}
 [gateLock unlock];
}
uint32_t network_framework_gate_active(void){uint32_t (*swiftOutstanding)(void)=dlsym(RTLD_DEFAULT,"loader_swift_network_outstanding");uint32_t swiftActive=swiftOutstanding?swiftOutstanding():0;initialize();[gateLock lock];uint64_t active=(uint64_t)hostCallbacks+swiftActive;for(id object in connections.keyEnumerator.allObjects){NSDictionary *value=[connections objectForKey:object];if(host(value[@"scope"]) && ([value[@"active"] boolValue] || [value[@"cancelling"] boolValue]))active++;}for(id object in groups.keyEnumerator.allObjects){NSDictionary *value=[groups objectForKey:object];if(host(value[@"scope"])&&([value[@"active"] boolValue]||[value[@"cancelling"] boolValue]))active++;}[gateLock unlock];return (uint32_t)MIN(active,UINT32_MAX);}
uint32_t network_framework_gate_pending_callbacks(void){initialize();[gateLock lock];uint64_t value=hostCallbacks;[gateLock unlock];return (uint32_t)MIN(value,UINT32_MAX);}
void network_framework_gate_statistics(uint64_t *starts,uint64_t *sends){initialize();[gateLock lock];if(starts)*starts=deniedStarts;if(sends)*sends=deniedSends;[gateLock unlock];}
// Preserve nw_connection_create's +1 ownership convention across interposition.
static nw_connection_t gated_create(nw_endpoint_t,nw_parameters_t) __attribute__((ns_returns_retained));
static nw_connection_t gated_create(nw_endpoint_t endpoint,nw_parameters_t parameters){
 nw_connection_t connection=nw_connection_create(endpoint,parameters);if(!connection || frameworkCaller(__builtin_return_address(0)))return connection;initialize();[gateLock lock];NSMutableDictionary *value=entry(connection,YES);
 // Local-domain endpoints carry in-process/IPC traffic and are not Internet paths.
 if(nw_endpoint_get_type(endpoint)==nw_endpoint_type_address){const struct sockaddr *address=nw_endpoint_get_address(endpoint);if(address && address->sa_family==AF_UNIX){value[@"scope"]=@(SOCKET_SCOPE_CONTROL);value[@"revoked"]=@NO;}}
 [gateLock unlock];gated_handler(connection,nil);return connection;
}
static void gated_start(nw_connection_t connection){initialize();[gateLock lock];if(permit(connection,YES,frameworkCaller(__builtin_return_address(0))))nw_connection_start(connection);else nw_connection_force_cancel(connection);[gateLock unlock];}
static void gated_restart(nw_connection_t connection){initialize();[gateLock lock];if(permit(connection,YES,frameworkCaller(__builtin_return_address(0))))nw_connection_restart(connection);else nw_connection_force_cancel(connection);[gateLock unlock];}
static void gated_send(nw_connection_t connection,dispatch_data_t content,nw_content_context_t context,bool complete,nw_connection_send_completion_t completion){
 initialize();[gateLock lock];BOOL allowed=permit(connection,NO,frameworkCaller(__builtin_return_address(0)));
 NSMutableDictionary *value=entry(connection,NO);BOOL track=completion && value && host(value[@"scope"]);
 nw_connection_send_completion_t wrapped=completion;
 if(track){hostCallbacks++;wrapped=^(nw_error_t error){@try {completion(error);}@finally {[gateLock lock];hostCallbacks--;[gateLock unlock];}};}
 if(allowed)nw_connection_send(connection,content,context,complete,wrapped);
 else {nw_connection_force_cancel(connection); // Native scheduling/error, without caller bytes.
  nw_connection_send(connection,NULL,context,complete,wrapped);}
 [gateLock unlock];
}
static nw_connection_receive_completion_t receiveCompletion(nw_connection_t connection,nw_connection_receive_completion_t completion){
 NSMutableDictionary *value=entry(connection,NO);BOOL track=completion && value && host(value[@"scope"]);
 if(!track)return completion;
 hostCallbacks++;
 return ^(dispatch_data_t content,nw_content_context_t context,bool complete,nw_error_t error){
  @try {completion(content,context,complete,error);}
  @finally {[gateLock lock];hostCallbacks--;[gateLock unlock];}
 };
}
static void gated_receive(nw_connection_t connection,uint32_t minimum,uint32_t maximum,nw_connection_receive_completion_t completion){
 initialize();[gateLock lock];BOOL allowed=permit(connection,NO,frameworkCaller(__builtin_return_address(0)));
 nw_connection_receive_completion_t wrapped=receiveCompletion(connection,completion);
 if(!allowed)nw_connection_force_cancel(connection);
 nw_connection_receive(connection,minimum,maximum,wrapped);[gateLock unlock];
}
static void gated_receive_message(nw_connection_t connection,nw_connection_receive_completion_t completion){
 initialize();[gateLock lock];BOOL allowed=permit(connection,NO,frameworkCaller(__builtin_return_address(0)));
 nw_connection_receive_completion_t wrapped=receiveCompletion(connection,completion);
 if(!allowed)nw_connection_force_cancel(connection);
 nw_connection_receive_message(connection,wrapped);[gateLock unlock];
}

static void markCancelling(nw_connection_t connection){[gateLock lock];NSMutableDictionary *value=entry(connection,NO);if(value)value[@"cancelling"]=@YES;[gateLock unlock];}
static void gated_cancel(nw_connection_t connection){initialize();markCancelling(connection);nw_connection_cancel(connection);}
static void gated_force_cancel(nw_connection_t connection){initialize();markCancelling(connection);nw_connection_force_cancel(connection);}
static void gated_handler(nw_connection_t connection,nw_connection_state_changed_handler_t handler){
 initialize();[gateLock lock];BOOL tracked=entry(connection,NO)!=nil;[gateLock unlock];if(!tracked && frameworkCaller(__builtin_return_address(0))){nw_connection_set_state_changed_handler(connection,handler);return;}
 // Capture a weak connection to avoid the connection -> handler -> connection cycle.
 __weak id weakConnection=(id)connection;
 nw_connection_set_state_changed_handler(connection,^(nw_connection_state_t state,nw_error_t error){
  id object=weakConnection;BOOL counted=NO;
  initialize();[gateLock lock];NSMutableDictionary *value=object?entry((nw_connection_t)object,NO):nil;
  if(handler && value && host(value[@"scope"])){hostCallbacks++;counted=YES;}
  if(object && (state==nw_connection_state_cancelled || state==nw_connection_state_failed)){
   if(value){value[@"active"]=@NO;value[@"cancelling"]=@NO;value[@"revoked"]=@YES;value[@"terminal"]=@YES;[passiveConnections setObject:value forKey:object];}[connections removeObjectForKey:object];
  }
  [gateLock unlock];
  @try {if(handler)handler(state,error);}
  @finally {if(counted){[gateLock lock];hostCallbacks--;[gateLock unlock];}}
 });
}
// Connection groups use a separate native lifecycle and can produce connections
// that never passed nw_connection_create in application code.
static NSMutableDictionary *groupEntry(nw_connection_group_t group,BOOL create){
 id object=(id)group;NSMutableDictionary *value=[groups objectForKey:object]?:[passiveGroups objectForKey:object];
 if(!value&&create&&groups.count<4096){value=[@{@"scope":@(socket_gate_current_scope()),@"active":@NO,@"revoked":@(denied&&host(@(socket_gate_current_scope())))} mutableCopy];value[@"identity"]=@(++groupSequence);[passiveGroups setObject:value forKey:object];}return value;
}
static BOOL groupPermit(nw_connection_group_t group,BOOL start,BOOL managed){
 if(managed&&!groupEntry(group,NO))return YES;NSMutableDictionary *value=groupEntry(group,YES);if(value&&[value[@"terminal"] boolValue]&&!host(value[@"scope"]))return YES;BOOL reject=!value||(host(value[@"scope"])&&(denied||[value[@"revoked"] boolValue]));
 if(reject){if(start)deniedStarts++;else deniedSends++;if(value){value[@"revoked"]=@YES;if(![value[@"terminal"] boolValue]){value[@"cancelling"]=@YES;[groups setObject:value forKey:(id)group];[passiveGroups removeObjectForKey:(id)group];}}}
 else{value[@"active"]=@YES;[groups setObject:value forKey:(id)group];[passiveGroups removeObjectForKey:(id)group];}return !reject;
}
static void adoptGroupConnection(nw_connection_t connection,NSDictionary *group){
 if(!connection||!group)return;NSMutableDictionary *previous=entry(connection,NO);BOOL previouslyRevoked=previous&&host(previous[@"scope"])&&[previous[@"revoked"] boolValue];NSMutableDictionary *value=entry(connection,YES);if(!value){nw_connection_force_cancel(connection);return;}value[@"scope"]=group[@"scope"];value[@"groupIdentity"]=group[@"identity"];value[@"revoked"]=@(previouslyRevoked||[group[@"revoked"] boolValue]||(denied&&host(value[@"scope"])));if([value[@"revoked"] boolValue])value[@"cancelling"]=@YES;
 gated_handler(connection,nil);if([value[@"revoked"] boolValue]){if(![value[@"terminal"] boolValue]){[connections setObject:value forKey:(id)connection];[passiveConnections removeObjectForKey:(id)connection];}nw_connection_force_cancel(connection);}
}
static nw_connection_group_t gated_group_create(nw_group_descriptor_t,nw_parameters_t) __attribute__((ns_returns_retained));
static nw_connection_group_t gated_group_create(nw_group_descriptor_t descriptor,nw_parameters_t parameters){
 GROUP_CALL("create");nw_connection_group_t group=nw_connection_group_create(descriptor,parameters);if(!group||frameworkCaller(__builtin_return_address(0)))return group;initialize();[gateLock lock];NSMutableDictionary *value=groupEntry(group,YES);
 __block BOOL local=YES,found=NO;nw_group_descriptor_enumerate_endpoints(descriptor,^bool(nw_endpoint_t endpoint){found=YES;const struct sockaddr *address=nw_endpoint_get_type(endpoint)==nw_endpoint_type_address?nw_endpoint_get_address(endpoint):NULL;if(!address||address->sa_family!=AF_UNIX)local=NO;return true;});if(found&&local){value[@"scope"]=@(SOCKET_SCOPE_CONTROL);value[@"revoked"]=@NO;}[gateLock unlock];gated_group_handler(group,nil);return group;
}
static void gated_group_handler(nw_connection_group_t group,nw_connection_group_state_changed_handler_t handler){
 initialize();[gateLock lock];BOOL tracked=groupEntry(group,NO)!=nil;[gateLock unlock];if(!tracked&&frameworkCaller(__builtin_return_address(0))){nw_connection_group_set_state_changed_handler(group,handler);return;}__weak id weak=(id)group;
 nw_connection_group_set_state_changed_handler(group,^(nw_connection_group_state_t state,nw_error_t error){id object=weak;BOOL counted=NO;[gateLock lock];NSMutableDictionary *value=object?groupEntry((nw_connection_group_t)object,NO):nil;if(handler&&value&&host(value[@"scope"])){hostCallbacks++;counted=YES;}
 if(state==nw_connection_group_state_cancelled&&object){if(value){value[@"active"]=@NO;value[@"cancelling"]=@NO;value[@"revoked"]=@YES;value[@"terminal"]=@YES;[passiveGroups setObject:value forKey:object];}[groups removeObjectForKey:object];}
 if(state==nw_connection_group_state_failed&&value){value[@"cancelling"]=@YES;nw_connection_group_cancel((nw_connection_group_t)object);}
 [gateLock unlock];@try{if(handler)handler(state,error);}@finally{if(counted){[gateLock lock];hostCallbacks--;[gateLock unlock];}}
 });
}
static void gated_group_start(nw_connection_group_t group){GROUP_CALL("start");initialize();[gateLock lock];if(groupPermit(group,YES,frameworkCaller(__builtin_return_address(0))))nw_connection_group_start(group);else nw_connection_group_cancel(group);[gateLock unlock];}
static void gated_group_cancel(nw_connection_group_t group){initialize();[gateLock lock];NSMutableDictionary *value=groupEntry(group,NO);if(value)value[@"cancelling"]=@YES;nw_connection_group_cancel(group);[gateLock unlock];}
// The group SDK does not complete sends submitted after cancellation. Keep the
// caller's queue contract and use a native cancellation error produced by an
// unstarted, cancelled control connection. No caller payload or network start
// is used to manufacture the error.
static nw_error_t groupRejectionError;
static nw_connection_t groupErrorSource;
static NSMutableArray *groupErrorWaiters;
static void rejectGroupSend(dispatch_queue_t queue,nw_connection_group_send_completion_t completion,BOOL track){
 if(!completion)return;if(track)hostCallbacks++;
 void (^deliver)(nw_error_t)=^(nw_error_t error){dispatch_async(queue,^{@try{completion(error);}@finally{if(track){[gateLock lock];hostCallbacks--;[gateLock unlock];}}});};
 if(groupRejectionError){deliver(groupRejectionError);return;}
 if(!groupErrorWaiters)groupErrorWaiters=[NSMutableArray array];[groupErrorWaiters addObject:[deliver copy]];if(groupErrorSource)return;
 dispatch_queue_t control=dispatch_queue_create("loader.group-native-error",DISPATCH_QUEUE_SERIAL);
 nw_parameters_t parameters=nw_parameters_create_secure_tcp(NW_PARAMETERS_DISABLE_PROTOCOL,NW_PARAMETERS_DEFAULT_CONFIGURATION);
 groupErrorSource=nw_connection_create(nw_endpoint_create_host("127.0.0.1","0"),parameters);nw_connection_set_queue(groupErrorSource,control);nw_connection_force_cancel(groupErrorSource);
 nw_connection_send(groupErrorSource,NULL,NW_CONNECTION_DEFAULT_MESSAGE_CONTEXT,true,^(nw_error_t error){[gateLock lock];if(error){groupRejectionError=error;NSArray *waiting=[groupErrorWaiters copy];[groupErrorWaiters removeAllObjects];for(id item in waiting)((void(^)(nw_error_t))item)(error);}else{fprintf(stderr,"NW_GROUP native-cancellation-error-unavailable\n");}groupErrorSource=nil;[gateLock unlock];});
}
static void gated_group_queue(nw_connection_group_t group,dispatch_queue_t queue){GROUP_CALL("queue");initialize();[gateLock lock];if(!frameworkCaller(__builtin_return_address(0))||groupEntry(group,NO)){NSMutableDictionary *value=groupEntry(group,YES);if(queue)value[@"queue"]=(id)queue;else [value removeObjectForKey:@"queue"];}nw_connection_group_set_queue(group,queue);[gateLock unlock];}
static void gated_group_send(nw_connection_group_t group,dispatch_data_t content,nw_endpoint_t endpoint,nw_content_context_t context,nw_connection_group_send_completion_t completion){
 GROUP_CALL("send");initialize();[gateLock lock];BOOL allowed=groupPermit(group,NO,frameworkCaller(__builtin_return_address(0)));NSDictionary *value=groupEntry(group,NO);BOOL track=completion&&value&&host(value[@"scope"]);
 if(!allowed){nw_connection_group_cancel(group);dispatch_queue_t queue=value[@"queue"]?:dispatch_get_main_queue();rejectGroupSend(queue,completion,track);[gateLock unlock];return;}
 nw_connection_group_send_completion_t wrapped=completion;if(track){hostCallbacks++;wrapped=^(nw_error_t error){@try{completion(error);}@finally{[gateLock lock];hostCallbacks--;[gateLock unlock];}};}
 nw_connection_group_send_message(group,content,endpoint,context,wrapped);[gateLock unlock];
}
static void gated_group_reply(nw_connection_group_t group,nw_content_context_t inbound,nw_content_context_t outbound,dispatch_data_t content){initialize();[gateLock lock];if(groupPermit(group,NO,frameworkCaller(__builtin_return_address(0))))nw_connection_group_reply(group,inbound,outbound,content);else nw_connection_group_cancel(group);[gateLock unlock];}
static void gated_group_receive_handler(nw_connection_group_t group,uint32_t maximum,bool reject,nw_connection_group_receive_handler_t handler){
 initialize();[gateLock lock];BOOL tracked=groupEntry(group,NO)!=nil;[gateLock unlock];if(!handler||(!tracked&&frameworkCaller(__builtin_return_address(0)))){nw_connection_group_set_receive_handler(group,maximum,reject,handler);return;}__weak id weak=(id)group;
 nw_connection_group_set_receive_handler(group,maximum,reject,^(dispatch_data_t content,nw_content_context_t context,bool complete){id object=weak;[gateLock lock];NSDictionary *value=object?groupEntry((nw_connection_group_t)object,NO):nil;BOOL track=value&&host(value[@"scope"]);if(track)hostCallbacks++;[gateLock unlock];@try{handler(content,context,complete);}@finally{if(track){[gateLock lock];hostCallbacks--;[gateLock unlock];}}});
}
static nw_connection_t gated_group_extract(nw_connection_group_t,nw_endpoint_t,nw_protocol_options_t) __attribute__((ns_returns_retained));
static nw_connection_t gated_group_extract(nw_connection_group_t group,nw_endpoint_t endpoint,nw_protocol_options_t options){initialize();[gateLock lock];if(!groupPermit(group,NO,frameworkCaller(__builtin_return_address(0)))){nw_connection_group_cancel(group);[gateLock unlock];return nil;}nw_connection_t result=nw_connection_group_extract_connection(group,endpoint,options);adoptGroupConnection(result,groupEntry(group,NO));[gateLock unlock];return result;}
static nw_connection_t gated_group_extract_message(nw_connection_group_t,nw_content_context_t) __attribute__((ns_returns_retained));
static nw_connection_t gated_group_extract_message(nw_connection_group_t group,nw_content_context_t message){initialize();[gateLock lock];if(!groupPermit(group,NO,frameworkCaller(__builtin_return_address(0)))){nw_connection_group_cancel(group);[gateLock unlock];return nil;}nw_connection_t result=nw_connection_group_extract_connection_for_message(group,message);adoptGroupConnection(result,groupEntry(group,NO));[gateLock unlock];return result;}
static bool gated_group_reinsert(nw_connection_group_t group,nw_connection_t connection){initialize();[gateLock lock];NSDictionary *parent=groupEntry(group,NO);NSMutableDictionary *child=entry(connection,NO);BOOL related=parent&&child&&[parent[@"identity"] isEqual:child[@"groupIdentity"]];
 if(!groupPermit(group,NO,frameworkCaller(__builtin_return_address(0)))){nw_connection_group_cancel(group);if(related){child[@"revoked"]=@YES;if(![child[@"terminal"] boolValue]){child[@"cancelling"]=@YES;[connections setObject:child forKey:(id)connection];[passiveConnections removeObjectForKey:(id)connection];}nw_connection_force_cancel(connection);}[gateLock unlock];return false;}
 if(related&&host(child[@"scope"])&&[child[@"revoked"] boolValue]){[gateLock unlock];return false;}
 bool result=nw_connection_group_reinsert_extracted_connection(group,connection);if(result&&related){child[@"active"]=@NO;child[@"cancelling"]=@NO;[passiveConnections setObject:child forKey:(id)connection];[connections removeObjectForKey:(id)connection];}[gateLock unlock];return result;
}
static void gated_group_new_connection_handler(nw_connection_group_t group,nw_connection_group_new_connection_handler_t handler){initialize();[gateLock lock];BOOL tracked=groupEntry(group,NO)!=nil;[gateLock unlock];if(!handler||(!tracked&&frameworkCaller(__builtin_return_address(0)))){nw_connection_group_set_new_connection_handler(group,handler);return;}__weak id weak=(id)group;
 nw_connection_group_set_new_connection_handler(group,^(nw_connection_t connection){id object=weak;[gateLock lock];NSDictionary *value=object?groupEntry((nw_connection_group_t)object,NO):nil;adoptGroupConnection(connection,value);BOOL track=value&&host(value[@"scope"]);if(track)hostCallbacks++;[gateLock unlock];@try{handler(connection);}@finally{if(track){[gateLock lock];hostCallbacks--;[gateLock unlock];}}});
}
#define INTERPOSE(replacement,original) __attribute__((used)) static struct{const void *newFunction;const void *oldFunction;} interpose_##original __attribute__((section("__DATA,__interpose")))={(const void*)replacement,(const void*)original}
INTERPOSE(gated_create,nw_connection_create);
INTERPOSE(gated_start,nw_connection_start);
INTERPOSE(gated_restart,nw_connection_restart);
INTERPOSE(gated_send,nw_connection_send);
INTERPOSE(gated_cancel,nw_connection_cancel);
INTERPOSE(gated_force_cancel,nw_connection_force_cancel);
INTERPOSE(gated_handler,nw_connection_set_state_changed_handler);

INTERPOSE(gated_receive,nw_connection_receive);
INTERPOSE(gated_receive_message,nw_connection_receive_message);

INTERPOSE(gated_group_create,nw_connection_group_create);
INTERPOSE(gated_group_handler,nw_connection_group_set_state_changed_handler);
INTERPOSE(gated_group_start,nw_connection_group_start);
INTERPOSE(gated_group_cancel,nw_connection_group_cancel);
INTERPOSE(gated_group_send,nw_connection_group_send_message);
INTERPOSE(gated_group_reply,nw_connection_group_reply);
INTERPOSE(gated_group_receive_handler,nw_connection_group_set_receive_handler);
INTERPOSE(gated_group_extract,nw_connection_group_extract_connection);
INTERPOSE(gated_group_extract_message,nw_connection_group_extract_connection_for_message);
INTERPOSE(gated_group_reinsert,nw_connection_group_reinsert_extracted_connection);
INTERPOSE(gated_group_new_connection_handler,nw_connection_group_set_new_connection_handler);

INTERPOSE(gated_group_queue,nw_connection_group_set_queue);
