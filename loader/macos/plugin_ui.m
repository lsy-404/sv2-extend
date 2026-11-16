// Resident plugin presentation registry. Disk is used only for explicit preferences,
// never as script request/reply transport. Account actions remain plugin-owned.
#import <Foundation/Foundation.h>
#include "loader_api.h"
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <math.h>
#include <dlfcn.h>
@interface ExtendUIPlugin:NSObject
@property NSDictionary *descriptor;
@property loader_plugin_ui_event event;
@property void *owner;
@end
@implementation ExtendUIPlugin
@end
static NSMutableDictionary<NSString*,ExtendUIPlugin*> *plugins;
static NSMutableDictionary *preferences;
static NSString *preferenceFile,*persistenceError;
static uint64_t generation=1;
static NSDictionary *fail(NSString *code){return @{@"ok":@NO,@"error":@{@"code":code}};}
static NSDictionary *success(id value){return @{@"ok":@YES,@"result":value?:@{}};}
static BOOL boolean(id v){return [v isKindOfClass:NSNumber.class]&&CFGetTypeID((__bridge CFTypeRef)v)==CFBooleanGetTypeID();}
static BOOL text(id v,NSUInteger maximum){return [v isKindOfClass:NSString.class]&&[v length]<=maximum;}
static BOOL name(id v){return text(v,96)&&[v rangeOfString:@"^[A-Za-z][A-Za-z0-9_.-]*$" options:NSRegularExpressionSearch].location!=NSNotFound;}
static id decode(const char *raw){if(!raw || strnlen(raw,65537)>65536)return nil;NSString *s=[NSString stringWithUTF8String:raw];return s?[NSJSONSerialization JSONObjectWithData:[s dataUsingEncoding:NSUTF8StringEncoding] options:0 error:NULL]:nil;}
static BOOL fieldValue(NSDictionary *field,id value){
 NSString *kind=field[@"type"];
 if([kind isEqual:@"boolean"])return boolean(value);
 if([kind isEqual:@"text"])return text(value,1024);
 if([kind isEqual:@"choice"])return text(value,256)&&[field[@"choices"] containsObject:value];
 if([kind isEqual:@"integer"])return [value isKindOfClass:NSNumber.class]&&!boolean(value)&&isfinite([value doubleValue])&&[value doubleValue]==floor([value doubleValue])&&[value doubleValue]>=[field[@"minimum"] doubleValue]&&[value doubleValue]<=[field[@"maximum"] doubleValue];
 return NO;
}
static BOOL sidebar(id v){
 if(![v isKindOfClass:NSDictionary.class]||!text(v[@"title"],128)||![v[@"rows"] isKindOfClass:NSArray.class]||[v[@"rows"] count]>32)return NO;
 NSMutableSet *actions=[NSMutableSet new];
 for(id row in v[@"rows"]){if(![row isKindOfClass:NSDictionary.class]||!text(row[@"text"],512))return NO;
  if([row[@"type"] isEqual:@"label"])continue;
  if(![row[@"type"] isEqual:@"button"]||!name(row[@"id"])||[actions containsObject:row[@"id"]])return NO;[actions addObject:row[@"id"]];
 }return YES;
}
static BOOL descriptor(id v){
 if(![v isKindOfClass:NSDictionary.class]||!name(v[@"id"])||!text(v[@"name"],128)||![v[@"name"] length]||!text(v[@"version"],64)||!text(v[@"status"],128)||!sidebar(v[@"sidebar"])||![v[@"settings"] isKindOfClass:NSArray.class]||[v[@"settings"] count]>64)return NO;
 NSMutableSet *keys=[NSMutableSet new];
 for(id f in v[@"settings"]){if(![f isKindOfClass:NSDictionary.class]||!name(f[@"key"])||[keys containsObject:f[@"key"]]||!text(f[@"title"],128))return NO;[keys addObject:f[@"key"]];
  if([f[@"type"] isEqual:@"choice"]){if(![f[@"choices"] isKindOfClass:NSArray.class]||![f[@"choices"] count]||[f[@"choices"] count]>32)return NO;for(id choice in f[@"choices"])if(!text(choice,256))return NO;}
  if([f[@"type"] isEqual:@"integer"]){for(NSString *key in @[@"minimum",@"maximum"])if(![f[key] isKindOfClass:NSNumber.class]||boolean(f[key])||!isfinite([f[key] doubleValue])||[f[key] doubleValue]!=floor([f[key] doubleValue]))return NO;if([f[@"minimum"] doubleValue]>[f[@"maximum"] doubleValue])return NO;}
  if(!fieldValue(f,f[@"default"]))return NO;
 }return YES;
}
int loader_plugin_ui_install(void){
 if(!NSThread.isMainThread)return LOADER_ERR_INVALID;if(plugins)return LOADER_OK;
 plugins=[NSMutableDictionary new];preferences=[NSMutableDictionary new];
 const char *raw=getenv("OPEN_SV2_LOADER_DATA_DIR");NSString *root=raw?@(raw):nil;
 if(!root.isAbsolutePath||![root isEqual:root.stringByResolvingSymlinksInPath]){persistenceError=@"UI_STORAGE_NOT_CONFIGURED";return LOADER_OK;}
 NSString *directory=[root stringByAppendingPathComponent:@"preferences"];struct stat st;
 if((mkdir(directory.fileSystemRepresentation,0700)&&errno!=EEXIST)||lstat(directory.fileSystemRepresentation,&st)||!S_ISDIR(st.st_mode)||st.st_uid!=getuid()||(st.st_mode&0077)){persistenceError=@"UI_STORAGE_UNAVAILABLE";return LOADER_OK;}
 preferenceFile=[directory stringByAppendingPathComponent:@"extend-ui.json"];
 if(lstat(preferenceFile.fileSystemRepresentation,&st)==0){
  if(!S_ISREG(st.st_mode)||st.st_uid!=getuid()||st.st_size>262144){persistenceError=@"UI_PREFERENCES_INVALID";return LOADER_OK;}
  NSData *data=[NSData dataWithContentsOfFile:preferenceFile];id value=data?[NSJSONSerialization JSONObjectWithData:data options:0 error:NULL]:nil;
  if(![value isKindOfClass:NSDictionary.class]||![value[@"schema"] isEqual:@1]||boolean(value[@"schema"])||![value[@"plugins"] isKindOfClass:NSDictionary.class]||[value[@"plugins"] count]>128){persistenceError=@"UI_PREFERENCES_INVALID";return LOADER_OK;}
  for(id key in value[@"plugins"]){id item=value[@"plugins"][key];if(!name(key)||![item isKindOfClass:NSDictionary.class]||!boolean(item[@"visible"])||![item[@"settings"] isKindOfClass:NSDictionary.class]){persistenceError=@"UI_PREFERENCES_INVALID";return LOADER_OK;}}
  preferences=[value[@"plugins"] mutableCopy];
 }else if(errno!=ENOENT)persistenceError=@"UI_STORAGE_UNAVAILABLE";
 return LOADER_OK;
}
static NSDictionary *saved(NSString *identifier){return preferences[identifier];}
static NSDictionary *values(ExtendUIPlugin *plugin){NSMutableDictionary *result=[NSMutableDictionary new];NSDictionary *stored=saved(plugin.descriptor[@"id"])[@"settings"];
 for(NSDictionary *field in plugin.descriptor[@"settings"]){id value=stored[field[@"key"]];
 if(!fieldValue(field,value)&&[@[@"opensv2.lua-extend",@"opensv2.javascript-extend"] containsObject:plugin.descriptor[@"id"]]&&![field[@"key"] hasSuffix:@".status"]&&![field[@"key"] hasSuffix:@".cancel"]&&field[@"legacyGroup"]&&boolean(stored[field[@"legacyGroup"]]))value=stored[field[@"legacyGroup"]];
 result[field[@"key"]]=fieldValue(field,value)?value:field[@"default"];}return result;}
