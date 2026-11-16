// WebKit's native setting is checked by RTCPeerConnection::create, including
// cached JS constructors. It does NOT revoke existing peer connections. Keep
// WEBRTC/WEBKIT unverified until their native lifetime is controlled and proved.
#import "webkit_rtc_gate.h"
#import <objc/runtime.h>
#include "socket_gate.h"
static NSMapTable<WKPreferences*,NSNumber*> *requestedValues;
static IMP getter,setter;
static SEL getSelector,setSelector;
static BOOL supported;
static BOOL desired(WKPreferences *preferences){NSNumber *value=[requestedValues objectForKey:preferences];return value?value.boolValue:((BOOL(*)(id,SEL))getter)(preferences,getSelector);}
static void apply(WKPreferences *preferences,BOOL requested){if(!supported||!preferences)return;[requestedValues setObject:@(requested) forKey:preferences];((void(*)(id,SEL,BOOL))setter)(preferences,setSelector,socket_gate_is_blocked()?NO:requested);}
int loader_webkit_rtc_install(void){
 if(supported)return 0;if(!NSThread.isMainThread)return -1;
 getSelector=NSSelectorFromString(@"_peerConnectionEnabled");setSelector=NSSelectorFromString(@"_setPeerConnectionEnabled:");
 Method get=class_getInstanceMethod(WKPreferences.class,getSelector),set=class_getInstanceMethod(WKPreferences.class,setSelector);if(!get||!set)return -1;
 NSMethodSignature *g=[NSMethodSignature signatureWithObjCTypes:method_getTypeEncoding(get)],*s=[NSMethodSignature signatureWithObjCTypes:method_getTypeEncoding(set)];
 if(g.numberOfArguments!=2||s.numberOfArguments!=3||strcmp(s.methodReturnType,"v")||!strchr("Bc",g.methodReturnType[0])||!strchr("Bc",[s getArgumentTypeAtIndex:2][0]))return -1;
 getter=method_getImplementation(get);setter=method_getImplementation(set);requestedValues=[NSMapTable weakToStrongObjectsMapTable];supported=YES;
 method_setImplementation(set,imp_implementationWithBlock(^(WKPreferences *preferences,BOOL requested){apply(preferences,requested);}));
 fprintf(stderr,"WEBKIT_RTC native-new-connection-guard=1 existing-revocation-verified=0\n");return 0;
}
BOOL loader_webkit_rtc_prepare(WKWebViewConfiguration *configuration){if(!supported)return NO;BOOL requested=desired(configuration.preferences);apply(configuration.preferences,requested);return requested;}
void loader_webkit_rtc_register(WKWebView *view,BOOL requested){if(supported&&view)apply(view.configuration.preferences,requested);}
void loader_webkit_rtc_update(WKWebView *view){if(supported&&view){WKPreferences *preferences=view.configuration.preferences;apply(preferences,desired(preferences));}}
