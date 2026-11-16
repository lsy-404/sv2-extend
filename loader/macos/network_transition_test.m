#import <Foundation/Foundation.h>
#include "loader_api.h"
#include <stdatomic.h>
extern int network_gate_install(void),network_gate_set(int),network_gate_get(void);
extern int network_gate_state(loader_network_state_v1*),network_gate_verify_quiescence(uint64_t,loader_network_state_v1*);
static dispatch_semaphore_t entered,releaseFirst;
static atomic_int sockets,network,webPolicy;
static atomic_bool armed;
static int reentrantSet,reentrantSnapshot;
int loader_urlsession_callbacks_install(void){return 0;}
void loader_urlsession_callback_resumed(NSURLSessionTask *task){(void)task;}
uint32_t loader_urlsession_callbacks_pending(void){return 0;}
void socket_gate_set_blocked(int value){atomic_store(&sockets,value!=0);}
uint32_t socket_gate_pending_calls(void){return 0;}
int socket_gate_discovery_incomplete(void){return 0;}
uint32_t socket_gate_live_host_connections(void){return 0;}
uint32_t network_framework_gate_active(void){return 0;}
int webkit_gate_install(void){return 0;}
void webkit_gate_update_policy(void){atomic_store(&webPolicy,atomic_load(&sockets));}
uint32_t webkit_gate_pending_updates(void){return 0;}
uint32_t webkit_gate_active_relays(void){return 0;}
uint32_t loader_web_retirement_pending(void){return 0;}
void network_framework_gate_set_blocked(int value){
 if(value&&atomic_exchange(&armed,false)){
  reentrantSet=network_gate_set(0);loader_network_state_v1 state={.abi_version=1,.struct_size=sizeof state};reentrantSnapshot=network_gate_state(&state);
  dispatch_semaphore_signal(entered);dispatch_semaphore_wait(releaseFirst,DISPATCH_TIME_FOREVER);
 }
 atomic_store(&network,value!=0);
}
int main(int argc,const char **argv){@autoreleasepool{
 if(argc!=2)return 64;NSMutableDictionary *checks=[NSMutableDictionary dictionary];checks[@"installed"]=@(network_gate_install()==0);
 entered=dispatch_semaphore_create(0);releaseFirst=dispatch_semaphore_create(0);atomic_store(&armed,true);
 dispatch_semaphore_t firstDone=dispatch_semaphore_create(0),allowEntered=dispatch_semaphore_create(0),allowDone=dispatch_semaphore_create(0),stateEntered=dispatch_semaphore_create(0),stateDone=dispatch_semaphore_create(0);
 __block int first=-99,second=-99,stateCode=-99;
 dispatch_async(dispatch_get_global_queue(0,0),^{first=network_gate_set(1);dispatch_semaphore_signal(firstDone);});
 if(dispatch_semaphore_wait(entered,dispatch_time(DISPATCH_TIME_NOW,2*NSEC_PER_SEC)))return 2;
 dispatch_async(dispatch_get_global_queue(0,0),^{dispatch_semaphore_signal(allowEntered);second=network_gate_set(0);dispatch_semaphore_signal(allowDone);});
 dispatch_async(dispatch_get_global_queue(0,0),^{dispatch_semaphore_signal(stateEntered);loader_network_state_v1 state={.abi_version=1,.struct_size=sizeof state};stateCode=network_gate_state(&state);dispatch_semaphore_signal(stateDone);});
 dispatch_semaphore_wait(allowEntered,DISPATCH_TIME_FOREVER);dispatch_semaphore_wait(stateEntered,DISPATCH_TIME_FOREVER);
 checks[@"allowWaitsForAllSubgates"]=@(dispatch_semaphore_wait(allowDone,dispatch_time(DISPATCH_TIME_NOW,100*NSEC_PER_MSEC))!=0&&atomic_load(&sockets)==1);
 checks[@"snapshotWaitsForCommittedTransition"]=@(dispatch_semaphore_wait(stateDone,dispatch_time(DISPATCH_TIME_NOW,100*NSEC_PER_MSEC))!=0);
 checks[@"reentrantMutationRefused"]=@(reentrantSet==LOADER_ERR_INVALID);checks[@"reentrantPartialSnapshotRefused"]=@(reentrantSnapshot==LOADER_ERR_INVALID);
 dispatch_semaphore_signal(releaseFirst);
 BOOL finished=dispatch_semaphore_wait(firstDone,dispatch_time(DISPATCH_TIME_NOW,2*NSEC_PER_SEC))==0&&dispatch_semaphore_wait(allowDone,dispatch_time(DISPATCH_TIME_NOW,2*NSEC_PER_SEC))==0&&dispatch_semaphore_wait(stateDone,dispatch_time(DISPATCH_TIME_NOW,2*NSEC_PER_SEC))==0;
 checks[@"bothTransitionsAndSnapshotComplete"]=@(finished&&first==0&&second==0&&stateCode==0);
 checks[@"finalPolicyCoherentAcrossSubgates"]=@(network_gate_get()==0&&atomic_load(&sockets)==0&&atomic_load(&network)==0&&atomic_load(&webPolicy)==0);
 loader_network_state_v1 state={.abi_version=1,.struct_size=sizeof state};checks[@"generationCountsBothTransitions"]=@(network_gate_state(&state)==0&&state.generation==2);
 checks[@"wholeHostReceiptStillUnverified"]=@(network_gate_verify_quiescence(state.generation,&state)==LOADER_ERR_NETWORK_UNVERIFIED&&!state.whole_host_verified);
 BOOL passed=YES;for(NSNumber *value in checks.allValues)passed&=value.boolValue;NSDictionary *report=@{@"checks":checks,@"passed":@(passed),@"scope":@"Actual network_gate policy coordinator and Foundation task interception with controlled subgate barriers. Tests transition ordering, not transport coverage."};NSData *data=[NSJSONSerialization dataWithJSONObject:report options:NSJSONWritingPrettyPrinted error:NULL];[data writeToFile:@(argv[1]) atomically:YES];puts([[NSString alloc]initWithData:data encoding:NSUTF8StringEncoding].UTF8String);return passed?0:1;
}}
