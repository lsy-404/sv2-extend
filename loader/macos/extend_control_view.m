// In-process AppKit renderer for Loader's generic UI registry. No web/IPC bridge.
#import <AppKit/AppKit.h>
#import <UniformTypeIdentifiers/UniformTypeIdentifiers.h>
#include "plugin_manager.h"
extern NSDictionary *loader_plugin_ui_call(NSString*,NSDictionary*);
extern int loader_extend_open_plugin_panel(const char*);
extern int loader_extend_open_control_panel(void);
// Host-matched controls: compact typography, neutral panels and green switches.
static NSColor *hostBackground(void){return [NSColor colorWithSRGBRed:.177 green:.169 blue:.177 alpha:1];}
static NSColor *hostText(void){return [NSColor colorWithWhite:.78 alpha:1];}
static NSAttributedString *switchTitle(NSString *title){NSMutableAttributedString *text=[[NSMutableAttributedString alloc]initWithString:title attributes:@{NSFontAttributeName:[NSFont systemFontOfSize:12],NSForegroundColorAttributeName:hostText()}];NSRange newline=[title rangeOfString:@"\n"];if(newline.location!=NSNotFound)[text addAttributes:@{NSFontAttributeName:[NSFont monospacedSystemFontOfSize:10 weight:NSFontWeightRegular],NSForegroundColorAttributeName:[NSColor colorWithWhite:.56 alpha:1]} range:NSMakeRange(newline.location+1,title.length-newline.location-1)];return text;}
@interface ExtendSwitchCell:NSButtonCell @end
@implementation ExtendSwitchCell
- (void)drawWithFrame:(NSRect)frame inView:(NSView*)view{(void)view;NSRect square=NSMakeRect(frame.origin.x+1,frame.origin.y+5,12,12);NSBezierPath *box=[NSBezierPath bezierPathWithRoundedRect:square xRadius:2 yRadius:2];[(self.state==NSControlStateValueOn?[NSColor colorWithSRGBRed:.59 green:.78 blue:.40 alpha:1]:[NSColor colorWithWhite:.23 alpha:1]) setFill];[box fill];[[NSColor colorWithWhite:self.state==NSControlStateValueOn?.62:.43 alpha:1] setStroke];box.lineWidth=.8;[box stroke];if(self.state==NSControlStateValueOn){NSBezierPath *mark=[NSBezierPath bezierPath];[mark moveToPoint:NSMakePoint(NSMinX(square)+2.5,NSMinY(square)+6)];[mark lineToPoint:NSMakePoint(NSMinX(square)+5,NSMinY(square)+8.5)];[mark lineToPoint:NSMakePoint(NSMinX(square)+9.5,NSMinY(square)+3.5)];mark.lineWidth=1.4;[[NSColor colorWithWhite:.13 alpha:1] setStroke];[mark stroke];}[switchTitle(self.title) drawWithRect:NSMakeRect(frame.origin.x+20,frame.origin.y+2,MAX(1,frame.size.width-20),frame.size.height-2) options:NSStringDrawingUsesLineFragmentOrigin];}
@end
@interface ExtendActionCell:NSButtonCell @end
@implementation ExtendActionCell
- (void)drawWithFrame:(NSRect)frame inView:(NSView*)view{(void)view;NSBezierPath *path=[NSBezierPath bezierPathWithRoundedRect:NSInsetRect(frame,.5,.5) xRadius:2 yRadius:2];[[NSColor colorWithWhite:self.isHighlighted?.27:.23 alpha:1] setFill];[path fill];[[NSColor colorWithWhite:.30 alpha:1] setStroke];path.lineWidth=.7;[path stroke];NSDictionary *attributes=@{NSFontAttributeName:[NSFont systemFontOfSize:12],NSForegroundColorAttributeName:hostText()};NSSize size=[self.title sizeWithAttributes:attributes];[self.title drawAtPoint:NSMakePoint(NSMidX(frame)-size.width/2,NSMidY(frame)-size.height/2) withAttributes:attributes];}
@end
static NSButton *hostSwitch(NSString *title,id target,SEL action){NSButton *button=[NSButton checkboxWithTitle:title target:target action:action];ExtendSwitchCell *cell=[[ExtendSwitchCell alloc]initTextCell:title];cell.buttonType=NSButtonTypeSwitch;cell.font=[NSFont systemFontOfSize:12];button.cell=cell;button.target=target;button.action=action;return button;}
static CGFloat switchHeight(NSString *title,CGFloat width){NSRect rect=[switchTitle(title) boundingRectWithSize:NSMakeSize(MAX(40,width-20),CGFLOAT_MAX) options:NSStringDrawingUsesLineFragmentOrigin];return MAX(23,ceil(rect.size.height)+5);}
@interface ExtendControlView : NSView
@property NSScrollView *scroll;
@property NSView *document;
@property NSMutableDictionary<NSString*,NSControl*> *fields;
@property NSDictionary *snapshot;
@property NSTextField *message;
@property BOOL editing;
@property NSString *pluginIdentifier;
- (void)refresh;
@end
@interface ExtendFlippedView : NSView @end
@implementation ExtendFlippedView
- (BOOL)isFlipped{return YES;}
@end
@implementation ExtendControlView
- (BOOL)isFlipped{return YES;}
- (instancetype)initWithFrame:(NSRect)frame{if((self=[super initWithFrame:frame])){self.appearance=[NSAppearance appearanceNamed:NSAppearanceNameDarkAqua];[self setAccessibilityElement:YES];[self setAccessibilityRole:NSAccessibilityGroupRole];[self setAccessibilityLabel:@"Extend 控制中心"];self.wantsLayer=YES;self.layer.backgroundColor=hostBackground().CGColor;self.scroll=[[NSScrollView alloc]initWithFrame:self.bounds];self.scroll.autoresizingMask=NSViewWidthSizable|NSViewHeightSizable;self.scroll.hasVerticalScroller=YES;self.scroll.scrollerStyle=NSScrollerStyleOverlay;[self.scroll setAccessibilityLabel:@"Extend 插件管理"];self.scroll.drawsBackground=NO;[self addSubview:self.scroll];[self setAccessibilityChildren:@[self.scroll]];[self refresh];}return self;}
- (void)setFrameSize:(NSSize)size{BOOL changed=fabs(size.width-self.frame.size.width)>1;[super setFrameSize:size];if(changed&&self.snapshot&&!self.editing)[self render];}
- (NSTextField*)label:(NSString*)text y:(CGFloat)y height:(CGFloat)height bold:(BOOL)bold{NSTextField *label=[NSTextField wrappingLabelWithString:text?:@""];label.frame=NSMakeRect(14,y,MAX(50,self.bounds.size.width-36),height);label.textColor=hostText();label.font=bold?[NSFont boldSystemFontOfSize:12]:[NSFont systemFontOfSize:12];[self.document addSubview:label];return label;}
- (NSButton*)button:(NSString*)title y:(CGFloat)y action:(SEL)action represented:(id)value{NSButton *button=[NSButton buttonWithTitle:title target:self action:action];ExtendActionCell *cell=[[ExtendActionCell alloc]initTextCell:title];cell.buttonType=NSButtonTypeMomentaryPushIn;button.cell=cell;button.target=self;button.action=action;button.frame=NSMakeRect(14,y,MAX(60,self.bounds.size.width-36),26);button.identifier=value;[self.document addSubview:button];return button;}
- (void)refresh{NSDictionary *reply=loader_plugin_ui_call(@"ui.snapshot",@{});if(![reply[@"ok"] boolValue])return;NSDictionary *next=reply[@"result"];if((![next[@"generation"] isEqual:self.snapshot[@"generation"]]||![next[@"manager"][@"generation"] isEqual:self.snapshot[@"manager"][@"generation"]])&&!self.editing){self.snapshot=next;[self render];}}
- (NSDictionary*)arguments:(NSString*)identifier{ return @{@"id":identifier,@"generation":self.snapshot[@"generation"]};}
- (void)apply:(NSString*)method values:(NSDictionary*)values{NSDictionary *reply=loader_plugin_ui_call(method,values);if([reply[@"ok"] boolValue]){self.editing=NO;self.snapshot=reply[@"result"];[self render];}else{self.message.stringValue=[NSString stringWithFormat:@"操作未保存：%@。请刷新后重试。",reply[@"error"][@"code"]?:@"UNKNOWN"];[self.document scrollRectToVisible:self.message.frame];}}
- (void)toggleExtend:(NSButton*)sender{[self apply:@"ui.set_extend_visible" values:@{@"generation":self.snapshot[@"generation"],@"visible":(sender.state==NSControlStateValueOn?@YES:@NO)}];}
- (void)toggle:(NSButton*)sender{NSMutableDictionary *args=[[self arguments:sender.identifier] mutableCopy];args[@"visible"]=(sender.state==NSControlStateValueOn?@YES:@NO);[self apply:@"ui.set_visible" values:args];}
- (void)toggleSetting:(NSButton*)sender{NSArray *parts=[sender.identifier componentsSeparatedByString:@"/"];if(parts.count!=2)return;NSMutableDictionary *args=[[self arguments:parts[0]] mutableCopy];args[@"key"]=parts[1];args[@"value"]=sender.state==NSControlStateValueOn?@YES:@NO;[self apply:@"ui.set_setting" values:args];}
- (void)manager:(NSString*)method arguments:(NSDictionary*)arguments{NSDictionary *reply=loader_plugin_manager_call(method,arguments);if(![reply[@"ok"] boolValue]){self.message.stringValue=loader_plugin_manager_error_message(reply[@"error"][@"code"]);return;}self.snapshot=nil;[self refresh];}
- (void)checkUpdates:(id)sender{(void)sender;[self manager:@"manager.check_updates" arguments:@{}];}
- (void)installUpdate:(NSButton*)sender{[self manager:@"manager.install_update" arguments:@{@"id":sender.identifier}];}
- (void)togglePackage:(NSButton*)sender{[self manager:@"manager.set_enabled" arguments:@{@"id":sender.identifier,@"enabled":sender.state==NSControlStateValueOn?@YES:@NO,@"generation":self.snapshot[@"manager"][@"generation"]?:@0}];}
- (void)removePackage:(NSButton*)sender{[self manager:@"manager.remove" arguments:@{@"id":sender.identifier,@"generation":self.snapshot[@"manager"][@"generation"]?:@0}];}
- (void)cancelRemove:(NSButton*)sender{[self manager:@"manager.cancel_remove" arguments:@{@"id":sender.identifier,@"generation":self.snapshot[@"manager"][@"generation"]?:@0}];}
- (void)installPackage:(id)sender{(void)sender;NSOpenPanel *panel=[NSOpenPanel openPanel];panel.canChooseFiles=YES;panel.canChooseDirectories=NO;panel.allowsMultipleSelection=NO;panel.allowedContentTypes=@[UTTypeZIP,[UTType typeWithFilenameExtension:@"sv2ext"]?:UTTypeData];panel.prompt=@"准备安装";panel.message=@"选择插件包。安装后重新启动启用。";[panel beginSheetModalForWindow:self.window completionHandler:^(NSModalResponse response){if(response==NSModalResponseOK)[self manager:@"manager.install_local" arguments:@{@"path":panel.URL.path}];}];}
- (void)installURL:(id)sender{(void)sender;NSAlert *alert=[NSAlert new];alert.messageText=@"从链接安装插件";alert.informativeText=@"输入插件发布信息的 HTTPS 链接。安装后重新启动启用。";NSTextField *field=[[NSTextField alloc]initWithFrame:NSMakeRect(0,0,360,24)];field.placeholderString=@"https://…/plugin-release.json";alert.accessoryView=field;[alert addButtonWithTitle:@"继续"];[alert addButtonWithTitle:@"取消"];[alert beginSheetModalForWindow:self.window completionHandler:^(NSModalResponse response){if(response==NSAlertFirstButtonReturn)[self manager:@"manager.install_url" arguments:@{@"url":field.stringValue}];}];}
- (void)openPanel:(NSButton*)sender{if(loader_extend_open_plugin_panel(sender.identifier.UTF8String)!=0)self.message.stringValue=@"请先显示该插件的右侧面板入口。";}
- (void)changed:(id)sender{(void)sender;self.editing=YES;}
- (void)controlTextDidChange:(NSNotification*)notification{(void)notification;self.editing=YES;}
- (void)reload:(id)sender{(void)sender;self.editing=NO;self.snapshot=nil;[self refresh];}
- (void)save:(NSButton*)sender{NSMutableDictionary *values=[NSMutableDictionary dictionary];NSDictionary *plugin=nil;for(NSDictionary *p in self.snapshot[@"plugins"])if([p[@"id"] isEqual:sender.identifier])plugin=p;for(NSDictionary *field in plugin[@"settings"]){NSControl *control=self.fields[field[@"key"]];NSString *type=field[@"type"];if(!control){values[field[@"key"]]=plugin[@"values"] [field[@"key"]];continue;}if([type isEqual:@"boolean"])values[field[@"key"]]=([(NSButton*)control state]==NSControlStateValueOn?@YES:@NO);else if([type isEqual:@"choice"])values[field[@"key"]]=field[@"choices"][[((NSPopUpButton*)control) indexOfSelectedItem]];else if([type isEqual:@"integer"]){NSString *text=control.stringValue;NSScanner *scanner=[NSScanner scannerWithString:text];long long number;if(![scanner scanLongLong:&number]||!scanner.isAtEnd){self.message.stringValue=@"整数设置无效，尚未保存。";return;}values[field[@"key"]]=@(number);}else values[field[@"key"]]=control.stringValue;}
 NSMutableDictionary *args=[[self arguments:sender.identifier] mutableCopy];args[@"settings"]=values;[self apply:@"ui.set_settings" values:args];}
