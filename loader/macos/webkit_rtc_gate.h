#import <WebKit/WebKit.h>
int loader_webkit_rtc_install(void);
BOOL loader_webkit_rtc_prepare(WKWebViewConfiguration *configuration);
void loader_webkit_rtc_register(WKWebView *view,BOOL requested);
void loader_webkit_rtc_update(WKWebView *view);
