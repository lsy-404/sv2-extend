// Independent executable: URLSession starts before its main, not in a plugin.
#import <Foundation/Foundation.h>
#include "loader_api.h"
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdatomic.h>
static NSDictionary *report;
__attribute__((constructor))static void beforeMain(void){@autoreleasepool{
 int(*state)(loader_network_state_v1*)=dlsym(RTLD_DEFAULT,"network_gate_state");
 uint32_t(*pending)(void)=dlsym(RTLD_DEFAULT,"loader_urlsession_callbacks_pending");
 if(!state||!pending)_exit(90);
 loader_network_state_v1 initial={.abi_version=1,.struct_size=sizeof initial};int initialCode=state(&initial);
 dispatch_semaphore_t entered=dispatch_semaphore_create(0),release=dispatch_semaphore_create(0),finished=dispatch_semaphore_create(0);
 __block unsigned replies=0;__block uint32_t pendingDuring=0;__block int status=0;__block BOOL correctBody=NO;
 NSURLSession *session=[NSURLSession sessionWithConfiguration:NSURLSessionConfiguration.ephemeralSessionConfiguration];
 NSURL *url=[NSURL URLWithString:[NSString stringWithFormat:@"http://127.0.0.1:%s/premain",getenv("EXTEND_PREMAIN_PORT")]];
 NSURLSessionDataTask *task=[session dataTaskWithURL:url completionHandler:^(NSData *body,NSURLResponse *response,NSError *error){
  replies++;pendingDuring=pending();status=error?(int)error.code:(int)[(NSHTTPURLResponse*)response statusCode];correctBody=[body isEqual:[@"premain-response" dataUsingEncoding:NSUTF8StringEncoding]];
  dispatch_semaphore_signal(entered);dispatch_semaphore_wait(release,dispatch_time(DISPATCH_TIME_NOW,5*NSEC_PER_SEC));dispatch_semaphore_signal(finished);
 }];[task resume];
 loader_network_state_v1 live={.abi_version=1,.struct_size=sizeof live};int liveCode=state(&live);
 BOOL gotCallback=dispatch_semaphore_wait(entered,dispatch_time(DISPATCH_TIME_NOW,5*NSEC_PER_SEC))==0;
 loader_network_state_v1 held={.abi_version=1,.struct_size=sizeof held};int heldCode=state(&held);uint32_t heldPending=pending();
 dispatch_semaphore_signal(release);BOOL returned=dispatch_semaphore_wait(finished,dispatch_time(DISPATCH_TIME_NOW,2*NSEC_PER_SEC))==0;
 for(unsigned i=0;i<100&&pending();i++)usleep(10000);
 report=@{@"initialCode":@(initialCode),@"liveCode":@(liveCode),@"liveTasks":@(live.tracked_host_tasks_active),@"heldCode":@(heldCode),@"heldDrained":@(held.tracked_tasks_drained),@"pendingDuring":@(pendingDuring),@"heldPending":@(heldPending),@"pendingAfter":@(pending()),@"gotCallback":@(gotCallback),@"returned":@(returned),@"replies":@(replies),@"status":@(status),@"correctBody":@(correctBody)};
 [session finishTasksAndInvalidate];
}}
int main(void){@autoreleasepool{NSData *data=[NSJSONSerialization dataWithJSONObject:report options:0 error:NULL];printf("PREMAIN_URLSESSION %s\n",[[NSString alloc]initWithData:data encoding:NSUTF8StringEncoding].UTF8String);fflush(stdout);}return 0;}
