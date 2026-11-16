#import <AppKit/AppKit.h>
#import <objc/runtime.h>
#include <stdlib.h>
#include <string.h>
extern BOOL loader_lua_editor_online(void);
static BOOL enabled,rescanned;
static unsigned generation;
static NSTimeInterval lastRequest;
static Class actionClass;
static IMP originalAction;
static void supervise(unsigned,unsigned);
static BOOL rescanTitle(NSString *title) {
    return [@[@"重新扫描",@"重新扫描脚本",@"Rescan",@"Rescan Scripts"] containsObject:title];
}
static BOOL stopTitle(NSString *title) {
    return [@[@"中止所有正在运行的脚本",@"停止所有正在运行的脚本",@"停止所有运行中的脚本",@"Abort All Running Scripts",@"Stop All Running Scripts"] containsObject:title];
}
static void observeMenuAction(id target,SEL selector,id sender) {
    NSString *title=[sender isKindOfClass:NSMenuItem.class]?[((NSMenuItem*)sender).title copy]:nil;
    ((void(*)(id,SEL,id))originalAction)(target,selector,sender);
    if(stopTitle(title)) {
        enabled=NO;generation++;
        fprintf(stderr,"LUA_EDITOR manual_stop_preserved=1\n");
    } else if(rescanTitle(title) || [title isEqualToString:@"Open SV2 Loader Runtime"]) {
        enabled=YES;lastRequest=NSProcessInfo.processInfo.systemUptime;
        unsigned ticket=++generation;
        dispatch_after(dispatch_time(DISPATCH_TIME_NOW,500000000),dispatch_get_main_queue(),^{supervise(0,ticket);});
        if(rescanTitle(title))fprintf(stderr,"LUA_EDITOR rescan_recovery_scheduled=1\n");
    }
}
// JUCE's native menu target uses a normal Objective-C target/action method.
// Observe that entry without altering other menu actions or their arguments.
// Unknown signatures/classes retain the original behavior.
static void observeTarget(NSMenuItem *item) {
    if(actionClass || !item.target || item.action!=NSSelectorFromString(@"menuItemInvoked:"))return;
    Class cls=object_getClass(item.target);Method method=class_getInstanceMethod(cls,item.action);
    if(!method)return;
    NSMethodSignature *signature=[NSMethodSignature signatureWithObjCTypes:method_getTypeEncoding(method)];
    if(signature.numberOfArguments!=3 || strcmp(signature.methodReturnType,@encode(void)) || strcmp([signature getArgumentTypeAtIndex:2],@encode(id)))return;
    IMP previous=method_getImplementation(method);
    if(!class_addMethod(cls,item.action,(IMP)observeMenuAction,method_getTypeEncoding(method)))method_setImplementation(class_getInstanceMethod(cls,item.action),(IMP)observeMenuAction);
    originalAction=previous;actionClass=cls;
    fprintf(stderr,"LUA_EDITOR menu_lifecycle_observer=1\n");
}
static NSMenuItem *findItem(NSMenu *menu,NSString *title) {
    [menu update];
    for(NSMenuItem *item in menu.itemArray){
        if(rescanTitle(item.title))observeTarget(item);
        if([item.title isEqualToString:title])return item;
        if(item.submenu){NSMenuItem *found=findItem(item.submenu,title);if(found)return found;}
    }
    return nil;
}
static void supervise(unsigned attempt,unsigned ticket) {
    if(!enabled || ticket!=generation)return;
    // Opening a document can terminate official script callbacks long after
    // startup. Keep a lightweight heartbeat for the lifetime of this generation;
    // explicit Stop still disables it through the native menu observer.
    if(loader_lua_editor_online()) {
        dispatch_after(dispatch_time(DISPATCH_TIME_NOW,1000000000),dispatch_get_main_queue(),^{supervise(0,ticket);});
        return;
    }
    NSMenuItem *item=findItem(NSApp.mainMenu,@"Open SV2 Loader Runtime");
    NSTimeInterval now=NSProcessInfo.processInfo.systemUptime;
    if(!loader_lua_editor_online() && now-lastRequest>=3 && item.enabled && item.action){
        lastRequest=now;BOOL sent=[NSApp sendAction:item.action to:item.target from:item];
        fprintf(stderr,"LUA_EDITOR autostart_requested=%d\n",sent);
        // The observed action may have scheduled the next generation itself.
        if(ticket!=generation)return;
    }
    if(!item && !rescanned && attempt>=4){
        for(NSString *title in @[@"重新扫描",@"重新扫描脚本",@"Rescan",@"Rescan Scripts"]){
            NSMenuItem *scan=findItem(NSApp.mainMenu,title);
            if(scan.enabled && scan.action){rescanned=YES;[NSApp sendAction:scan.action to:scan.target from:scan];break;}
        }
        if(ticket!=generation)return;
    }
    dispatch_after(dispatch_time(DISPATCH_TIME_NOW,250000000),dispatch_get_main_queue(),^{supervise(attempt<600?attempt+1:600,ticket);});
}
void loader_lua_editor_autostart(void) {
    const char *flag=getenv("OPEN_SV2_AUTO_EDITOR_PROVIDER");
    if(flag && !strcmp(flag,"1") && getenv("OPEN_SV2_LOADER_DATA_DIR")){
        enabled=YES;supervise(0,++generation);
    }
}
