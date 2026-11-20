#import <Foundation/Foundation.h>
#include <dlfcn.h>
#include <unistd.h>
#include "loader_api.h"

static int owner;
static const char plugin_id[]="test.sidebar-icon";

int loader_plugin_init_v1(const loader_api_v1 *api){
 if(!api||!api->register_plugin_ui||!api->update_plugin_ui)return LOADER_ERR_INVALID;
 const char *descriptor="{\"id\":\"test.sidebar-icon\",\"name\":\"Sidebar Icon Test\",\"version\":\"1\",\"status\":\"White icon\",\"icon\":\"<svg viewBox='0 0 24 24'><circle cx='12' cy='12' r='8' fill='white'/></svg>\",\"sidebar\":{\"title\":\"Sidebar Icon Test\",\"rows\":[]},\"settings\":[]}";
 int registered=api->register_plugin_ui(descriptor,NULL,&owner);
 if(registered!=LOADER_OK)return registered;
 int (*open_panel)(const char *)=dlsym(RTLD_DEFAULT,"loader_extend_open_plugin_panel");
 const char *data=getenv("OPEN_SV2_LOADER_DATA_DIR");
 if(!open_panel||!data)return LOADER_ERR_INVALID;
 NSString *path=[@(data) stringByAppendingPathComponent:@"icon-visual-receipt.json"];
 __block unsigned attempts=0;
 NSTimer *timer=[NSTimer timerWithTimeInterval:1 repeats:YES block:^(NSTimer *running){
  const int opened=open_panel(plugin_id);
  if(opened!=0&&++attempts<8)return;
  [running invalidate];
  const char *update="{\"activeIcon\":\"<svg viewBox='0 0 24 24'><path d='M12 2L22 12L12 22L2 12Z' fill='#e65f2a'/></svg>\",\"status\":\"Active orange diamond\"}";
  const int updated=api->update_plugin_ui(plugin_id,update,&owner);
  NSData *receipt=[NSJSONSerialization dataWithJSONObject:@{ @"pid":@(getpid()),@"opened":@(opened==0),@"updated":@(updated==LOADER_OK) } options:0 error:NULL];
  [receipt writeToFile:path atomically:YES];
 }];
 [NSRunLoop.mainRunLoop addTimer:timer forMode:NSRunLoopCommonModes];
 return LOADER_OK;
}
