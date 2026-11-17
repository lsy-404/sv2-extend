// SPDX-License-Identifier: Apache-2.0
#import <Foundation/Foundation.h>
#include "../../loader/macos/loader_api.h"
#include <stdatomic.h>
static _Atomic int mcpEnabled;
static BOOL listening;
static NSDictionary *toolSettings;
static NSObject *toolSettingsLock;
static BOOL toolEnabled(NSString *name){@synchronized(toolSettingsLock){id value=toolSettings[[@"tool." stringByAppendingString:name?:@""]];return !value||[value boolValue];}}

static const loader_api_v1 *loader;
@interface MCPResponse : NSObject
@property id requestID;
@property NSString *mode;
@property NSString *toolName;
@property NSDictionary *arguments;
@property loader_json_completion complete;
@property void *context;
@end
@implementation MCPResponse
@end
static NSArray *toolDefinitions(void) {
    NSDictionary *empty=@{@"type":@"object",@"properties":@{},@"additionalProperties":@NO};
    NSDictionary *group=@{@"type":@"object",@"properties":@{@"groupUUID":@{@"type":@"string"}},@"additionalProperties":@NO};
    return @[
        @{@"name":@"sv_status",@"method":@"capabilities",@"description":@"Read the resident SV2 MCP status and actual editor capabilities. No product account is required.",@"inputSchema":empty},
        @{@"name":@"sv_export_snapshot",@"method":@"exports.snapshot",@"description":@"Get a fresh 15-second current-project export guard and native render busy state.",@"inputSchema":empty},
        @{@"name":@"sv_export_wav",@"method":@"exports.start",@"description":@"Start an asynchronous WAV mixdown. Output parent must exist; existing files are never overwritten. Poll sv_export_status until terminal.",@"inputSchema":@{@"type":@"object",@"properties":@{@"guard":@{@"type":@"string"},@"outputPath":@{@"type":@"string"},@"channels":@{@"type":@"integer",@"enum":@[@1,@2]},@"sampleRate":@{@"type":@"integer",@"enum":@[@44100,@48000,@88200,@96000]},@"bitDepth":@{@"type":@"integer",@"enum":@[@16,@24,@32]}},@"required":@[@"guard",@"outputPath",@"channels",@"sampleRate",@"bitDepth"],@"additionalProperties":@NO}},
        @{@"name":@"sv_export_status",@"method":@"exports.status",@"description":@"Poll an export job. Completed native output is validated and atomically published without overwriting. Keep the same project open.",@"inputSchema":@{@"type":@"object",@"properties":@{@"jobId":@{@"type":@"string"}},@"required":@[@"jobId"],@"additionalProperties":@NO}},
        @{@"name":@"sv_export_cancel",@"method":@"exports.cancel",@"description":@"Request cancellation of the current native export job; poll status to confirm.",@"inputSchema":@{@"type":@"object",@"properties":@{@"jobId":@{@"type":@"string"}},@"required":@[@"jobId"],@"additionalProperties":@NO}},
        @{@"name":@"sv_read_editor",@"method":@"selection.read",@"description":@"Read the current live editor selection, notes and edit guards.",@"inputSchema":group},
        @{@"name":@"sv_read_voicebank",@"method":@"voicebanks.current",@"description":@"Read the host-selected voicebank identity through the native host adapter.",@"inputSchema":group},
        @{@"name":@"sv_project_voicebanks",@"method":@"voicebanks.project",@"description":@"List assigned voicebank identities used by the current project and their reference counts.",@"inputSchema":empty},
        @{@"name":@"sv_list_voicebanks",@"method":@"voicebanks.list",@"description":@"List voicebanks reported as available by the running editor, not the sales catalog.",@"inputSchema":empty},
        @{@"name":@"sv_select_voicebank",@"method":@"voicebanks.select",@"description":@"Select a voicebank using a fresh host selection guard; verify the resulting identity.",@"inputSchema":@{@"type":@"object",@"properties":@{@"voicebankId":@{@"type":@"string"},@"guard":@{@"type":@"string"},@"groupUUID":@{@"type":@"string"}},@"required":@[@"voicebankId",@"guard"],@"additionalProperties":@NO}},
        @{@"name":@"sv_set_note_pitch",@"method":@"notes.set_pitch",@"description":@"Set MIDI note pitch through the live editor, with a fresh guard and one undo record.",@"inputSchema":@{@"type":@"object",@"properties":@{@"groupUUID":@{@"type":@"string"},@"noteIndex":@{@"type":@"integer",@"minimum":@1},@"midiPitch":@{@"type":@"integer",@"minimum":@0,@"maximum":@127},@"guard":@{@"type":@"string"}},@"required":@[@"noteIndex",@"midiPitch",@"guard"],@"additionalProperties":@NO}},
        @{@"name":@"sv_read_pitch_range",@"method":@"pitch.read_range",@"description":@"Read pitchDelta points and a fresh range guard from the live editor.",@"inputSchema":@{@"type":@"object",@"properties":@{@"groupUUID":@{@"type":@"string"},@"startBlick":@{@"type":@"integer",@"minimum":@0},@"endBlick":@{@"type":@"integer",@"minimum":@0}},@"required":@[@"startBlick",@"endBlick"],@"additionalProperties":@NO}},
        @{@"name":@"sv_set_pitch_curve",@"method":@"pitch.replace_range",@"description":@"Replace an explicit bounded pitchDelta range, preserving unrelated control points and one undo record.",@"inputSchema":@{@"type":@"object",@"properties":@{@"groupUUID":@{@"type":@"string"},@"startBlick":@{@"type":@"integer",@"minimum":@0},@"endBlick":@{@"type":@"integer",@"minimum":@0},@"points":@{@"type":@"array",@"maxItems":@4096,@"items":@{@"type":@"array",@"minItems":@2,@"maxItems":@2,@"items":@{@"type":@"number"}}},@"guard":@{@"type":@"string"},@"curveGuard":@{@"type":@"string"}},@"required":@[@"startBlick",@"endBlick",@"points",@"guard",@"curveGuard"],@"additionalProperties":@NO}},
        @{@"name":@"sv_create_group",@"method":@"groups.create",@"description":@"Create a note group on the current track using the current edit guard.",@"inputSchema":@{@"type":@"object",@"properties":@{@"name":@{@"type":@"string",@"maxLength":@256},@"guard":@{@"type":@"string"}},@"required":@[@"guard"],@"additionalProperties":@NO}},
        @{@"name":@"sv_remove_group",@"method":@"groups.remove",@"description":@"Remove an exclusively referenced library group using its fresh edit guard.",@"inputSchema":@{@"type":@"object",@"properties":@{@"groupUUID":@{@"type":@"string"},@"guard":@{@"type":@"string"}},@"required":@[@"groupUUID",@"guard"],@"additionalProperties":@NO}},
        @{@"name":@"sv_add_note",@"method":@"notes.add",@"description":@"Add one note to an exclusive group using a fresh edit guard.",@"inputSchema":@{@"type":@"object",@"properties":@{@"groupUUID":@{@"type":@"string"},@"onsetBlick":@{@"type":@"integer",@"minimum":@0},@"durationBlick":@{@"type":@"integer",@"minimum":@1},@"midiPitch":@{@"type":@"integer",@"minimum":@0,@"maximum":@127},@"lyrics":@{@"type":@"string",@"maxLength":@1024},@"guard":@{@"type":@"string"}},@"required":@[@"onsetBlick",@"durationBlick",@"midiPitch",@"guard"],@"additionalProperties":@NO}},
        @{@"name":@"sv_remove_note",@"method":@"notes.remove",@"description":@"Remove one note from an exclusive group using a fresh edit guard.",@"inputSchema":@{@"type":@"object",@"properties":@{@"groupUUID":@{@"type":@"string"},@"noteIndex":@{@"type":@"integer",@"minimum":@1},@"guard":@{@"type":@"string"}},@"required":@[@"noteIndex",@"guard"],@"additionalProperties":@NO}}
    ];
}
static NSArray *availableDefinitions(NSDictionary *snapshot) {
    NSArray *available=snapshot[@"capabilities"][@"methods"]?:@[];NSMutableArray *definitions=[NSMutableArray array];
    for(NSDictionary *definition in toolDefinitions())if(toolEnabled(definition[@"name"])&&([definition[@"method"] isEqualToString:@"capabilities"] || [available containsObject:definition[@"method"]]))[definitions addObject:definition];
    for(NSDictionary *provider in snapshot[@"capabilities"][@"providers"])for(NSDictionary *definition in provider[@"tools"])if(toolEnabled(definition[@"name"])&&[available containsObject:definition[@"method"]])[definitions addObject:definition];
    return definitions;
}
static void sendJSON(NSDictionary *value,loader_json_completion complete,void *context) {NSData *data=[NSJSONSerialization dataWithJSONObject:value options:0 error:NULL];complete(data.bytes,data.length,context);}
static void sendError(id requestID,int code,NSString *message,loader_json_completion complete,void *context) {sendJSON(@{@"jsonrpc":@"2.0",@"id":requestID?:NSNull.null,@"error":@{@"code":@(code),@"message":message}},complete,context);}
static void editorComplete(const char *json,size_t length,void *opaque) {
    MCPResponse *response=CFBridgingRelease(opaque);
    if(!atomic_load(&mcpEnabled)){sendError(response.requestID,-32001,@"MCP is disabled",response.complete,response.context);return;}id value=[NSJSONSerialization JSONObjectWithData:[NSData dataWithBytes:json length:length] options:0 error:NULL];
    if(![value isKindOfClass:NSDictionary.class]){sendError(response.requestID,-32603,@"Invalid editor adapter response",response.complete,response.context);return;}
    NSDictionary *result;
    if([response.mode isEqualToString:@"resolve"]) {
        if(!toolEnabled(response.toolName)){sendError(response.requestID,-32002,@"Tool is disabled in Extend settings",response.complete,response.context);return;}
        // Resolve extension methods from a fresh registry snapshot; no stale tool cache.
        NSString *method;
        for(NSDictionary *definition in availableDefinitions(value))if([definition[@"name"] isEqualToString:response.toolName]){method=definition[@"method"];break;}
        // Known built-ins retain a precise unavailable-adapter error when not advertised.
        if(!method)for(NSDictionary *definition in toolDefinitions())if([definition[@"name"] isEqualToString:response.toolName]){method=definition[@"method"];break;}
        if(!method){sendError(response.requestID,-32602,@"Unknown tool",response.complete,response.context);return;}
        response.mode=@"call";NSData *data=[NSJSONSerialization dataWithJSONObject:response.arguments options:0 error:NULL];NSString *arguments=[[NSString alloc] initWithData:data encoding:NSUTF8StringEncoding];
        loader->editor_request_json(method.UTF8String,arguments.UTF8String,editorComplete,(__bridge_retained void*)response);return;
    }
    if([response.mode isEqualToString:@"list"]) {
        NSMutableArray *tools=[NSMutableArray array];
        for(NSDictionary *definition in availableDefinitions(value)){
            NSMutableDictionary *tool=[definition mutableCopy];[tool removeObjectForKey:@"method"];[tools addObject:tool];
        }
        result=@{@"tools":tools};
    } else {
        NSData *encoded=[NSJSONSerialization dataWithJSONObject:value options:NSJSONWritingSortedKeys error:NULL];
        result=@{@"content":@[@{@"type":@"text",@"text":[[NSString alloc] initWithData:encoded encoding:NSUTF8StringEncoding]}],@"structuredContent":value,@"isError":([value[@"ok"] boolValue]?@NO:@YES)};
    }
    sendJSON(@{@"jsonrpc":@"2.0",@"id":response.requestID,@"result":result},response.complete,response.context);
}
static void handle(const char *body,size_t length,loader_json_completion complete,void *completionContext,void *handlerContext) {
    if(!atomic_load(&mcpEnabled)){sendError(nil,-32001,@"MCP is disabled",complete,completionContext);return;}
    (void)handlerContext;id request=[NSJSONSerialization JSONObjectWithData:[NSData dataWithBytes:body length:length] options:0 error:NULL];
    if(![request isKindOfClass:NSDictionary.class] || ![request[@"jsonrpc"] isEqual:@"2.0"] || ![request[@"method"] isKindOfClass:NSString.class]){sendError(nil,-32600,@"Invalid JSON-RPC request",complete,completionContext);return;}
    NSString *method=request[@"method"];id requestID=request[@"id"];
    if(!requestID){if([method hasPrefix:@"notifications/"]){complete(NULL,0,completionContext);return;}sendError(nil,-32600,@"Request id required",complete,completionContext);return;}
    if((![requestID isKindOfClass:NSString.class] && ![requestID isKindOfClass:NSNumber.class]) || CFGetTypeID((__bridge CFTypeRef)requestID)==CFBooleanGetTypeID()){sendError(nil,-32600,@"Invalid request id",complete,completionContext);return;}
    if(request[@"params"] && ![request[@"params"] isKindOfClass:NSDictionary.class]){sendError(requestID,-32602,@"Parameters must be an object",complete,completionContext);return;}
    if([method isEqualToString:@"initialize"]){NSString *version=request[@"params"][@"protocolVersion"];if(![@[@"2025-11-25",@"2025-06-18",@"2025-03-26"] containsObject:version])version=@"2025-11-25";sendJSON(@{@"jsonrpc":@"2.0",@"id":requestID,@"result":@{@"protocolVersion":version,@"capabilities":@{@"tools":@{@"listChanged":@NO}},@"serverInfo":@{@"name":@"sv2-mcp",@"version":@"0.3.0"}}},complete,completionContext);return;}
    if([method isEqualToString:@"ping"]){sendJSON(@{@"jsonrpc":@"2.0",@"id":requestID,@"result":@{}},complete,completionContext);return;}
    NSString *editorMethod;NSDictionary *arguments=@{};
    if([method isEqualToString:@"tools/list"])editorMethod=@"capabilities";
    else if([method isEqualToString:@"tools/call"]){
        id params=request[@"params"];if(![params isKindOfClass:NSDictionary.class] || ![params[@"name"] isKindOfClass:NSString.class]){sendError(requestID,-32602,@"Invalid tool parameters",complete,completionContext);return;}
        editorMethod=@"capabilities";
        arguments=params[@"arguments"]?:@{};if(![arguments isKindOfClass:NSDictionary.class]){sendError(requestID,-32602,@"Tool arguments must be an object",complete,completionContext);return;}
    } else {sendError(requestID,-32601,@"Method not found",complete,completionContext);return;}
    MCPResponse *response=[MCPResponse new];response.requestID=requestID;response.mode=[method isEqualToString:@"tools/list"]?@"list":@"resolve";response.toolName=request[@"params"][@"name"];response.arguments=arguments;response.complete=complete;response.context=completionContext;
    NSData *data=[NSJSONSerialization dataWithJSONObject:arguments options:0 error:NULL];NSString *encoded=[[NSString alloc] initWithData:data encoding:NSUTF8StringEncoding];
    loader->editor_request_json(editorMethod.UTF8String,encoded.UTF8String,editorComplete,(__bridge_retained void*)response);
}
static NSString *uiEndpoint;
static void updateUI(const char *body,void *owner){
 (void)owner;id event=[NSJSONSerialization JSONObjectWithData:[@(body) dataUsingEncoding:NSUTF8StringEncoding] options:0 error:NULL];
 if(![event[@"kind"] isEqual:@"settings"])return;
 @synchronized(toolSettingsLock){toolSettings=[event[@"settings"] copy];}
 BOOL enabled=[event[@"settings"][@"enabled"] boolValue];atomic_store(&mcpEnabled,enabled);
 if(enabled&&!listening){loader_http_endpoint_v1 endpoint={.struct_size=sizeof endpoint};listening=loader->listen_local_json("sv2-mcp",handle,NULL,&endpoint)==LOADER_OK;if(listening)uiEndpoint=[NSString stringWithFormat:@"http://127.0.0.1:%u/mcp",endpoint.port];}
 if(!enabled&&listening){if(loader->stop_local_json("sv2-mcp",handle,NULL)==LOADER_OK){listening=NO;uiEndpoint=nil;}}
 NSString *status=!enabled?@"已停用":listening?@"已就绪":@"监听启动失败";
 NSMutableArray *rows=[NSMutableArray arrayWithObject:@{@"type":@"label",@"text":!enabled?@"MCP 已关闭":listening?@"实时编辑服务运行中":@"无法启动本地服务"}];
 if(listening&&[event[@"settings"][@"showEndpoint"] boolValue])[rows addObject:@{@"type":@"label",@"text":uiEndpoint}];
 NSData *data=[NSJSONSerialization dataWithJSONObject:@{@"status":status,@"sidebar":@{@"title":@"SV2 MCP",@"rows":rows}} options:0 error:NULL];NSString *encoded=[[NSString alloc]initWithData:data encoding:NSUTF8StringEncoding];loader->update_plugin_ui("opensv2.mcp",encoded.UTF8String,NULL);
}
int loader_plugin_init_v1(const loader_api_v1 *api) {
 if(!api||api->abi_version!=LOADER_ABI_V1||api->struct_size<offsetof(loader_api_v1,stop_local_json)+sizeof api->stop_local_json||!api->stop_local_json||!api->register_plugin_ui)return LOADER_ERR_INVALID;
 loader=api;toolSettingsLock=[NSObject new];
 NSMutableArray *fields=[NSMutableArray arrayWithObjects:@{@"key":@"enabled",@"title":@"启用 MCP",@"type":@"boolean",@"default":@YES},@{@"key":@"showEndpoint",@"title":@"侧栏显示连接地址",@"type":@"boolean",@"default":@NO},nil];
 NSDictionary *titles=@{@"sv_status":@"读取服务状态",@"sv_read_editor":@"读取工程与选区",@"sv_read_voicebank":@"读取当前声库",@"sv_project_voicebanks":@"读取工程声库",@"sv_list_voicebanks":@"读取声库列表",@"sv_select_voicebank":@"切换声库",@"sv_set_note_pitch":@"修改音符音高",@"sv_read_pitch_range":@"读取音高曲线",@"sv_set_pitch_curve":@"修改音高曲线",@"sv_export_snapshot":@"读取导出状态",@"sv_export_wav":@"开始导出",@"sv_export_status":@"查询导出任务",@"sv_export_cancel":@"取消导出任务",@"sv_create_group":@"创建音符组",@"sv_remove_group":@"删除音符组",@"sv_add_note":@"添加音符",@"sv_remove_note":@"删除音符"};
 for(NSDictionary *tool in toolDefinitions())[fields addObject:@{@"key":[@"tool." stringByAppendingString:tool[@"name"]],@"title":titles[tool[@"name"]]?:tool[@"name"],@"detail":tool[@"name"],@"section":@"MCP 工具",@"type":@"boolean",@"default":@YES}];
 NSDictionary *descriptor=@{@"id":@"opensv2.mcp",@"name":@"SV2 MCP",@"version":@"0.3.0",@"status":@"正在启动",@"sidebar":@{@"title":@"SV2 MCP",@"rows":@[]},@"settings":fields};
 NSData *data=[NSJSONSerialization dataWithJSONObject:descriptor options:0 error:NULL];NSString *encoded=[[NSString alloc]initWithData:data encoding:NSUTF8StringEncoding];
 return loader->register_plugin_ui(encoded.UTF8String,updateUI,NULL);
}
