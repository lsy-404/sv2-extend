// Diagnostic only. Never included in the release application.
#import <Foundation/Foundation.h>
#import <AppKit/AppKit.h>
#include "loader_api.h"
#include <dlfcn.h>
static const loader_api_v1 *loader;static int owner,extraOwner,settingsEvents,actionEvents;
static void event(const char *body,void *context){
 if(context!=&owner&&context!=&extraOwner)abort();id value=[NSJSONSerialization JSONObjectWithData:[@(body)dataUsingEncoding:NSUTF8StringEncoding] options:0 error:NULL];
 if([value[@"kind"]isEqual:@"settings"])settingsEvents++;
 if([value[@"kind"]isEqual:@"action"]){actionEvents++;
  if(context==&extraOwner){loader->unregister_plugin_ui("test.extra-panel",&extraOwner);return;}
  if([value[@"action"]isEqual:@"add"]){const char *extra="{\"id\":\"test.extra-panel\",\"name\":\"Extra Panel\",\"version\":\"1\",\"status\":\"动态注册成功\",\"sidebar\":{\"title\":\"Extra Panel\",\"rows\":[{\"type\":\"label\",\"text\":\"由插件包在运行中注册的独立面板\"},{\"type\":\"button\",\"id\":\"remove\",\"text\":\"移除此动态面板\"}]},\"settings\":[]}";loader->register_plugin_ui(extra,event,&extraOwner);return;}

  if([value[@"action"]isEqual:@"remove"]){loader->unregister_plugin_ui("test.sidebar",&owner);return;}
  NSString *status=[NSString stringWithFormat:@"动作 %d · 设置 %d",actionEvents,settingsEvents];NSData *data=[NSJSONSerialization dataWithJSONObject:@{@"status":status} options:0 error:NULL];NSString *text=[[NSString alloc]initWithData:data encoding:NSUTF8StringEncoding];loader->update_plugin_ui("test.sidebar",text.UTF8String,&owner);
 }
}
int loader_plugin_init_v1(const loader_api_v1 *api){
 if(api->struct_size<offsetof(loader_api_v1,update_plugin_ui)+sizeof api->update_plugin_ui)return LOADER_ERR_INVALID;loader=api;
 const char *descriptor="{\"id\":\"test.sidebar\",\"name\":\"Sidebar Test\",\"version\":\"1\",\"status\":\"测试已注册\",\"sidebar\":{\"title\":\"Sidebar Test\",\"rows\":[{\"type\":\"label\",\"text\":\"驻留原生测试插件\"},{\"type\":\"button\",\"id\":\"refresh\",\"text\":\"刷新测试状态\"},{\"type\":\"button\",\"id\":\"remove\",\"text\":\"移除测试插件\"},{\"type\":\"button\",\"id\":\"add\",\"text\":\"新增测试面板\"}]},\"settings\":[{\"key\":\"enabled\",\"title\":\"测试布尔设置\",\"type\":\"boolean\",\"default\":true},{\"key\":\"count\",\"title\":\"测试数量\",\"type\":\"integer\",\"minimum\":0,\"maximum\":5,\"default\":1},{\"key\":\"choice\",\"title\":\"测试选项\",\"type\":\"choice\",\"choices\":[\"one\",\"two\"],\"default\":\"one\"},{\"key\":\"title\",\"title\":\"测试文本\",\"type\":\"text\",\"default\":\"default\"}]}";
 int result=loader->register_plugin_ui(descriptor,event,&owner);
 NSDictionary *(*call)(NSString*,NSDictionary*)=dlsym(RTLD_DEFAULT,"loader_plugin_ui_call");const char *path=getenv("LOADER_UI_TEST_SNAPSHOT");
 if(call&&path){NSString *file=@(path);NSTimer *timer=[NSTimer timerWithTimeInterval:.5 repeats:YES block:^(NSTimer *t){(void)t;NSMutableArray *windows=[NSMutableArray array];for(NSWindow *w in NSApp.windows)[windows addObject:@{@"title":w.title?:@"",@"visible":@(w.isVisible),@"frame":NSStringFromRect(w.frame),@"class":NSStringFromClass(w.class)}];NSDictionary *value=@{@"windows":windows,@"pid":@(getpid()),@"snapshot":call(@"ui.snapshot",@{}),@"settingsEvents":@(settingsEvents),@"actionEvents":@(actionEvents)};NSData *data=[NSJSONSerialization dataWithJSONObject:value options:0 error:NULL];[data writeToFile:file atomically:YES];}];[NSRunLoop.mainRunLoop addTimer:timer forMode:NSRunLoopCommonModes];}
 return result;
}
