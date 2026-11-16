#import <Foundation/Foundation.h>
#include <stdio.h>
extern int loader_plugin_ui_install(void),loader_lua_call_gate_install(void);
extern void loader_plugin_ui_register_runtimes(int,int);
extern NSDictionary *loader_plugin_ui_call(NSString*,NSDictionary*);
extern const char *loader_script_call_json_for_runtime(const char*,size_t,int);
int loader_javascript_gate_ready(void){return 1;}
NSDictionary *loader_export_call(NSString *m,NSDictionary *a){(void)m;(void)a;return @{@"ok":@YES};}
NSDictionary *loader_voice_call(NSString *m,NSDictionary *a){(void)m;(void)a;return @{@"ok":@YES};}
NSDictionary *loader_lua_editor_call(NSString *m,NSDictionary *a){(void)m;(void)a;return @{@"ok":@YES};}
NSDictionary *loader_lua_capability_call(NSString *m,NSDictionary *a){(void)m;(void)a;return @{@"ok":@YES};}
NSDictionary *loader_capability_snapshot(void){return @{@"capabilities":@{},@"capabilityGeneration":@1};}
static NSDictionary *call(NSString *method,NSString *target,int js){NSData *d=[NSJSONSerialization dataWithJSONObject:@{@"method":method,@"arguments":target?@{@"method":target}:@{}} options:0 error:NULL];const char *r=loader_script_call_json_for_runtime(d.bytes,d.length,js);return r?[NSJSONSerialization JSONObjectWithData:[[@(r) dataUsingEncoding:NSUTF8StringEncoding] copy] options:0 error:NULL]:nil;}
static BOOL setting(NSString *id,NSString *key,BOOL value){NSNumber *generation=loader_plugin_ui_call(@"ui.snapshot",@{})[@"result"][@"generation"];return [loader_plugin_ui_call(@"ui.set_setting",@{@"id":id,@"generation":generation,@"key":key,@"value":@(value)})[@"ok"] boolValue];}
int main(int argc,const char **argv){(void)argv;@autoreleasepool{loader_plugin_ui_install();loader_lua_call_gate_install();loader_plugin_ui_register_runtimes(1,1);NSMutableDictionary *c=[NSMutableDictionary dictionary];
 if(argc>1&&!strcmp(argv[1],"migration")){NSDictionary *v=loader_plugin_ui_call(@"ui.snapshot",@{})[@"result"];NSDictionary *lua=nil;for(NSDictionary *item in v[@"plugins"])if([item[@"id"] isEqual:@"opensv2.lua-extend"])lua=item;BOOL ok=![lua[@"values"][@"voicebanks.current"] boolValue]&&![lua[@"values"][@"voicebanks.list"] boolValue]&&![lua[@"values"][@"voicebanks.select"] boolValue]&&setting(@"opensv2.lua-extend",@"voicebanks.list",YES)&&[call(@"call.submit",@"voicebanks.list",0)[@"ok"] boolValue]&&[call(@"voice.current",nil,0)[@"error"][@"code"] isEqual:@"FEATURE_DISABLED"];puts(ok?"{\"migration\":true}":"{\"migration\":false}");return ok?0:1;}
 if(argc>1){NSDictionary *v=loader_plugin_ui_call(@"ui.snapshot",@{})[@"result"];BOOL ok=![v[@"extendEntryVisible"] boolValue]&&[call(@"call.submit",@"http.start",1)[@"error"][@"code"] isEqual:@"EXTENSION_DISABLED"];puts(ok?"{\"restored\":true}":"{\"restored\":false}");return ok?0:1;}
 c[@"defaultVoiceEnabled"]=call(@"voice.current",nil,0)[@"ok"];
 c[@"disableLuaVoiceSaved"]=@(setting(@"opensv2.lua-extend",@"voicebanks.current",NO));
 c[@"luaVoiceDenied"]=@([call(@"voice.current",nil,0)[@"error"][@"code"] isEqual:@"FEATURE_DISABLED"]);
 c[@"luaAsyncVoiceDenied"]=@([call(@"call.submit",@"voicebanks.current",0)[@"error"][@"code"] isEqual:@"FEATURE_DISABLED"]);
 c[@"luaListStillEnabled"]=call(@"call.submit",@"voicebanks.list",0)[@"ok"];
 c[@"jsVoiceIndependent"]=call(@"voice.current",nil,1)[@"ok"];
 c[@"otherLuaFeatureUnaffected"]=call(@"export.snapshot",nil,0)[@"ok"];
 c[@"disableLuaSaved"]=@(setting(@"opensv2.lua-extend",@"enabled",NO));
 c[@"luaExportDenied"]=@([call(@"export.snapshot",nil,0)[@"error"][@"code"] isEqual:@"EXTENSION_DISABLED"]);
 c[@"editorProviderUnaffected"]=call(@"editor.next",nil,0)[@"ok"];
 c[@"internalVoiceProviderUnaffected"]=call(@"voice.capture.begin",nil,0)[@"ok"];
 c[@"internalExportProviderUnaffected"]=call(@"export.capture.begin",nil,0)[@"ok"];
 c[@"alreadyAcceptedCleanupAllowed"]=call(@"call.submit",@"exports.cancel",0)[@"ok"];
 c[@"explicitCancelDisabledSaved"]=@(setting(@"opensv2.lua-extend",@"exports.cancel",NO));
 c[@"explicitCancelDisabledEnforced"]=@([call(@"call.submit",@"exports.cancel",0)[@"error"][@"code"] isEqual:@"FEATURE_DISABLED"]);setting(@"opensv2.lua-extend",@"exports.cancel",YES);
 c[@"disableJSSaved"]=@(setting(@"opensv2.javascript-extend",@"enabled",NO));
 c[@"jsDenied"]=@([call(@"call.submit",@"http.start",1)[@"error"][@"code"] isEqual:@"EXTENSION_DISABLED"]);
 c[@"restoreLuaSaved"]=@(setting(@"opensv2.lua-extend",@"enabled",YES));
 c[@"restoreVoiceSaved"]=@(setting(@"opensv2.lua-extend",@"voicebanks.current",YES));
 c[@"luaRestored"]=call(@"voice.current",nil,0)[@"ok"];
 NSDictionary *snap=loader_plugin_ui_call(@"ui.snapshot",@{})[@"result"];c[@"entryDefaultVisible"]=snap[@"extendEntryVisible"];
 NSDictionary *hidden=loader_plugin_ui_call(@"ui.set_extend_visible",@{@"generation":snap[@"generation"],@"visible":@NO});c[@"entryHidden"]=@([hidden[@"ok"] boolValue]&&![hidden[@"result"][@"extendEntryVisible"] boolValue]);
 c[@"entryNotPlugin"]=@([hidden[@"result"][@"plugins"] count]==2);
 c[@"staleEntryRejected"]=@(![loader_plugin_ui_call(@"ui.set_extend_visible",@{@"generation":snap[@"generation"],@"visible":@YES})[@"ok"] boolValue]);
 BOOL ok=YES;for(NSNumber *v in c.allValues)if(!v.boolValue)ok=NO;NSDictionary *report=@{@"checks":c,@"passed":@(ok)};NSData *d=[NSJSONSerialization dataWithJSONObject:report options:NSJSONWritingPrettyPrinted error:NULL];puts([[NSString alloc]initWithData:d encoding:NSUTF8StringEncoding].UTF8String);return ok?0:1;}}
