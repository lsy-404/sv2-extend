#import <Foundation/Foundation.h>
#import <WebKit/WebKit.h>
int loader_web_store_tracking_install(void);
BOOL loader_web_store_tracking_supported(void);
void loader_web_store_register(WKWebsiteDataStore *store);
// Main-thread snapshot. nil is unavailable/overflow, never an empty success.
NSArray<WKWebsiteDataStore*> *loader_web_stores_snapshot(void);
