#import <Foundation/Foundation.h>
#include "loader_api.h"
#include <assert.h>
#include <stdlib.h>
extern int loader_plugin_ui_install(void);
extern int loader_register_plugin_ui(const char *,loader_plugin_ui_event,void *);
extern int loader_update_plugin_ui(const char *,const char *,void *);
extern int loader_plugin_ui_apply_icon_defaults(NSString *,NSDictionary *);
extern NSDictionary *loader_plugin_ui_call(NSString *,NSDictionary *);

static int owner;
static NSDictionary *snapshot(void){
 return loader_plugin_ui_call(@"ui.snapshot",@{})[@"result"];
}
static NSDictionary *plugin(void){
 return [snapshot()[@"plugins"] firstObject];
}
int main(int argc,char **argv){@autoreleasepool{
 assert(argc==2);
 setenv("OPEN_SV2_LOADER_DATA_DIR",argv[1],1);
 assert(loader_plugin_ui_install()==LOADER_OK);
 const char *definition="{\"id\":\"test.icon\",\"name\":\"Icon\",\"version\":\"1\",\"status\":\"ready\",\"sidebar\":{\"title\":\"Icon\",\"rows\":[]},\"settings\":[]}";
 assert(loader_register_plugin_ui(definition,NULL,&owner)==LOADER_OK);
 NSNumber *initial=snapshot()[@"generation"];
 NSDictionary *defaults=@{@"icon":@"<svg/>",@"activeIcon":@"<svg id='active'/>",@"matchIconColors":@NO};
 assert(loader_plugin_ui_apply_icon_defaults(@"test.icon",defaults)==LOADER_OK);
 assert([plugin()[@"icon"] isEqual:@"<svg/>"]);
 assert([plugin()[@"activeIcon"] isEqual:@"<svg id='active'/>"]);
 assert(![plugin()[@"matchIconColors"] boolValue]);
 assert([snapshot()[@"generation"] unsignedLongLongValue]>initial.unsignedLongLongValue);
 assert(loader_update_plugin_ui("test.icon","{\"icon\":\"<svg id='changed'/>\"}",&owner)==LOADER_OK);
 assert([plugin()[@"icon"] isEqual:@"<svg id='changed'/>"]);
 assert(loader_plugin_ui_apply_icon_defaults(@"test.icon",defaults)==LOADER_OK);
 assert([plugin()[@"icon"] isEqual:@"<svg id='changed'/>"]);
 NSDictionary *before=plugin();
 assert(loader_update_plugin_ui("test.icon","{\"activeIcon\":2}",&owner)==LOADER_ERR_INVALID);
 assert(loader_update_plugin_ui("test.icon","{\"icon\":\"invalid\"}",&owner)==LOADER_ERR_INVALID);
 assert(loader_update_plugin_ui("test.icon","{\"matchIconColors\":1}",&owner)==LOADER_ERR_INVALID);
 assert(loader_update_plugin_ui("test.icon","{\"icon\":\"other\"}",NULL)==LOADER_ERR_INVALID);
 assert([plugin() isEqual:before]);
 return 0;
}}