- (void)action:(NSButton*)sender{NSArray *parts=[sender.identifier componentsSeparatedByString:@"/"];if(parts.count!=2)return;NSMutableDictionary *args=[[self arguments:parts[0]] mutableCopy];args[@"action"]=parts[1];[self apply:@"ui.action" values:args];}
- (CGFloat)renderSettings:(NSDictionary*)plugin atY:(CGFloat)y{
 NSString *identifier=plugin[@"id"];BOOL builtIn=[@[@"opensv2.lua-extend",@"opensv2.javascript-extend",@"opensv2.mcp"] containsObject:identifier];NSDictionary *master=nil;for(NSDictionary *field in plugin[@"settings"])if([field[@"key"] isEqual:@"enabled"]&&[field[@"type"] isEqual:@"boolean"])master=field;
NSString *section=nil;BOOL needsSave=NO;
 for(NSDictionary *field in plugin[@"settings"]){NSString *key=field[@"key"],*type=field[@"type"];if(master&&[key isEqual:@"enabled"])continue;
 if([field[@"section"] isKindOfClass:NSString.class]&&![field[@"section"] isEqual:section]){section=field[@"section"];y+=7;[self label:section y:y height:20 bold:YES];y+=24;}
 id value=plugin[@"values"][key];NSControl *control=nil;CGFloat height=25;
 if([type isEqual:@"boolean"]){NSString *title=field[@"title"];if([field[@"detail"] isKindOfClass:NSString.class])title=[NSString stringWithFormat:@"%@\n%@",title,field[@"detail"]];BOOL immediate=builtIn;NSButton *check=hostSwitch(title,self,immediate?@selector(toggleSetting:):@selector(changed:));check.identifier=[NSString stringWithFormat:@"%@/%@",identifier,key];check.state=[value boolValue]?NSControlStateValueOn:NSControlStateValueOff;control=check;height=switchHeight(title,MAX(60,self.bounds.size.width-36));if(!immediate)needsSave=YES;}
 else{needsSave=YES;[self label:field[@"title"] y:y height:20 bold:NO];y+=23;if([type isEqual:@"choice"]){NSPopUpButton *choice=[[NSPopUpButton alloc]initWithFrame:NSZeroRect pullsDown:NO];for(NSString *option in field[@"choices"]){[choice addItemWithTitle:option];if([option isEqual:value])[choice selectItemAtIndex:choice.numberOfItems-1];}choice.target=self;choice.action=@selector(changed:);control=choice;}else{NSTextField *text=[[NSTextField alloc]initWithFrame:NSZeroRect];text.stringValue=[value isKindOfClass:NSString.class]?value:[value description];text.delegate=(id<NSTextFieldDelegate>)self;control=text;}}
 control.frame=NSMakeRect(14,y,MAX(60,self.bounds.size.width-36),height);[self.document addSubview:control];self.fields[key]=control;y+=height+5;
 }
 if(needsSave){[self button:@"保存设置" y:y action:@selector(save:) represented:identifier];y+=31;}

 return y;
}
- (void)renderPlugin{
 self.fields=[NSMutableDictionary dictionary];CGFloat previous=self.scroll.contentView.bounds.origin.y;self.document=[[ExtendFlippedView alloc]initWithFrame:NSMakeRect(0,0,self.bounds.size.width,100)];CGFloat y=12;NSDictionary *plugin=nil;for(NSDictionary *item in self.snapshot[@"plugins"])if([item[@"id"] isEqual:self.pluginIdentifier])plugin=item;
 if(!plugin){[self label:@"此插件已卸载" y:y height:40 bold:YES];y+=46;}else{
 for(NSDictionary *field in plugin[@"settings"])if([field[@"key"] isEqual:@"enabled"]&&[field[@"type"] isEqual:@"boolean"]){NSButton *master=hostSwitch(field[@"title"],self,@selector(toggleSetting:));master.identifier=[self.pluginIdentifier stringByAppendingString:@"/enabled"];master.frame=NSMakeRect(14,y,MAX(70,self.bounds.size.width-36),23);master.state=[plugin[@"values"][@"enabled"] boolValue]?NSControlStateValueOn:NSControlStateValueOff;[self.document addSubview:master];y+=29;}
 NSDictionary *sidebar=plugin[@"sidebar"];
 for(NSDictionary *row in sidebar[@"rows"]){if([row[@"type"] isEqual:@"button"]){[self button:row[@"text"] y:y action:@selector(action:) represented:[NSString stringWithFormat:@"%@/%@",plugin[@"id"],row[@"id"]]];y+=30;}else{NSTextField *label=[self label:row[@"text"] y:y height:45 bold:NO];NSSize fit=[label sizeThatFits:NSMakeSize(label.frame.size.width,CGFLOAT_MAX)];label.frame=NSMakeRect(label.frame.origin.x,y,label.frame.size.width,MAX(20,fit.height));y+=label.frame.size.height+7;}}
 y=[self renderSettings:plugin atY:y];
 }
 self.message=[self label:@"" y:y height:45 bold:NO];y+=47;self.document.frame=NSMakeRect(0,0,self.bounds.size.width,MAX(y,self.bounds.size.height));self.scroll.documentView=self.document;[self.scroll.contentView scrollToPoint:NSMakePoint(0,MIN(previous,MAX(0,y-self.scroll.contentView.bounds.size.height)))];[self.scroll reflectScrolledClipView:self.scroll.contentView];
}
- (void)render{
 if(self.pluginIdentifier){[self renderPlugin];return;}
 CGFloat previous=self.scroll.contentView.bounds.origin.y;self.document=[[ExtendFlippedView alloc]initWithFrame:NSMakeRect(0,0,self.bounds.size.width,100)];self.fields=[NSMutableDictionary dictionary];CGFloat y=12;
 [self label:@"插件" y:y height:20 bold:YES];y+=25;
 NSButton *entry=hostSwitch(@"显示 Extend 入口",self,@selector(toggleExtend:));entry.frame=NSMakeRect(14,y,MAX(70,self.bounds.size.width-36),23);entry.state=[self.snapshot[@"extendEntryVisible"] boolValue]?NSControlStateValueOn:NSControlStateValueOff;[self.document addSubview:entry];y+=32;
 NSDictionary *manager=self.snapshot[@"manager"];BOOL busy=[manager[@"busy"] boolValue];
 NSButton *check=[self button:@"检查插件更新" y:y action:@selector(checkUpdates:) represented:@"check-updates"];check.enabled=!busy;y+=29;
 NSButton *local=[self button:@"安装新插件…" y:y action:@selector(installPackage:) represented:@"install-package"];local.enabled=!busy;y+=29;
 NSButton *online=[self button:@"从链接安装…" y:y action:@selector(installURL:) represented:@"install-url"];online.enabled=!busy;y+=30;
 NSTextField *status=[self label:manager[@"status"]?:@"" y:y height:50 bold:NO];NSSize fit=[status sizeThatFits:NSMakeSize(status.frame.size.width,CGFLOAT_MAX)];status.frame=NSMakeRect(status.frame.origin.x,y,status.frame.size.width,MAX(20,fit.height));y+=status.frame.size.height+10;
 for(NSDictionary *item in manager[@"pending"]){[self label:[NSString stringWithFormat:@"%@：%@ %@",[item[@"operation"] isEqual:@"remove"]?@"待删除":@"待重启",item[@"name"],item[@"version"]] y:y height:40 bold:NO];y+=43;if([item[@"operation"] isEqual:@"remove"]){NSButton *cancel=[self button:@"取消删除" y:y action:@selector(cancelRemove:) represented:item[@"id"]];cancel.enabled=!busy;y+=30;}}
 for(NSDictionary *item in manager[@"updates"]){NSButton *update=[self button:[NSString stringWithFormat:@"更新 %@ 至 %@",item[@"name"],item[@"version"]] y:y action:@selector(installUpdate:) represented:item[@"id"]];update.enabled=!busy;y+=30;}
 NSMutableSet *registered=[NSMutableSet set];for(NSDictionary *plugin in self.snapshot[@"plugins"])[registered addObject:plugin[@"id"]];
 for(NSDictionary *package in manager[@"installed"])if(![registered containsObject:package[@"id"]]){[self label:package[@"name"] y:y height:20 bold:YES];y+=24;NSButton *enabled=hostSwitch(@"随宿主启动（重启生效）",self,@selector(togglePackage:));enabled.identifier=package[@"id"];enabled.frame=NSMakeRect(14,y,MAX(70,self.bounds.size.width-36),23);enabled.state=[package[@"enabled"] boolValue]?NSControlStateValueOn:NSControlStateValueOff;enabled.enabled=!busy;[self.document addSubview:enabled];y+=27;NSButton *remove=[self button:@"删除插件（重启生效）" y:y action:@selector(removePackage:) represented:package[@"id"]];remove.enabled=!busy;y+=43;}
 for(NSDictionary *plugin in self.snapshot[@"plugins"]){NSString *identifier=plugin[@"id"];
 [self label:plugin[@"name"] y:y height:20 bold:YES];y+=22;
 for(NSDictionary *package in manager[@"installed"])if([package[@"id"] isEqual:identifier]){NSButton *start=hostSwitch(@"随宿主启动（重启生效）",self,@selector(togglePackage:));start.identifier=identifier;start.frame=NSMakeRect(14,y,MAX(70,self.bounds.size.width-36),23);start.state=[package[@"enabled"] boolValue]?NSControlStateValueOn:NSControlStateValueOff;start.enabled=!busy;[self.document addSubview:start];y+=26;NSButton *remove=[self button:@"删除插件（重启生效）" y:y action:@selector(removePackage:) represented:identifier];remove.enabled=!busy;y+=30;}
 NSDictionary *master=nil;for(NSDictionary *field in plugin[@"settings"])if([field[@"key"] isEqual:@"enabled"]&&[field[@"type"] isEqual:@"boolean"])master=field;
 if(master){NSButton *enabled=hostSwitch(@"启用插件",self,@selector(toggleSetting:));enabled.identifier=[identifier stringByAppendingString:@"/enabled"];enabled.frame=NSMakeRect(14,y,MAX(70,self.bounds.size.width-36),23);enabled.state=[plugin[@"values"][@"enabled"] boolValue]?NSControlStateValueOn:NSControlStateValueOff;[self.document addSubview:enabled];y+=26;}
 if(![plugin[@"status"] isEqual:@"已就绪"]&&![plugin[@"status"] isEqual:@"已启用"]){[self label:plugin[@"status"] y:y height:20 bold:NO];y+=23;}
 NSButton *visible=hostSwitch(@"显示右侧面板入口",self,@selector(toggle:));visible.identifier=identifier;visible.frame=NSMakeRect(14,y,MAX(70,self.bounds.size.width-36),23);visible.state=[plugin[@"visible"] boolValue]?NSControlStateValueOn:NSControlStateValueOff;[self.document addSubview:visible];y+=27;
 [self button:@"打开插件面板" y:y action:@selector(openPanel:) represented:identifier];y+=30;

 y+=16;
 }

 self.message=[self label:@"" y:y height:45 bold:NO];y+=47;[self button:@"刷新插件列表" y:y action:@selector(reload:) represented:@"reload"];y+=42;self.document.frame=NSMakeRect(0,0,self.bounds.size.width,MAX(y,self.bounds.size.height));self.scroll.documentView=self.document;[self.scroll.contentView scrollToPoint:NSMakePoint(0,MIN(previous,MAX(0,y-self.scroll.contentView.bounds.size.height)))];[self.scroll reflectScrolledClipView:self.scroll.contentView];
}
@end
NSView *loader_extend_control_view(void){return [[ExtendControlView alloc]initWithFrame:NSMakeRect(0,0,240,600)];}
void loader_extend_control_refresh(NSView *view){[(ExtendControlView*)view refresh];}

