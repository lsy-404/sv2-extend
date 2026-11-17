#import <AppKit/AppKit.h>
#import <objc/runtime.h>
#include "host_navigation_registry.h"

static navigation_registry registry;
static BOOL (*original_open_url)(id,SEL,NSURL *);

int loader_register_host_navigation_handler(const char *owner,loader_host_navigation_handler handler,void *context){
 if(!NSThread.isMainThread)return LOADER_ERR_INVALID;
 return navigation_register(&registry,owner,handler,context);
}
int loader_unregister_host_navigation_handler(const char *owner,loader_host_navigation_handler handler,void *context){
 if(!NSThread.isMainThread)return LOADER_ERR_INVALID;
 return navigation_unregister(&registry,owner,handler,context);
}
static BOOL navigation_open_url(id object,SEL selector,NSURL *url){
 if(!url)return original_open_url(object,selector,url);
 __block int decision=0;
 void (^invoke)(void)=^{const char *value=url.absoluteString.UTF8String;if(value)decision=navigation_dispatch(&registry,value);};
 if(NSThread.isMainThread)invoke();else dispatch_sync(dispatch_get_main_queue(),invoke);
 if(decision==1)return YES;
 if(decision<0)return NO;
 return original_open_url(object,selector,url);
}
int loader_host_navigation_install(void){
 if(!NSThread.isMainThread)return LOADER_ERR_INVALID;
 Class cls=NSWorkspace.class;SEL selector=@selector(openURL:);Method method=class_getInstanceMethod(cls,selector);
 if(!method||method_getNumberOfArguments(method)!=3)return LOADER_ERR_NOT_SUPPORTED;
 char result[8]={0},argument[8]={0};method_getReturnType(method,result,sizeof result);method_getArgumentType(method,2,argument,sizeof argument);
 if((result[0]!='B'&&result[0]!='c')||argument[0]!='@')return LOADER_ERR_NOT_SUPPORTED;
 IMP old=method_getImplementation(method);if(!old)return LOADER_ERR_NOT_SUPPORTED;
 original_open_url=(BOOL (*)(id,SEL,NSURL *))old;
 if(!class_addMethod(cls,selector,(IMP)navigation_open_url,method_getTypeEncoding(method)))method_setImplementation(method,(IMP)navigation_open_url);
 return LOADER_OK;
}
