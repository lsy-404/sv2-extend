// Profile-local native packages. Both entry phases execute inside the host.
#import <Foundation/Foundation.h>
#include "loader_api.h"
#include "../../sdk/include/loader_bootstrap_api.h"
#include "../../sdk/include/extend_platform.h"
#include "startup_policy_internal.h"
#include <dlfcn.h>
#include <sys/stat.h>
#include <limits.h>
#include <unistd.h>
extern NSDictionary *loader_plugin_ui_call(NSString*,NSDictionary*);
static NSArray<NSDictionary*> *packages;
static NSMutableDictionary<NSString*,NSValue*> *modules;
static BOOL bootstrapAttempted,normalAttempted;
static int bootstrapResult=LOADER_ERR_INVALID;
static BOOL identifier(id value){return [value isKindOfClass:NSString.class]&&[value length]<=96&&[value rangeOfString:@"^[A-Za-z][A-Za-z0-9_.-]*$" options:NSRegularExpressionSearch].location!=NSNotFound;}
static BOOL ordinary(NSString *path,BOOL directory){struct stat info;return lstat(path.fileSystemRepresentation,&info)==0&&info.st_uid==getuid()&&!(info.st_mode&0022)&&(directory?S_ISDIR(info.st_mode):(S_ISREG(info.st_mode)&&info.st_nlink==1));}
static NSArray *scan(void){
 const char *raw=getenv("OPEN_SV2_LOADER_DATA_DIR");if(!raw)return @[];
 NSString *folder=[@(raw) stringByAppendingPathComponent:@"plugins"];
 if(![NSFileManager.defaultManager fileExistsAtPath:folder])return @[];
 if(!ordinary(folder,YES)||![folder isEqual:folder.stringByResolvingSymlinksInPath])return nil;
 NSArray *names=[[NSFileManager.defaultManager contentsOfDirectoryAtPath:folder error:NULL] sortedArrayUsingSelector:@selector(compare:)];
 NSMutableArray *result=[NSMutableArray array];NSMutableSet *ids=[NSMutableSet set];
 for(NSString *name in names){
  NSString *package=[folder stringByAppendingPathComponent:name],*manifest=[package stringByAppendingPathComponent:@"plugin.json"];
  if(!ordinary(package,YES)||!ordinary(manifest,NO))continue;
  NSData *data=[NSData dataWithContentsOfFile:manifest];if(!data||data.length>65536)continue;
  id definition=[NSJSONSerialization JSONObjectWithData:data options:0 error:NULL];
  if(![definition isKindOfClass:NSDictionary.class]||![definition[@"schema"] isEqual:@1]||CFGetTypeID((__bridge CFTypeRef)definition[@"schema"])==CFBooleanGetTypeID()||!identifier(definition[@"id"]))continue;
  if(definition[@"enabled"]&&CFGetTypeID((__bridge CFTypeRef)definition[@"enabled"])!=CFBooleanGetTypeID())continue;
  if(definition[@"enabled"]&&![definition[@"enabled"] boolValue])continue;
  if([ids containsObject:definition[@"id"]])return nil;
  id libraries=definition[@"nativeLibraries"];NSString *library=libraries?([libraries isKindOfClass:NSDictionary.class]?libraries[@(loader_platform_key())]:nil):definition[@"nativeLibrary"],*title=definition[@"name"],*version=definition[@"version"];
  if(![library isKindOfClass:NSString.class]||library.isAbsolutePath||library.length>512||![library.pathExtension isEqual:@"dylib"]||![title isKindOfClass:NSString.class]||!title.length||title.length>128||![version isKindOfClass:NSString.class]||version.length>64)continue;
  NSString *path=[package stringByAppendingPathComponent:library];char actual[PATH_MAX],base[PATH_MAX];
  if(!ordinary(path,NO)||!realpath(path.fileSystemRepresentation,actual)||!realpath(package.fileSystemRepresentation,base))continue;
  NSString *canonical=@(actual),*prefix=[@(base) stringByAppendingString:@"/"];
  if(![canonical hasPrefix:prefix])continue;
  if(result.count>=32)return nil;
  [ids addObject:definition[@"id"]];[result addObject:@{@"definition":definition,@"path":canonical}];
 }
 return result;
}
// The table owns the original dlopen reference for process lifetime, including
// partial/failed bootstrap hooks. Normal initialization reuses that exact handle.
static void *moduleFor(NSDictionary *package){
 NSString *path=package[@"path"];if(!modules)modules=[NSMutableDictionary dictionary];
 void *module=[modules[path] pointerValue];if(!module){module=dlopen(path.fileSystemRepresentation,RTLD_NOW|RTLD_LOCAL);if(module)modules[path]=[NSValue valueWithPointer:module];}return module;
}
int loader_bootstrap_plugin_packages(const loader_bootstrap_api_v1 *api){
 if(!NSThread.isMainThread||!api)return LOADER_ERR_INVALID;
 if(bootstrapAttempted)return bootstrapResult;bootstrapAttempted=YES;
 packages=scan();if(!packages)return bootstrapResult;
 NSArray *required=getenv("OPEN_SV2_LOADER_DATA_DIR")?loader_startup_required_bootstrap_owners():@[];
 if(!required)return bootstrapResult;
 NSMutableSet *declared=[NSMutableSet set],*ready=[NSMutableSet set];
 // Validate every declaration before invoking any plugin.
 for(NSDictionary *package in packages){
  id b=package[@"definition"][@"bootstrap"];if(!b)continue;
  if(![b isKindOfClass:NSDictionary.class]||![b[@"abi"] isEqual:@1]||CFGetTypeID((__bridge CFTypeRef)b[@"abi"])==CFBooleanGetTypeID()||![b[@"guardOwners"] isKindOfClass:NSArray.class]||[b[@"guardOwners"] count]>16)return bootstrapResult;
  for(id owner in b[@"guardOwners"]){if(!identifier(owner)||[declared containsObject:owner])return bootstrapResult;[declared addObject:owner];}
 }
 for(NSString *owner in required)if(![declared containsObject:owner])return bootstrapResult;
 for(NSDictionary *package in packages){
  NSDictionary *b=package[@"definition"][@"bootstrap"];if(!b)continue;
  void *module=moduleFor(package);loader_plugin_bootstrap_v1_fn entry=module?(loader_plugin_bootstrap_v1_fn)dlsym(module,"loader_plugin_bootstrap_v1"):NULL;
  if(!entry||entry(api)!=LOADER_OK){if(api->log)api->log("plugin-bootstrap-failed; host initialization refused");return bootstrapResult;}
  for(NSString *owner in b[@"guardOwners"]){[ready addObject:owner];loader_startup_register_bootstrap_owner(owner.UTF8String);}
 }
 for(NSString *owner in required)if(![ready containsObject:owner])return bootstrapResult;
 bootstrapResult=LOADER_OK;return bootstrapResult;
}
void loader_load_plugin_packages(const loader_api_v1 *api){
 if(!NSThread.isMainThread||normalAttempted)return;normalAttempted=YES;
 if(!bootstrapAttempted||bootstrapResult!=LOADER_OK){api->log("plugin-packages-bootstrap-not-ready");return;}
 for(NSDictionary *package in packages){
  NSDictionary *definition=package[@"definition"];NSString *title=definition[@"name"],*version=definition[@"version"];
  void *module=moduleFor(package);if(!module){api->log("plugin-package-module-load-failed");continue;}
  loader_plugin_init_v1_fn entry=(loader_plugin_init_v1_fn)dlsym(module,"loader_plugin_init_v1");int result=entry?entry(api):LOADER_ERR_INVALID;
  BOOL registered=NO;for(NSDictionary *item in loader_plugin_ui_call(@"ui.snapshot",@{})[@"result"][@"plugins"])if([item[@"id"] isEqual:definition[@"id"]])registered=YES;
  if(!registered){NSDictionary *ui=@{@"id":definition[@"id"],@"name":title,@"version":version,@"status":result==0?@"已就绪":@"初始化失败",@"sidebar":@{@"title":title,@"rows":@[@{@"type":@"label",@"text":result==0?@"插件已加载到宿主":@"插件初始化失败；模块保留至宿主退出"}]},@"settings":@[]};NSData *json=[NSJSONSerialization dataWithJSONObject:ui options:0 error:NULL];NSString *text=[[NSString alloc]initWithData:json encoding:NSUTF8StringEncoding];api->register_plugin_ui(text.UTF8String,NULL,module);}
  api->log(result==0?"plugin-package-resident-ready":"plugin-package-entry-failed");
 }
}