static void notify(ExtendUIPlugin *plugin,NSDictionary *event){if(!plugin.event)return;NSData *data=[NSJSONSerialization dataWithJSONObject:event options:0 error:NULL];NSString *body=[[NSString alloc]initWithData:data encoding:NSUTF8StringEncoding];@try{plugin.event(body.UTF8String,plugin.owner);}@catch(NSException *e){(void)e;}}
int loader_register_plugin_ui(const char *json,loader_plugin_ui_event event,void *owner){
 if(!NSThread.isMainThread)return LOADER_ERR_INVALID;loader_plugin_ui_install();id value=decode(json);if(!descriptor(value)||plugins[value[@"id"]]||plugins.count>=32)return LOADER_ERR_INVALID;
 ExtendUIPlugin *plugin=[ExtendUIPlugin new];plugin.descriptor=value;plugin.event=event;plugin.owner=owner;plugins[value[@"id"]]=plugin;generation++;
 notify(plugin,@{@"kind":@"settings",@"settings":values(plugin)});return LOADER_OK;
}
int loader_unregister_plugin_ui(const char *raw,void *owner){if(!NSThread.isMainThread||!raw)return LOADER_ERR_INVALID;NSString *identifier=[NSString stringWithUTF8String:raw];ExtendUIPlugin *plugin=plugins[identifier];if(!plugin||plugin.owner!=owner)return LOADER_ERR_INVALID;[plugins removeObjectForKey:identifier];generation++;return LOADER_OK;}
int loader_update_plugin_ui(const char *raw,const char *json,void *owner){if(!NSThread.isMainThread||!raw)return LOADER_ERR_INVALID;NSString *identifier=[NSString stringWithUTF8String:raw];ExtendUIPlugin *plugin=plugins[identifier];id state=decode(json);if(!plugin||plugin.owner!=owner||![state isKindOfClass:NSDictionary.class])return LOADER_ERR_INVALID;
 for(id key in state)if(![@[@"status",@"sidebar"] containsObject:key])return LOADER_ERR_INVALID;
 NSMutableDictionary *next=[plugin.descriptor mutableCopy];[next addEntriesFromDictionary:state];if(!descriptor(next))return LOADER_ERR_INVALID;if(![plugin.descriptor isEqual:next]){plugin.descriptor=next;generation++;}return LOADER_OK;}