NSView *loader_extend_plugin_view(NSString *identifier){ExtendControlView *view=[[ExtendControlView alloc]initWithFrame:NSMakeRect(0,0,240,600)];view.pluginIdentifier=identifier;[view setAccessibilityLabel:identifier];[view render];return view;}

@interface ExtendScriptsView:NSView
@property NSButton *open;
- (void)refresh;
@end
@implementation ExtendScriptsView
- (instancetype)initWithFrame:(NSRect)frame{if((self=[super initWithFrame:frame])){self.appearance=[NSAppearance appearanceNamed:NSAppearanceNameDarkAqua];self.wantsLayer=YES;self.layer.backgroundColor=hostBackground().CGColor;self.open=[NSButton buttonWithTitle:@"打开 Extend 菜单" target:self action:@selector(openCenter:)];ExtendActionCell *cell=[[ExtendActionCell alloc]initTextCell:self.open.title];cell.buttonType=NSButtonTypeMomentaryPushIn;self.open.cell=cell;self.open.target=self;self.open.action=@selector(openCenter:);self.open.frame=NSMakeRect(12,6,MAX(80,frame.size.width-24),24);self.open.autoresizingMask=NSViewWidthSizable;[self addSubview:self.open];[self setAccessibilityElement:YES];[self setAccessibilityRole:NSAccessibilityGroupRole];[self setAccessibilityLabel:@"Extend 菜单入口"];[self setAccessibilityChildren:@[self.open]];}return self;}
- (BOOL)isFlipped{return YES;}
- (void)openCenter:(id)sender{(void)sender;NSDictionary *snapshot=loader_plugin_ui_call(@"ui.snapshot",@{})[@"result"];if(![snapshot[@"extendEntryVisible"] boolValue]){NSDictionary *reply=loader_plugin_ui_call(@"ui.set_extend_visible",@{@"generation":snapshot[@"generation"],@"visible":@YES});if(![reply[@"ok"] boolValue]){NSBeep();return;}}[self refresh];if(loader_extend_open_control_panel()!=0)NSBeep();}
- (void)refresh{}
@end
NSView *loader_extend_scripts_view(void){return [[ExtendScriptsView alloc]initWithFrame:NSMakeRect(0,0,240,36)];}
