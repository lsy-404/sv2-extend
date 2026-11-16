#import <Foundation/Foundation.h>
#include "startup_policy.h"
#include "startup_policy_internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
static NSDictionary *listed;
static void complete(const char *body,size_t length,void *context){(void)context;listed=[NSJSONSerialization JSONObjectWithData:[NSData dataWithBytes:body length:length] options:0 error:NULL];}
static void output(id value){NSData *d=[NSJSONSerialization dataWithJSONObject:value options:NSJSONWritingSortedKeys error:NULL];puts([[NSString alloc]initWithData:d encoding:NSUTF8StringEncoding].UTF8String);fflush(stdout);}
int main(int argc,const char **argv){@autoreleasepool{if(argc!=2)return 64;setenv("OPEN_SV2_LOADER_DATA_DIR",argv[1],1);int bootstrap=loader_profile_lease_bootstrap();output(@{@"bootstrap":@(bootstrap),@"pid":@(getpid())});if(bootstrap!=0)return 73;char *line=NULL;size_t capacity=0;
 while(getline(&line,&capacity,stdin)>0){@autoreleasepool{id command=[NSJSONSerialization JSONObjectWithData:[[@(line) dataUsingEncoding:NSUTF8StringEncoding] copy] options:0 error:NULL];NSString *method=command[@"op"];if([method isEqual:@"quit"])break;const char *owner=[command[@"owner"] UTF8String]?:"test.owner",*operation=[command[@"id"] UTF8String]?:"operation";int code=0;
 if([method isEqual:@"acquire"])code=loader_acquire_profile_exclusive(owner,operation);
 else if([method isEqual:@"release"])code=loader_release_profile_exclusive(owner,operation);
 else if([method isEqual:@"guard"])code=loader_set_startup_network_guard(owner,operation,[command[@"enabled"] boolValue]);
 else if([method isEqual:@"owner-count"])code=loader_startup_owner_guard_count(owner);
 else if([method isEqual:@"register-bootstrap"])loader_startup_register_bootstrap_owner(owner);
 else if([method isEqual:@"required-bootstrap"]){NSArray *required=loader_startup_required_bootstrap_owners();listed=required?@{@"owners":required}:@{@"invalid":@YES};}
 else if([method isEqual:@"close-other-fd"]){int fd=open([[NSString stringWithUTF8String:argv[1]] stringByAppendingPathComponent:@".profile-host.lock"].fileSystemRepresentation,O_RDONLY);if(fd>=0)close(fd);}
 else if([method isEqual:@"list"]){listed=nil;loader_list_startup_network_guards(owner,complete,NULL);}
 loader_profile_lease_state_v1 lease={.abi_version=LOADER_ABI_V1,.struct_size=sizeof lease};int leaseCode=loader_get_profile_lease_state(&lease);loader_startup_network_state_v1 policy={.abi_version=LOADER_ABI_V1,.struct_size=sizeof policy};loader_get_startup_network_policy(&policy);
 output(@{@"code":@(code),@"leaseCode":@(leaseCode),@"leaseMode":@(lease.mode),@"leaseGeneration":@(lease.generation),@"leasePid":@(lease.host_pid),@"blocked":@(policy.requested_blocked),@"guards":@(policy.active_guards),@"policyValid":@(policy.policy_valid),@"policyGeneration":@(policy.generation),@"listed":listed?:@{}});
 }}free(line);return 0;}}