static NSDictionary *snapshot(void){NSDictionary *(*manager)(void)=dlsym(RTLD_DEFAULT,"loader_plugin_manager_snapshot");NSDictionary *managerState=manager?manager():@{};NSMutableArray *items=[NSMutableArray new];for(NSString *key in [plugins.allKeys sortedArrayUsingSelector:@selector(compare:)]){ExtendUIPlugin *p=plugins[key];NSMutableDictionary *item=[p.descriptor mutableCopy];item[@"visible"]=saved(key)?saved(key)[@"visible"]:@YES;item[@"values"]=values(p);[items addObject:item];}
 return @{@"generation":@(generation),@"manager":managerState,@"plugins":items,@"canPersist":@(!persistenceError),@"persistenceError":persistenceError?:@"",@"hostPid":@(getpid()),@"extendEntryVisible":saved(@"opensv2.extend")?saved(@"opensv2.extend")[@"visible"]:@YES};}
static BOOL persist(NSMutableDictionary *next){if(persistenceError||!preferenceFile)return NO;
 NSData *data=[NSJSONSerialization dataWithJSONObject:@{@"schema":@1,@"plugins":next} options:NSJSONWritingSortedKeys error:NULL];if(!data||data.length>262144)return NO;
 if(![data writeToFile:preferenceFile options:NSDataWritingAtomic error:NULL])return NO;chmod(preferenceFile.fileSystemRepresentation,0600);preferences=next;return YES;}
