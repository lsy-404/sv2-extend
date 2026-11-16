#import <AppKit/AppKit.h>
// Computation-only WebKit test host: never creates or activates a native window.
int main(void){@autoreleasepool{[NSApplication sharedApplication];[NSApp setActivationPolicy:NSApplicationActivationPolicyProhibited];[NSApp run];}return 0;}
