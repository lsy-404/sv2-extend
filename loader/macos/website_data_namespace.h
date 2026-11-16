#import <WebKit/WebKit.h>
// Opt-in generic storage isolation. No account or session business logic.
int loader_website_data_namespace_install(const char *root);
WKWebsiteDataStore *loader_website_data_namespace_map(WKWebsiteDataStore *store);
