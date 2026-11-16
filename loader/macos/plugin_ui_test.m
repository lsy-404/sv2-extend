#import <Foundation/Foundation.h>
#include "loader_api.h"
#include <pthread.h>
extern int loader_plugin_ui_install(void);
extern int loader_register_plugin_ui(const char*,loader_plugin_ui_event,void*);
extern int loader_unregister_plugin_ui(const char*,void*);
extern int loader_update_plugin_ui(const char*,const char*,void*);
extern NSDictionary *loader_plugin_ui_call(NSString*,NSDictionary*);
static int owner,settingsEvents,actionEvents;
static const char *definition="{\"id\":\"test.plugin\",\"name\":\"Test Plugin\",\"version\":\"1\",\"status\":\"ready\",\"sidebar\":{\"title\":\"Test\",\"rows\":[{\"type\":\"label\",\"text\":\"Hello\"},{\"type\":\"button\",\"id\":\"refresh\",\"text\":\"Refresh\"}]},\"settings\":[{\"key\":\"enabled\",\"title\":\"Enabled\",\"type\":\"boolean\",\"default\":true},{\"key\":\"count\",\"title\":\"Count\",\"type\":\"integer\",\"minimum\":0,\"maximum\":5,\"default\":1},{\"key\":\"choice\",\"title\":\"Choice\",\"type\":\"choice\",\"choices\":[\"one\",\"two\"],\"default\":\"one\"},{\"key\":\"title\",\"title\":\"Title\",\"type\":\"text\",\"default\":\"default\"}]}";
static void event(const char *json,void *context){if(context!=&owner)abort();id data=[NSJSONSerialization JSONObjectWithData:[@(json) dataUsingEncoding:NSUTF8StringEncoding] options:0 error:NULL];if([data[@"kind"]isEqual:@"settings"])settingsEvents++;else if([data[@"kind"]isEqual:@"action"])actionEvents++;}
static NSDictionary *snap(void){return loader_plugin_ui_call(@"ui.snapshot",@{})[@"result"];}
static NSDictionary *change(NSString *method,NSDictionary *args){NSMutableDictionary *value=[args mutableCopy];value[@"id"]=@"test.plugin";value[@"generation"]=snap()[@"generation"];return loader_plugin_ui_call(method,value);}
static void *worker(void *arg){int *result=arg;*result=loader_register_plugin_ui(definition,event,&owner);return NULL;}
int main(int argc,char **argv){@autoreleasepool{
 if(argc!=3)return 2;setenv("OPEN_SV2_LOADER_DATA_DIR",argv[1],1);loader_plugin_ui_install();NSMutableDictionary *checks=[NSMutableDictionary new];BOOL restored=!strcmp(argv[2],"restore");
 checks[@"registered"]=@(loader_register_plugin_ui(definition,event,&owner)==0);NSDictionary *initial=snap();checks[@"onePlugin"]=@([initial[@"plugins"]count]==1);checks[@"initialSettingsEvent"]=@(settingsEvents==1);
 if(restored){NSDictionary *p=[initial[@"plugins"]firstObject];checks[@"visibilityRestored"]=@(![p[@"visible"]boolValue]);checks[@"booleanRestored"]=@(![p[@"values"][@"enabled"]boolValue]);checks[@"integerRestored"]=@([p[@"values"][@"count"]isEqual:@4]);checks[@"choiceRestored"]=@([p[@"values"][@"choice"]isEqual:@"two"]);checks[@"textRestored"]=@([p[@"values"][@"title"]isEqual:@"声库"]);}
 else if(![initial[@"canPersist"]boolValue]){
  checks[@"badPreferencesPreservedReadOnly"]=@([loader_plugin_ui_call(@"ui.set_visible",@{@"id":@"test.plugin",@"generation":initial[@"generation"],@"visible":@NO})[@"error"][@"code"]isEqual:@"UI_PREFERENCES_WRITE_FAILED"]);
 }else{
  checks[@"duplicateRejected"]=@(loader_register_plugin_ui(definition,event,&owner)!=0);checks[@"wrongUnregisterOwnerRejected"]=@(loader_unregister_plugin_ui("test.plugin",NULL)!=0);checks[@"wrongUpdateOwnerRejected"]=@(loader_update_plugin_ui("test.plugin","{\"status\":\"bad\"}",NULL)!=0);
  int workerResult=0;pthread_t thread;pthread_create(&thread,NULL,worker,&workerResult);pthread_join(thread,NULL);checks[@"workerRegistrationRejected"]=@(workerResult==LOADER_ERR_INVALID);
  NSDictionary *stale=loader_plugin_ui_call(@"ui.set_visible",@{@"id":@"test.plugin",@"generation":@0,@"visible":@NO});checks[@"staleControlRejected"]=@([stale[@"error"][@"code"]isEqual:@"STALE_UI_GENERATION"]);
  checks[@"hideSaved"]=@([change(@"ui.set_visible",@{@"visible":@NO})[@"ok"]boolValue]);checks[@"hideDoesNotUnload"]=@([snap()[@"plugins"]count]==1&&settingsEvents==1&&actionEvents==0);
  checks[@"booleanSaved"]=@([change(@"ui.set_setting",@{@"key":@"enabled",@"value":@NO})[@"ok"]boolValue]);checks[@"integerSaved"]=@([change(@"ui.set_setting",@{@"key":@"count",@"value":@4})[@"ok"]boolValue]);checks[@"choiceSaved"]=@([change(@"ui.set_setting",@{@"key":@"choice",@"value":@"two"})[@"ok"]boolValue]);checks[@"unicodeTextSaved"]=@([change(@"ui.set_setting",@{@"key":@"title",@"value":@"声库"})[@"ok"]boolValue]);
  checks[@"invalidValueRejected"]=@([change(@"ui.set_setting",@{@"key":@"count",@"value":@1.5})[@"error"][@"code"]isEqual:@"INVALID_SETTING_VALUE"]);checks[@"unknownSettingRejected"]=@([change(@"ui.set_setting",@{@"key":@"password",@"value":@"x"})[@"error"][@"code"]isEqual:@"INVALID_SETTING_VALUE"]);
  NSDictionary *validBatch=@{@"enabled":@NO,@"count":@4,@"choice":@"two",@"title":@"声库"};
  NSNumber *beforeGeneration=snap()[@"generation"];int beforeEvents=settingsEvents;
  checks[@"invalidBatchRejected"]=@([change(@"ui.set_settings",@{@"settings":@{@"enabled":@YES,@"count":@999,@"choice":@"one",@"title":@"changed"}})[@"error"][@"code"]isEqual:@"INVALID_SETTING_VALUE"]);
  checks[@"invalidBatchAtomic"]=@([snap()[@"generation"]isEqual:beforeGeneration]&&settingsEvents==beforeEvents&&[[snap()[@"plugins"] firstObject][@"values"]isEqual:validBatch]);
  checks[@"missingBatchFieldRejected"]=@(![change(@"ui.set_settings",@{@"settings":@{@"enabled":@YES}})[@"ok"]boolValue]);
  checks[@"batchSaved"]=@([change(@"ui.set_settings",@{@"settings":validBatch})[@"ok"]boolValue]);
  checks[@"batchOneEvent"]=@(settingsEvents==beforeEvents+1);
  checks[@"actionDelivered"]=@([change(@"ui.action",@{@"action":@"refresh"})[@"ok"]boolValue]&&actionEvents==1);checks[@"unknownActionRejected"]=@([change(@"ui.action",@{@"action":@"unknown"})[@"error"][@"code"]isEqual:@"UI_ACTION_UNAVAILABLE"]);
  checks[@"invalidUpdateRejected"]=@(loader_update_plugin_ui("test.plugin","{\"sidebar\":[]}",&owner)!=0);checks[@"validUpdateAccepted"]=@(loader_update_plugin_ui("test.plugin","{\"status\":\"updated\"}",&owner)==0);checks[@"settingsEventsDelivered"]=@(settingsEvents==6);
  checks[@"unregisterAccepted"]=@(loader_unregister_plugin_ui("test.plugin",&owner)==0);checks[@"removedFromSnapshot"]=@([snap()[@"plugins"]count]==0);checks[@"removedActionRejected"]=@([change(@"ui.action",@{@"action":@"refresh"})[@"error"][@"code"]isEqual:@"PLUGIN_UI_NOT_FOUND"]);
 }
 BOOL passed=YES;for(NSNumber *v in checks.allValues)passed=passed&&v.boolValue;NSData *out=[NSJSONSerialization dataWithJSONObject:@{@"checks":checks,@"passed":@(passed)} options:NSJSONWritingPrettyPrinted error:NULL];fwrite(out.bytes,1,out.length,stdout);return passed?0:1;
}}
