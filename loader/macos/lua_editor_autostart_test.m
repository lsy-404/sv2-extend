#import <AppKit/AppKit.h>
static BOOL testOnline;
BOOL loader_lua_editor_online(void){return testOnline;}
#include "lua_editor_autostart.m"
@interface LifecycleTarget:NSObject
@property unsigned starts;
-(void)menuItemInvoked:(id)sender;
@end
@implementation LifecycleTarget
-(void)menuItemInvoked:(NSMenuItem*)sender {if([sender.title isEqualToString:@"Open SV2 Loader Runtime"]){self.starts++;testOnline=YES;}else testOnline=NO;}
@end
int main(){@autoreleasepool{
 [NSApplication sharedApplication];NSMenu *menu=[NSMenu new];LifecycleTarget *target=[LifecycleTarget new];
 for(NSString *title in @[@"Rescan Scripts",@"Open SV2 Loader Runtime",@"Abort All Running Scripts"]){NSMenuItem *item=[[NSMenuItem alloc]initWithTitle:title action:@selector(menuItemInvoked:) keyEquivalent:@""];item.target=target;[menu addItem:item];}NSApp.mainMenu=menu;
 enabled=YES;generation=1;lastRequest=0;supervise(600,1);if(target.starts!=1 || !testOnline)return 1;
 supervise(600,1);if(target.starts!=1)return 2;
 NSMenuItem *stop=[menu itemWithTitle:@"Abort All Running Scripts"];[NSApp sendAction:stop.action to:stop.target from:stop];if(enabled || testOnline)return 3;
 supervise(600,generation);if(target.starts!=1)return 4;
 NSMenuItem *scan=[menu itemWithTitle:@"Rescan Scripts"];[NSApp sendAction:scan.action to:scan.target from:scan];if(!enabled)return 5;
 lastRequest=0;supervise(600,generation);if(target.starts!=2 || !testOnline)return 6;
 puts("6 lifecycle checks passed, including recovery at former attempt limit");
}return 0;}
