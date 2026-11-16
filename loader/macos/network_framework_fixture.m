#import <Foundation/Foundation.h>
#import <Network/Network.h>
#include <dlfcn.h>
#include <unistd.h>
#include <stdatomic.h>
#include "loader_api.h"
static const loader_api_v1 *loader;
static void hostPolicy(int value){loader->set_host_network_blocked(value);}
static void noSocketPolicy(int value){(void)value;}
static void pluginHTTP(int32_t status,int32_t error,const uint8_t *bytes,size_t length,void *context){(void)bytes;(void)length;atomic_store((_Atomic int*)context,status==200 && !error?1:-1);}
static dispatch_queue_t queue;
static void (*policy)(int),(*sockets)(int);
static uint32_t (*active)(void),(*pendingCallbacks)(void);
static _Atomic int stateCallbackTracked,sendCallbackTracked;
static void (*statistics)(uint64_t*,uint64_t*);
static nw_connection_t make(const char *port,BOOL udp,dispatch_semaphore_t ready,dispatch_semaphore_t cancelled){
 nw_endpoint_t endpoint=nw_endpoint_create_host("127.0.0.1",port);
 nw_parameters_t parameters=udp?nw_parameters_create_secure_udp(NW_PARAMETERS_DISABLE_PROTOCOL,NW_PARAMETERS_DEFAULT_CONFIGURATION):nw_parameters_create_secure_tcp(NW_PARAMETERS_DISABLE_PROTOCOL,NW_PARAMETERS_DEFAULT_CONFIGURATION);
 nw_connection_t connection=nw_connection_create(endpoint,parameters);nw_connection_set_queue(connection,queue);
 nw_connection_set_state_changed_handler(connection,^(nw_connection_state_t state,nw_error_t error){fprintf(stderr,"NW_FIXTURE state=%u error=%d\n",state,error?nw_error_get_error_code(error):0);if(state==nw_connection_state_cancelled && pendingCallbacks && pendingCallbacks()>0 && active()>0)atomic_store(&stateCallbackTracked,1);if(state==nw_connection_state_ready)dispatch_semaphore_signal(ready);if(state==nw_connection_state_cancelled || state==nw_connection_state_failed)dispatch_semaphore_signal(cancelled);});return connection;
}
static BOOL waitSignal(dispatch_semaphore_t sem){return dispatch_semaphore_wait(sem,dispatch_time(DISPATCH_TIME_NOW,3000000000))==0;}
static BOOL sendText(nw_connection_t connection,const char *text,BOOL expectedError){
 dispatch_semaphore_t sem=dispatch_semaphore_create(0);__block BOOL matched=NO;dispatch_data_t data=dispatch_data_create(text,strlen(text),queue,DISPATCH_DATA_DESTRUCTOR_DEFAULT);
 nw_connection_send(connection,data,NW_CONNECTION_DEFAULT_MESSAGE_CONTEXT,true,^(nw_error_t error){if(pendingCallbacks && pendingCallbacks()>0)atomic_store(&sendCallbackTracked,1);matched=(error!=nil)==expectedError;dispatch_semaphore_signal(sem);});return waitSignal(sem)&&matched;
}
static BOOL receiveText(nw_connection_t connection,BOOL message,const char *expected){
 dispatch_semaphore_t done=dispatch_semaphore_create(0);__block BOOL good=NO;
 nw_connection_receive_completion_t received=^(dispatch_data_t content,nw_content_context_t context,bool complete,nw_error_t error){
  (void)context;NSMutableData *bytes=[NSMutableData new];
  if(content)dispatch_data_apply(content,^bool(dispatch_data_t region,size_t offset,const void *buffer,size_t length){(void)region;(void)offset;[bytes appendBytes:buffer length:length];return true;});
  good=!error && (!message || complete) && bytes.length==strlen(expected) && !memcmp(bytes.bytes,expected,bytes.length) && pendingCallbacks()>0;
  dispatch_semaphore_signal(done);
 };
 if(message)nw_connection_receive_message(connection,received);else nw_connection_receive(connection,(uint32_t)strlen(expected),4096,received);
 return waitSignal(done)&&good;
}
static int runFixture(int argc,const char **argv){@autoreleasepool{
 if(argc!=3)return 64;policy=hostPolicy;sockets=noSocketPolicy;active=dlsym(RTLD_DEFAULT,"network_framework_gate_active");statistics=dlsym(RTLD_DEFAULT,"network_framework_gate_statistics");if(!policy||!sockets||!active||!statistics)return 65;
 pendingCallbacks=dlsym(RTLD_DEFAULT,"network_framework_gate_pending_callbacks");if(!pendingCallbacks)return 66;
 queue=dispatch_queue_create("network-framework-fixture",DISPATCH_QUEUE_SERIAL);NSMutableDictionary *checks=[NSMutableDictionary dictionary];
 dispatch_semaphore_t dormantReady=dispatch_semaphore_create(0),dormantCancelled=dispatch_semaphore_create(0);nw_connection_t dormant=make(argv[1],NO,dormantReady,dormantCancelled);
 dispatch_semaphore_t ready=dispatch_semaphore_create(0),cancelled=dispatch_semaphore_create(0);nw_connection_t live=make(argv[1],NO,ready,cancelled);nw_connection_start(live);checks[@"tcpAllowed"]=@(waitSignal(ready)&&sendText(live,"before-block",NO));checks[@"activeTracked"]=@(active()>0);
 dispatch_semaphore_t receiveDone=dispatch_semaphore_create(0);__block BOOL receiveError=NO,receiveTracked=NO;
 nw_connection_receive(live,1,4096,^(dispatch_data_t content,nw_content_context_t context,bool complete,nw_error_t error){(void)content;(void)context;(void)complete;receiveError=error!=nil;receiveTracked=pendingCallbacks()>0 && active()>0;dispatch_semaphore_signal(receiveDone);});
 checks[@"receiveCountedBeforeCancellation"]=@(pendingCallbacks()>0);
 sockets(1);policy(1);_Atomic int *http=calloc(1,sizeof(*http));loader->plugin_http_get(getenv("LOADER_NW_HTTP_URL"),pluginHTTP,http);for(unsigned attempt=0;attempt<100 && !atomic_load(http);attempt++)usleep(20000);checks[@"pluginHTTPWhileBlocked"]=@(atomic_load(http)==1);
 loader_network_state_v1 state={.abi_version=LOADER_ABI_V1,.struct_size=sizeof(state)};loader->get_host_network_state(&state);checks[@"frameworkCoverageReported"]=@((state.intercepted_paths&LOADER_NET_NETWORK_FRAMEWORK)!=0);checks[@"wholeHostStillUnverified"]=@(!state.whole_host_verified && loader->verify_host_network_quiescence(state.generation,&state)==LOADER_ERR_NETWORK_UNVERIFIED);
 checks[@"receiveCancelledWithNativeError"]=@(waitSignal(receiveDone)&&receiveError);checks[@"receiveCallbackCountedUntilReturn"]=@(receiveTracked);
 checks[@"liveCancellationAcknowledged"]=@(waitSignal(cancelled));checks[@"revokedSendError"]=@(sendText(live,"forbidden-live",YES));
 ready=dispatch_semaphore_create(0);cancelled=dispatch_semaphore_create(0);nw_connection_t blocked=make(argv[1],NO,ready,cancelled);nw_connection_start(blocked);checks[@"tcpBlockedCancelled"]=@(waitSignal(cancelled));checks[@"blockedSendCompletion"]=@(sendText(blocked,"forbidden-new",YES));
 dispatch_semaphore_t duringReady=dispatch_semaphore_create(0),duringCancelled=dispatch_semaphore_create(0);nw_connection_t duringBlock=make(argv[1],NO,duringReady,duringCancelled);
 sockets(0);policy(0);dispatch_suspend(queue);nw_connection_start(dormant);checks[@"unstartedCancellationQueuedTracked"]=@(active()>0);dispatch_resume(queue);checks[@"unstartedConnectionRevokedAfterAllow"]=@(waitSignal(dormantCancelled));checks[@"unstartedSendRejectedAfterAllow"]=@(sendText(dormant,"forbidden-dormant",YES));nw_connection_start(duringBlock);checks[@"createdDuringBlockRevokedAfterAllow"]=@(waitSignal(duringCancelled));checks[@"createdDuringBlockSendRejectedAfterAllow"]=@(sendText(duringBlock,"forbidden-during-block",YES));ready=dispatch_semaphore_create(0);cancelled=dispatch_semaphore_create(0);nw_connection_t restored=make(argv[1],NO,ready,cancelled);nw_connection_start(restored);checks[@"tcpRestored"]=@(waitSignal(ready)&&sendText(restored,"restored",NO));checks[@"receivePayloadPreserved"]=@(receiveText(restored,NO,"ack-restored"));nw_connection_cancel(restored);
 ready=dispatch_semaphore_create(0);cancelled=dispatch_semaphore_create(0);nw_connection_t udp=make(argv[2],YES,ready,cancelled);nw_connection_start(udp);checks[@"udpAllowed"]=@(waitSignal(ready)&&sendText(udp,"udp-allowed",NO));dispatch_semaphore_t messageDone=dispatch_semaphore_create(0);__block BOOL messageError=NO,messageTracked=NO;
 nw_connection_receive_message(udp,^(dispatch_data_t content,nw_content_context_t context,bool complete,nw_error_t error){(void)content;(void)context;(void)complete;messageError=error!=nil;messageTracked=pendingCallbacks()>0 && active()>0;dispatch_semaphore_signal(messageDone);});
 checks[@"receiveMessageCountedBeforeCancellation"]=@(pendingCallbacks()>0);
 sockets(1);policy(1);checks[@"udpLiveTerminated"]=@(waitSignal(cancelled));checks[@"receiveMessageCancelledWithNativeError"]=@(waitSignal(messageDone)&&messageError);checks[@"receiveMessageCallbackCountedUntilReturn"]=@(messageTracked);checks[@"udpRevokedSendError"]=@(sendText(udp,"udp-forbidden-live",YES));
 ready=dispatch_semaphore_create(0);cancelled=dispatch_semaphore_create(0);nw_connection_t udpBlocked=make(argv[2],YES,ready,cancelled);nw_connection_start(udpBlocked);checks[@"udpBlockedCancelled"]=@(waitSignal(cancelled));checks[@"udpBlockedSendError"]=@(sendText(udpBlocked,"udp-forbidden-new",YES));
 policy(0);ready=dispatch_semaphore_create(0);cancelled=dispatch_semaphore_create(0);nw_connection_t udpEcho=make(argv[2],YES,ready,cancelled);nw_connection_start(udpEcho);
 checks[@"receiveMessagePayloadPreserved"]=@(waitSignal(ready)&&sendText(udpEcho,"udp-echo",NO)&&receiveText(udpEcho,YES,"ack-udp"));nw_connection_cancel(udpEcho);waitSignal(cancelled);policy(1);
 usleep(200000);checks[@"stateCallbackCountedUntilReturn"]=@(atomic_load(&stateCallbackTracked)==1);checks[@"sendCallbackCountedUntilReturn"]=@(atomic_load(&sendCallbackTracked)==1);checks[@"callbacksDrainedAfterReturn"]=@(pendingCallbacks()==0);checks[@"trackedConnectionsDrained"]=@(active()==0);uint64_t starts=0,sends=0;statistics(&starts,&sends);checks[@"denyCounters"]=@(starts>=2&&sends>=4);
 NSData *json=[NSJSONSerialization dataWithJSONObject:@{@"checks":checks,@"deniedStarts":@(starts),@"deniedSends":@(sends)} options:0 error:NULL];puts([[NSString alloc]initWithData:json encoding:NSUTF8StringEncoding].UTF8String);for(NSNumber *value in checks.allValues)if(!value.boolValue)return 1;
}return 0;}

int loader_plugin_init_v1(const loader_api_v1 *api){if(!api || api->abi_version!=LOADER_ABI_V1)return 1;loader=api;dispatch_async(dispatch_get_global_queue(QOS_CLASS_UTILITY,0),^{const char *args[]={"fixture",getenv("LOADER_NW_TCP_PORT"),getenv("LOADER_NW_UDP_PORT")};int result=runFixture(3,args);fprintf(stderr,"NW_HOST_FIXTURE_DONE result=%d pid=%d\n",result,getpid());fflush(stdout);fflush(stderr);});return 0;}
