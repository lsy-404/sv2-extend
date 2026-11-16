#import <Foundation/Foundation.h>
#include "plugin_manager.h"
#include "startup_policy.h"
#include <stdio.h>
#include <unistd.h>
static NSDictionary *perform(NSString *method,NSDictionary *args){NSDictionary *reply;if([method isEqual:@"apply"]){loader_apply_pending_plugin_updates();reply=@{@"ok":@YES};}else reply=loader_plugin_manager_call(method,args);NSTimeInterval deadline=NSProcessInfo.processInfo.systemUptime+25;while([loader_plugin_manager_snapshot()[@"busy"] boolValue]&&NSProcessInfo.processInfo.systemUptime<deadline)[NSRunLoop.mainRunLoop runUntilDate:[NSDate dateWithTimeIntervalSinceNow:.03]];if([loader_plugin_manager_snapshot()[@"busy"] boolValue])exit(3);return @{@"reply":reply,@"snapshot":loader_plugin_manager_snapshot()};}
int main(int argc,const char **argv){@autoreleasepool{if(argc<3)return 64;setenv("OPEN_SV2_LOADER_DATA_DIR",argv[1],1);if(loader_profile_lease_bootstrap()!=LOADER_OK)return 73;NSString *method=@(argv[2]);id args=argc>3?[NSJSONSerialization JSONObjectWithData:[[@(argv[3]) dataUsingEncoding:NSUTF8StringEncoding] copy] options:0 error:NULL]:@{};id result;
 if([method isEqual:@"sequence"]){NSMutableArray *steps=[NSMutableArray array];for(NSDictionary *step in args)[steps addObject:perform(step[@"method"],step[@"arguments"]?:@{})];result=@{@"steps":steps,@"snapshot":loader_plugin_manager_snapshot()};}else result=perform(method,args);
 NSData *d=[NSJSONSerialization dataWithJSONObject:result options:NSJSONWritingPrettyPrinted error:NULL];puts([[NSString alloc]initWithData:d encoding:NSUTF8StringEncoding].UTF8String);return 0;}}
