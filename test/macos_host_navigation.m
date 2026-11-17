#import <AppKit/AppKit.h>
#import <objc/runtime.h>
#include <assert.h>
#include "../loader/macos/host_navigation.m"
static int originals,decision;
static BOOL mock_open(id self,SEL selector,NSURL *url){(void)self;(void)selector;(void)url;++originals;return YES;}
static int callback(const char *url,void *context){(void)context;assert(strcmp(url,"fixture:test")==0);return decision;}
int main(void){@autoreleasepool {
 Method method=class_getInstanceMethod(NSWorkspace.class,@selector(openURL:));assert(method);method_setImplementation(method,(IMP)mock_open);
 assert(loader_host_navigation_install()==0);assert(loader_register_host_navigation_handler("fixture",callback,NULL)==0);
 NSURL *url=[NSURL URLWithString:@"fixture:test"];decision=1;assert([NSWorkspace.sharedWorkspace openURL:url]&&originals==0);
 decision=-1;assert(![NSWorkspace.sharedWorkspace openURL:url]&&originals==0);
 decision=0;assert([NSWorkspace.sharedWorkspace openURL:url]&&originals==1);
 assert(loader_unregister_host_navigation_handler("fixture",callback,NULL)==0);assert([NSWorkspace.sharedWorkspace openURL:url]&&originals==2);
 }}
