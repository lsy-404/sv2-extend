#import <AppKit/AppKit.h>
#import <UniformTypeIdentifiers/UniformTypeIdentifiers.h>
#include "native_widgets.h"
#include <map>
#include <set>
#include <memory>
#include <vector>
extern "C" NSDictionary *loader_plugin_ui_call(NSString*,NSDictionary*);
extern "C" NSDictionary *loader_plugin_manager_call(NSString*,NSDictionary*);
extern "C" NSString *loader_plugin_manager_error_message(NSString*);
extern "C" int loader_extend_open_control_panel(void);
extern "C" int loader_extend_open_plugin_panel(const char*);
#if defined(__arm64__)
namespace {
using namespace native_widgets;
struct Field { NSString *__strong key=nil; NSString *__strong type=nil; NSArray *__strong choices=nil; id __strong initial=nil; Button button;Choice choice;TextEditor text; void destroy(){button.destroy();choice.destroy();text.destroy();} };
unsigned long nextInstance=0;
struct Row {void *object; NSString *__strong text;};
struct Panel {
 void *pane; NSString *__strong pluginId; bool script,editing=false,rendering=false; NSDictionary *__strong snapshot=nil; NSString *__strong message=@""; Viewport viewport; native_widgets::Component content; std::vector<std::unique_ptr<Button>> buttons;std::vector<std::unique_ptr<Label>> labels;std::vector<std::unique_ptr<Field>> fields;std::vector<Row> rows;int y=10,width=200;unsigned long generation=0,instance=++nextInstance;Label *messageLabel=nullptr,*statusLabel=nullptr;
 Panel(void *p,NSString *identifier,bool isScript):pane(p),pluginId(identifier),script(isScript){}
 ~Panel(){clear();viewport.destroy();content.destroy();}
 void clear(){rows.clear();messageLabel=nullptr;statusLabel=nullptr;for(auto &f:fields)f->destroy();fields.clear();for(auto &b:buttons)b->destroy();buttons.clear();for(auto &l:labels)l->destroy();labels.clear();}
 void showMessage(NSString *v){message=v;if(messageLabel){messageLabel->set(v.UTF8String);for(auto &row:rows)if(row.object==messageLabel->object)row.text=v;layout();}}
 void checkEditing();void render();void refresh();void event(NSString*,NSString*,NSString*,bool,Button*);void layout();
};
std::map<void*,std::unique_ptr<Panel>> panels;
std::set<void*> retiring;
unsigned long nextGeneration=0;
void enqueue(void *pane,unsigned long generation,NSString *action,NSString *identifier,NSString *key,bool state,Button *button){dispatch_async(dispatch_get_main_queue(),^{auto it=panels.find(pane);if(it!=panels.end()&&it->second->generation==generation)it->second->event(action,identifier,key,state,button);});}
void Panel::layout(){int w=*(int*)((char*)pane+0x40),h=*(int*)((char*)pane+0x44);if(w<=0||h<=44){visible(viewport.object,false);return;}int top=script?h-MIN(36,h-44):44, height=script?MIN(36,h-44):h-44;bounds(viewport.object,0,top,w,height);visible(viewport.object,true);int nextWidth=MAX(1,w-30);int nextY=10;
 for(auto &row:rows){
  int height=26;
  if(row.text){
   NSDictionary *attrs=@{NSFontAttributeName:[NSFont systemFontOfSize:12]};
   NSRect rect=[row.text boundingRectWithSize:NSMakeSize(nextWidth,CGFLOAT_MAX)
      options:NSStringDrawingUsesLineFragmentOrigin attributes:attrs];
   height=MAX(24,(int)ceil(rect.size.height)+8);
  }
  char *p=(char*)row.object;
  if(*(int*)(p+0x38)!=12||*(int*)(p+0x3c)!=nextY||
     *(int*)(p+0x40)!=nextWidth||*(int*)(p+0x44)!=height)
   bounds(row.object,12,nextY,nextWidth,height);
  nextY+=height+4;
 }
 y=nextY;width=nextWidth;
 int x=*(int*)((char*)content.object+0x38),cy=*(int*)((char*)content.object+0x3c);bounds(content.object,x,cy,w,MAX(height,y+10));}
void Panel::checkEditing(){for(auto &f:fields){if([f->type isEqual:@"boolean"]&&f->button.get()!=[f->initial boolValue])editing=true;else if([f->type isEqual:@"choice"]){int i=f->choice.get()-1;if(i>=0&&i<(int)f->choices.count&&![f->choices[i] isEqual:f->initial])editing=true;}else if(![f->type isEqual:@"boolean"]&&![f->type isEqual:@"choice"]){NSString *current=@(f->text.get().c_str());if(![current isEqual:[f->initial description]])editing=true;}}}
void Panel::refresh(){if(rendering||!viewport.object)return;checkEditing();NSDictionary *reply=loader_plugin_ui_call(@"ui.snapshot",@{});if(![reply[@"ok"] boolValue])return;NSDictionary *next=reply[@"result"];
 if(editing&&pluginId){
  NSDictionary *old=nil,*fresh=nil;
  for(NSDictionary *p in snapshot[@"plugins"])if([p[@"id"] isEqual:pluginId])old=p;
  for(NSDictionary *p in next[@"plugins"])if([p[@"id"] isEqual:pluginId])fresh=p;
  if(old&&fresh&&[old[@"values"] isEqual:fresh[@"values"]]){
   if(statusLabel&&![old[@"status"] isEqual:fresh[@"status"]]){
    statusLabel->set([fresh[@"status"] UTF8String]);
    for(auto &row:rows)if(row.object==statusLabel->object)row.text=fresh[@"status"];
   }
   snapshot=next;
  }
  else if(![next[@"generation"] isEqual:snapshot[@"generation"]])
   showMessage(@"设置已在其他位置更改，请重新载入后再保存。");
 }
 if(!editing&&pluginId&&!fields.empty()&&snapshot){
  NSDictionary *old=nil,*fresh=nil;
  for(NSDictionary *p in snapshot[@"plugins"])if([p[@"id"] isEqual:pluginId])old=p;
  for(NSDictionary *p in next[@"plugins"])if([p[@"id"] isEqual:pluginId])fresh=p;
  if(old&&fresh){
   NSMutableDictionary *a=[old mutableCopy],*b=[fresh mutableCopy];
   [a removeObjectForKey:@"status"];[b removeObjectForKey:@"status"];
   if([a isEqual:b]){if(statusLabel&&![old[@"status"] isEqual:fresh[@"status"]]){
 statusLabel->set([fresh[@"status"] UTF8String]);
 for(auto &row:rows)if(row.object==statusLabel->object)row.text=fresh[@"status"];
}snapshot=next;layout();return;}
  }
 }
 if(!editing&&snapshot&&fields.empty()&&width!=MAX(1,*(int*)((char*)pane+0x40)-30))render();if(!editing&&(![next[@"generation"] isEqual:snapshot[@"generation"]]||![next[@"manager"][@"generation"] isEqual:snapshot[@"manager"][@"generation"]])){snapshot=next;render();}layout();}
void Panel::render(){if(!viewport.object||!content.object||rendering)return;rendering=true;clear();generation=++nextGeneration;y=10;width=MAX(1,*(int*)((char*)pane+0x40)-30);void *parent=content.object;
 auto text=[&](NSString *value,bool bold=false){(void)bold;auto control=std::unique_ptr<Label>(new Label);NSString *v=value?:@"";if(!control->create(parent,v.UTF8String))return;CGFloat height=MAX(24,ceil((CGFloat)v.length/MAX(8,width/7))*16+8);bounds(control->object,12,y,width,(int)height);visible(control->object,true);y+=(int)height+4;rows.push_back({control->object,v});labels.push_back(std::move(control));};
 auto action=[&](NSString *caption,NSString *method,NSString *identifier,NSString *key,bool isToggle,bool state){auto control=std::unique_ptr<Button>(new Button);auto token=generation;void *owner=pane;NSString *a=[method copy],*i=[identifier copy],*k=[key copy];Button *raw=control.get();if(!control->create(parent,caption.UTF8String,isToggle,[owner,token,a,i,k,raw,isToggle]{enqueue(owner,token,a,i,k,isToggle&&raw->get(),raw);}))return;if(isToggle)control->set(state);bounds(control->object,12,y,width,26);visible(control->object,true);y+=30;rows.push_back({control->object,isToggle?caption:nil});buttons.push_back(std::move(control));};
 auto settings=[&](NSDictionary *plugin){NSString *pid=plugin[@"id"];BOOL builtin=[@[@"opensv2.lua-extend",@"opensv2.javascript-extend",@"opensv2.mcp"] containsObject:pid];NSString *section=nil;bool save=false;for(NSDictionary *f in plugin[@"settings"]){NSString *key=f[@"key"],*type=f[@"type"];if([key isEqual:@"enabled"]&&[type isEqual:@"boolean"])continue;if([f[@"section"] isKindOfClass:NSString.class]&&![f[@"section"] isEqual:section]){section=f[@"section"];text(section,true);}id value=plugin[@"values"][key];if([type isEqual:@"boolean"]&&builtin){action(f[@"title"],@"setting",pid,key,true,[value boolValue]);if([f[@"detail"] isKindOfClass:NSString.class])text(f[@"detail"]);continue;}
 auto field=std::unique_ptr<Field>(new Field);field->key=key;field->type=type;field->choices=f[@"choices"];field->initial=value;void *object=nullptr;save=true;if([type isEqual:@"boolean"]){if(!field->button.create(parent,[f[@"title"] UTF8String],true,[]{}))continue;field->button.set([value boolValue]);object=field->button.object;}else{text(f[@"title"]);if([type isEqual:@"choice"]){if(!field->choice.create(parent))continue;int n=0,selected=0;for(NSString *item in field->choices){field->choice.item(item.UTF8String,++n);if([item isEqual:value])selected=n;}if(selected)field->choice.set(selected);object=field->choice.object;}else{NSString *initial=[value isKindOfClass:NSString.class]?value:[value description];if(!field->text.create(parent,initial.UTF8String))continue;object=field->text.component();}}
 bounds(object,12,y,width,26);visible(object,true);y+=30;rows.push_back({object,nil});if([f[@"detail"] isKindOfClass:NSString.class])text(f[@"detail"]);fields.push_back(std::move(field));}if(save){action(@"保存设置",@"save",pid,nil,false,false);action(@"重新载入设置",@"reload",pid,nil,false,false);}};
 if(script){action(@"打开 Extend 菜单",@"open-center",nil,nil,false,false);layout();rendering=false;return;}
 if(pluginId){
 NSDictionary *plugin=nil;
 for(NSDictionary *p in snapshot[@"plugins"])if([p[@"id"] isEqual:pluginId])plugin=p;
 if(!plugin)text(@"此插件已卸载",true);
 else {
 text(plugin[@"sidebar"][@"title"],true);text(plugin[@"status"]);if(!labels.empty())statusLabel=labels.back().get();
 for(NSDictionary *f in plugin[@"settings"])
 if([f[@"key"] isEqual:@"enabled"]&&[f[@"type"] isEqual:@"boolean"])
 action(f[@"title"],@"setting",pluginId,@"enabled",true,[plugin[@"values"][@"enabled"] boolValue]);
 settings(plugin);
 for(NSDictionary *row in plugin[@"sidebar"][@"rows"]){
 if([row[@"type"] isEqual:@"button"])action(row[@"text"],@"action",pluginId,row[@"id"],false,false);
 else text(row[@"text"]);
 }
 }
 }
 else {text(@"插件",true);action(@"显示 Extend 入口",@"entry",nil,nil,true,[snapshot[@"extendEntryVisible"] boolValue]);action(@"检查插件更新",@"check",nil,nil,false,false);action(@"安装新插件…",@"local",nil,nil,false,false);action(@"从链接安装…",@"url",nil,nil,false,false);NSDictionary *manager=snapshot[@"manager"];text(manager[@"status"]?:@"");for(NSDictionary *p in manager[@"pending"]){text([NSString stringWithFormat:@"%@：%@ %@",[p[@"operation"] isEqual:@"remove"]?@"待删除":@"待重启",p[@"name"],p[@"version"]]);if([p[@"operation"] isEqual:@"remove"])action(@"取消删除",@"cancel",p[@"id"],nil,false,false);}for(NSDictionary *u in manager[@"updates"])action([NSString stringWithFormat:@"更新 %@ 至 %@",u[@"name"],u[@"version"]],@"update",u[@"id"],nil,false,false);
 NSMutableSet *registered=[NSMutableSet set];for(NSDictionary *p in snapshot[@"plugins"])[registered addObject:p[@"id"]];for(NSDictionary *p in manager[@"installed"])if(![registered containsObject:p[@"id"]]){text(p[@"name"],true);action(@"随宿主启动（重启生效）",@"package",p[@"id"],nil,true,[p[@"enabled"] boolValue]);action(@"删除插件（重启生效）",@"remove",p[@"id"],nil,false,false);}
 for(NSDictionary *p in snapshot[@"plugins"]){NSString *pid=p[@"id"];text(p[@"name"],true);for(NSDictionary *package in manager[@"installed"])if([package[@"id"] isEqual:pid]){action(@"随宿主启动（重启生效）",@"package",pid,nil,true,[package[@"enabled"] boolValue]);action(@"删除插件（重启生效）",@"remove",pid,nil,false,false);}for(NSDictionary *f in p[@"settings"])if([f[@"key"] isEqual:@"enabled"]&&[f[@"type"] isEqual:@"boolean"])action(@"启用插件",@"setting",pid,@"enabled",true,[p[@"values"][@"enabled"] boolValue]);if(![@[@"已就绪",@"已启用"] containsObject:p[@"status"]])text(p[@"status"]);action(@"显示右侧面板入口",@"visible",pid,nil,true,[p[@"visible"] boolValue]);action(@"打开插件面板",@"open",pid,nil,false,false);}}
 text(message);if(!labels.empty())messageLabel=labels.back().get();if(!pluginId)action(@"刷新插件列表",@"reload",nil,nil,false,false);layout();rendering=false;}
void Panel::event(NSString *action,NSString *identifier,NSString *key,bool state,Button *trigger){
 checkEditing();
 void *owner=pane;unsigned long identity=instance,epoch=generation;
 auto apply=[&](NSString *method,NSDictionary *args){
  NSDictionary *reply=loader_plugin_ui_call(method,args);
  auto it=panels.find(owner);
  if(it==panels.end()||it->second->instance!=identity)return;
  Panel *live=it->second.get();
  if([reply[@"ok"] boolValue]){
   live->snapshot=reply[@"result"];live->message=@"";
   if(!live->editing||[action isEqual:@"save"]){live->editing=false;live->render();}
  }else{
   if(live->generation==epoch&&trigger&&trigger->toggle)trigger->set(!state);
   live->showMessage([NSString stringWithFormat:@"操作未保存：%@。请刷新后重试。",reply[@"error"][@"code"]?:@"UNKNOWN"]);
  }
 };
 auto manager=[&](NSString *method,NSDictionary *args){
  if([snapshot[@"manager"][@"busy"] boolValue]){if(trigger&&trigger->toggle)trigger->set(!state);return;}
  NSDictionary *reply=loader_plugin_manager_call(method,args);
  auto it=panels.find(owner);
  if(it==panels.end()||it->second->instance!=identity)return;
  Panel *live=it->second.get();
  if(![reply[@"ok"] boolValue]){
   if(live->generation==epoch&&trigger&&trigger->toggle)trigger->set(!state);
   live->message=loader_plugin_manager_error_message(reply[@"error"][@"code"]);
  }else live->message=@"";
  live->editing=false;live->snapshot=nil;live->refresh();
 };

 NSNumber *gen=snapshot[@"generation"]?:@0,*mgen=snapshot[@"manager"][@"generation"]?:@0;
 if([action isEqual:@"entry"]){apply(@"ui.set_extend_visible",@{@"generation":gen,@"visible":@(state)});return;}
 if([action isEqual:@"setting"]){apply(@"ui.set_setting",@{@"id":identifier,@"key":key,@"value":@(state),@"generation":gen});return;}
 if([action isEqual:@"visible"]){apply(@"ui.set_visible",@{@"id":identifier,@"visible":@(state),@"generation":gen});return;}
 if([action isEqual:@"action"]){apply(@"ui.action",@{@"id":identifier,@"action":key,@"generation":gen});return;}
 if([action isEqual:@"save"]){NSMutableDictionary *values=[NSMutableDictionary dictionary];NSDictionary *plugin=nil;for(NSDictionary *p in snapshot[@"plugins"])if([p[@"id"] isEqual:identifier])plugin=p;[values addEntriesFromDictionary:plugin[@"values"]];for(auto &f:fields){if([f->type isEqual:@"boolean"])values[f->key]=@(f->button.get());else if([f->type isEqual:@"choice"]){int i=f->choice.get()-1;if(i<0||i>=(int)f->choices.count){showMessage(@"选项无效，尚未保存。");return;}values[f->key]=f->choices[i];}else{NSString *v=@(f->text.get().c_str());if([f->type isEqual:@"integer"]){NSScanner *scanner=[NSScanner scannerWithString:v];long long n;if(![scanner scanLongLong:&n]||!scanner.isAtEnd){showMessage(@"整数设置无效，尚未保存。");return;}values[f->key]=@(n);}else values[f->key]=v;}}apply(@"ui.set_settings",@{@"id":identifier,@"settings":values,@"generation":gen});return;}
 if([action isEqual:@"open"]){if(loader_extend_open_plugin_panel(identifier.UTF8String)!=0){message=@"请先显示该插件的右侧面板入口。";render();}return;}
 if([action isEqual:@"open-center"]){if(![snapshot[@"extendEntryVisible"] boolValue]){NSDictionary *r=loader_plugin_ui_call(@"ui.set_extend_visible",@{@"generation":gen,@"visible":@YES});if(![r[@"ok"] boolValue]){NSBeep();return;}}if(loader_extend_open_control_panel()!=0)NSBeep();return;}
 if([action isEqual:@"reload"]){editing=false;snapshot=nil;refresh();return;}
 if([action isEqual:@"check"]){manager(@"manager.check_updates",@{});return;}
 if([action isEqual:@"update"]){manager(@"manager.install_update",@{@"id":identifier});return;}
 if([action isEqual:@"package"]){manager(@"manager.set_enabled",@{@"id":identifier,@"enabled":@(state),@"generation":mgen});return;}
 if([action isEqual:@"remove"]){manager(@"manager.remove",@{@"id":identifier,@"generation":mgen});return;}
 if([action isEqual:@"cancel"]){manager(@"manager.cancel_remove",@{@"id":identifier,@"generation":mgen});return;}
 if([action isEqual:@"local"]){if([snapshot[@"manager"][@"busy"] boolValue])return;NSOpenPanel *panel=[NSOpenPanel openPanel];panel.canChooseFiles=YES;panel.canChooseDirectories=NO;panel.allowsMultipleSelection=NO;panel.allowedContentTypes=@[UTTypeZIP,[UTType typeWithFilenameExtension:@"sv2ext"]?:UTTypeData];void *owner=pane;unsigned long token=instance;[panel beginWithCompletionHandler:^(NSModalResponse response){auto it=panels.find(owner);if(response!=NSModalResponseOK||it==panels.end()||it->second->instance!=token)return;NSDictionary *r=loader_plugin_manager_call(@"manager.install_local",@{@"path":panel.URL.path});it=panels.find(owner);if(it==panels.end()||it->second->instance!=token)return;it->second->message=[r[@"ok"] boolValue]?@"":loader_plugin_manager_error_message(r[@"error"][@"code"]);it->second->snapshot=nil;it->second->refresh();}];return;}
 if([action isEqual:@"url"]){if([snapshot[@"manager"][@"busy"] boolValue])return;NSAlert *alert=[NSAlert new];alert.messageText=@"从链接安装插件";alert.informativeText=@"输入插件发布信息的 HTTPS 链接。安装后重新启动启用。";NSTextField *field=[[NSTextField alloc]initWithFrame:NSMakeRect(0,0,360,24)];field.placeholderString=@"https://…/plugin-release.json";alert.accessoryView=field;[alert addButtonWithTitle:@"继续"];[alert addButtonWithTitle:@"取消"];void *owner=pane;unsigned long token=instance;[alert beginSheetModalForWindow:NSApp.keyWindow completionHandler:^(NSModalResponse response){auto it=panels.find(owner);if(response!=NSAlertFirstButtonReturn||it==panels.end()||it->second->instance!=token)return;NSDictionary *r=loader_plugin_manager_call(@"manager.install_url",@{@"url":field.stringValue});it=panels.find(owner);if(it==panels.end()||it->second->instance!=token)return;it->second->message=[r[@"ok"] boolValue]?@"":loader_plugin_manager_error_message(r[@"error"][@"code"]);it->second->snapshot=nil;it->second->refresh();}];}
}
}
extern "C" bool loader_native_panel_owns(void *pane,void *child){auto i=panels.find(pane);return i!=panels.end()&&i->second->viewport.object==child;}
extern "C" void loader_native_panel_remove(void *pane){retiring.insert(pane);auto i=panels.find(pane);if(i!=panels.end()){auto owned=std::move(i->second);panels.erase(i);owned.reset();}retiring.erase(pane);}
extern "C" void loader_native_panel_capture(void *pane,NSString *id,bool script,intptr_t slide){
 if(retiring.count(pane))return;
 auto i=panels.find(pane);
 if(i==panels.end()){
  initialize(slide);auto p=std::unique_ptr<Panel>(new Panel(pane,id,script));
  p->rendering=true;panels.emplace(pane,std::move(p));i=panels.find(pane);
  if(!i->second->content.create()||!i->second->viewport.create(pane)){
   panels.erase(i);return;
  }
  i->second->viewport.content(i->second->content.object);
  i->second->rendering=false;
 }
 i->second->refresh();
}

#else
extern "C" bool loader_native_panel_owns(void*,void*){return false;}
extern "C" void loader_native_panel_remove(void*){}
extern "C" void loader_native_panel_capture(void*,NSString*,bool,intptr_t){}
#endif