NSDictionary *loader_plugin_ui_call(NSString *method,NSDictionary *args){
 if(!NSThread.isMainThread)return fail(@"MAIN_THREAD_REQUIRED");loader_plugin_ui_install();if(![args isKindOfClass:NSDictionary.class])return fail(@"INVALID_ARGUMENT");
 if([method isEqual:@"ui.snapshot"]){if(args.count)return fail(@"INVALID_ARGUMENT");return success(snapshot());}
 if(![args[@"generation"] isKindOfClass:NSNumber.class]||boolean(args[@"generation"])||[args[@"generation"] unsignedLongLongValue]!=generation||[args[@"generation"] doubleValue]!=(double)generation)return fail(@"STALE_UI_GENERATION");
 if([method isEqual:@"ui.set_extend_visible"]){if(args.count!=2||!boolean(args[@"visible"]))return fail(@"INVALID_ARGUMENT");NSMutableDictionary *next=[preferences mutableCopy];next[@"opensv2.extend"]=@{@"visible":args[@"visible"],@"settings":@{}};if(!persist(next))return fail(@"UI_PREFERENCES_WRITE_FAILED");generation++;return success(snapshot());}
 NSString *identifier=args[@"id"];ExtendUIPlugin *plugin=[identifier isKindOfClass:NSString.class]?plugins[identifier]:nil;if(!plugin)return fail(@"PLUGIN_UI_NOT_FOUND");
 if([method isEqual:@"ui.action"]){if(args.count!=3||!name(args[@"action"]))return fail(@"INVALID_ARGUMENT");BOOL found=NO;for(NSDictionary *row in plugin.descriptor[@"sidebar"][@"rows"])if([row[@"type"] isEqual:@"button"]&&[row[@"id"] isEqual:args[@"action"]])found=YES;if(!found||!plugin.event)return fail(@"UI_ACTION_UNAVAILABLE");notify(plugin,@{@"kind":@"action",@"action":args[@"action"]});return success(snapshot());}
 NSMutableDictionary *next=[preferences mutableCopy];NSMutableDictionary *record=[(saved(identifier)?:@{@"visible":@YES,@"settings":@{}}) mutableCopy];
 if([method isEqual:@"ui.set_visible"]){if(args.count!=3||!boolean(args[@"visible"]))return fail(@"INVALID_ARGUMENT");record[@"visible"]=args[@"visible"];}
 else if([method isEqual:@"ui.set_setting"]){if(args.count!=4||!name(args[@"key"]))return fail(@"INVALID_ARGUMENT");NSDictionary *field=nil;for(NSDictionary *f in plugin.descriptor[@"settings"])if([f[@"key"] isEqual:args[@"key"]])field=f;if(!field||!fieldValue(field,args[@"value"]))return fail(@"INVALID_SETTING_VALUE");NSMutableDictionary *settings=[record[@"settings"] mutableCopy];settings[args[@"key"]]=args[@"value"];record[@"settings"]=[settings copy];}
 else if([method isEqual:@"ui.set_settings"]){if(args.count!=3||![args[@"settings"] isKindOfClass:NSDictionary.class])return fail(@"INVALID_ARGUMENT");NSDictionary *requested=args[@"settings"];if(requested.count!=[plugin.descriptor[@"settings"] count])return fail(@"INVALID_SETTING_VALUE");for(NSDictionary *field in plugin.descriptor[@"settings"])if(!fieldValue(field,requested[field[@"key"]]))return fail(@"INVALID_SETTING_VALUE");record[@"settings"]=[requested copy];}
 else return fail(@"UNSUPPORTED_UI_METHOD");
 next[identifier]=record;if(!persist(next))return fail(@"UI_PREFERENCES_WRITE_FAILED");generation++;
 if([method isEqual:@"ui.set_setting"]||[method isEqual:@"ui.set_settings"])notify(plugin,@{@"kind":@"settings",@"settings":values(plugin)});
 return success(snapshot());
}
static BOOL luaRuntimeReady,jsRuntimeReady;
static void runtimeChanged(const char *body,void *owner){
 id event=decode(body);if(![event[@"kind"] isEqual:@"settings"])return;
 BOOL details=[event[@"settings"][@"showDetails"] boolValue];NSString *identifier=@((const char*)owner);
 BOOL active=[event[@"settings"][@"enabled"] boolValue];
 BOOL ready=[identifier containsString:@"javascript"]?jsRuntimeReady:luaRuntimeReady;
 NSMutableArray *rows=[NSMutableArray arrayWithObject:@{@"type":@"label",@"text":!active?@"扩展功能已停用":(ready?@"脚本扩展功能可用":@"当前宿主的原生接口不可用")}];
 if(details)[rows addObject:@{@"type":@"label",@"text":@"在插件设置中可单独开关各个函数。"}];
 NSString *title=[identifier containsString:@"javascript"]?@"JS Extend":@"Lua Extend";
 NSData *data=[NSJSONSerialization dataWithJSONObject:@{@"status":!active?@"已停用":ready?@"已启用":@"当前宿主不可用",@"sidebar":@{@"title":title,@"rows":rows}} options:0 error:NULL];NSString *text=[[NSString alloc]initWithData:data encoding:NSUTF8StringEncoding];loader_update_plugin_ui((const char*)owner,text.UTF8String,owner);
}
void loader_plugin_ui_register_runtimes(int luaReady,int jsReady){
 luaRuntimeReady=luaReady;jsRuntimeReady=jsReady;
 for(NSString *kind in @[@"lua",@"javascript"]){BOOL lua=[kind isEqual:@"lua"];NSString *identifier=lua?@"opensv2.lua-extend":@"opensv2.javascript-extend";NSString *title=lua?@"Lua Extend":@"JS Extend";BOOL ready=lua?luaReady:jsReady;
  const char *context=lua?"opensv2.lua-extend":"opensv2.javascript-extend";
  NSMutableArray *fields=[NSMutableArray arrayWithObjects:@{@"key":@"enabled",@"title":lua?@"启用 Lua Extend":@"启用 JS Extend",@"type":@"boolean",@"default":@YES},nil];
  NSArray *functions=@[
   @[@"voicebanks.current",@"读取当前声库",@"声库",@"voicebanks"],@[@"voicebanks.list",@"读取声库列表",@"声库",@"voicebanks"],@[@"voicebanks.select",@"切换声库",@"声库",@"voicebanks"],
   @[@"exports.snapshot",@"读取导出状态",@"音频导出",@"exports"],@[@"exports.start",@"开始导出",@"音频导出",@"exports"],@[@"exports.status",@"查询导出任务",@"音频导出",@"exports"],@[@"exports.cancel",@"取消导出任务",@"音频导出",@"exports"],
   @[@"selection.read",@"读取工程与选区",@"工程与音高",@"editor"],@[@"notes.set_pitch",@"修改音符音高",@"工程与音高",@"editor"],@[@"pitch.read_range",@"读取音高曲线",@"工程与音高",@"editor"],@[@"pitch.replace_range",@"修改音高曲线",@"工程与音高",@"editor"],
   @[@"transport.status",@"读取播放状态",@"播放",@"editor"],@[@"transport.play",@"开始播放",@"播放",@"editor"],@[@"transport.stop",@"停止播放",@"播放",@"editor"],
   @[@"http.start",@"发起本地或在线请求",@"HTTP 请求",@"http"],@[@"http.status",@"查询请求结果",@"HTTP 请求",@"http"],@[@"http.cancel",@"取消请求",@"HTTP 请求",@"http"]];
  for(NSArray *field in functions)[fields addObject:@{@"key":field[0],@"title":field[1],@"detail":field[0],@"section":field[2],@"legacyGroup":field[3],@"type":@"boolean",@"default":@YES}];
  [fields addObject:@{@"key":@"showDetails",@"title":@"显示运行详情",@"type":@"boolean",@"default":@NO}];
  NSDictionary *d=@{@"id":identifier,@"name":title,@"version":@"1.1",@"status":ready?@"已就绪":@"当前宿主不可用",@"sidebar":@{@"title":title,@"rows":@[@{@"type":@"label",@"text":ready?@"原生扩展接口可用":@"原生扩展接口不可用"}]},@"settings":fields};
  NSData *data=[NSJSONSerialization dataWithJSONObject:d options:0 error:NULL];NSString *text=[[NSString alloc]initWithData:data encoding:NSUTF8StringEncoding];loader_register_plugin_ui(text.UTF8String,runtimeChanged,(void*)context);
 }
}

// Main-thread policy lookup used by resident script entry points. Feature defaults
// remain enabled for existing profiles, including pre-settings registry fixtures.
int loader_plugin_ui_feature_allowed(const char *raw,const char *feature){
 if(!NSThread.isMainThread||!raw||!feature)return 0;
 loader_plugin_ui_install();ExtendUIPlugin *plugin=plugins[@(raw)];if(!plugin)return 1;
 id value=values(plugin)[@(feature)];return value?[value boolValue]:1;
}
