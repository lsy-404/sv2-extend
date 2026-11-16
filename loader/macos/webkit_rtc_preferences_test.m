#import <Foundation/Foundation.h>
#import <WebKit/WebKit.h>
#import <objc/message.h>
#include "webkit_rtc_gate.h"
static int blocked;
int socket_gate_is_blocked(void){return blocked;}
static BOOL enabled(WKPreferences *preferences){return ((BOOL(*)(id,SEL))objc_msgSend)(preferences,NSSelectorFromString(@"_peerConnectionEnabled"));}
static void request(WKPreferences *preferences,BOOL value){((void(*)(id,SEL,BOOL))objc_msgSend)(preferences,NSSelectorFromString(@"_setPeerConnectionEnabled:"),value);}
int main(int argc,const char **argv){@autoreleasepool{
 if(argc!=2)return 64;NSMutableDictionary *checks=[NSMutableDictionary dictionary];checks[@"nativeSPIAvailable"]=@(loader_webkit_rtc_install()==0);if(![checks[@"nativeSPIAvailable"] boolValue])return 2;
 WKWebViewConfiguration *configuration=[WKWebViewConfiguration new];request(configuration.preferences,YES);checks[@"onlineRequestedFeatureEnabled"]=@(enabled(configuration.preferences));
 blocked=1;checks[@"blockRemembersRequestedTrue"]=@(loader_webkit_rtc_prepare(configuration));checks[@"nativeFeatureActuallyDisabled"]=@(!enabled(configuration.preferences));
 request(configuration.preferences,YES);checks[@"callerCannotReenableWhileBlocked"]=@(!enabled(configuration.preferences));
 blocked=0;checks[@"allowRestoresRequestedTrue"]=@(loader_webkit_rtc_prepare(configuration)&&enabled(configuration.preferences));
 blocked=1;loader_webkit_rtc_prepare(configuration);request(configuration.preferences,NO);blocked=0;checks[@"explicitUserDisablePreserved"]=@(!loader_webkit_rtc_prepare(configuration)&&!enabled(configuration.preferences));
 request(configuration.preferences,YES);blocked=1;loader_webkit_rtc_prepare(configuration);checks[@"repeatedBlockDoesNotLoseWantedTrue"]=@(loader_webkit_rtc_prepare(configuration));blocked=0;checks[@"repeatedTransitionRestores"]=@(loader_webkit_rtc_prepare(configuration)&&enabled(configuration.preferences));
 WKWebViewConfiguration *other=[WKWebViewConfiguration new];request(other.preferences,NO);blocked=1;loader_webkit_rtc_prepare(other);blocked=0;checks[@"separatePreferencesIndependent"]=@(!loader_webkit_rtc_prepare(other)&&enabled(configuration.preferences));
 BOOL passed=YES;for(NSNumber *value in checks.allValues)passed&=value.boolValue;NSDictionary *report=@{@"checks":checks,@"passed":@(passed),@"scope":@"Production RTC preference component with real WebKit SPI. Native values, restoration and caller override policy; not renderer/old connection revocation."};NSData *data=[NSJSONSerialization dataWithJSONObject:report options:NSJSONWritingPrettyPrinted error:NULL];[data writeToFile:@(argv[1]) atomically:YES];puts([[NSString alloc]initWithData:data encoding:NSUTF8StringEncoding].UTF8String);return passed?0:1;
}}
