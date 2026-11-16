#import <Foundation/Foundation.h>
// Host-resident management. Lifecycle mutations are main-thread only; no dlclose.
NSDictionary *loader_plugin_manager_snapshot(void);
NSDictionary *loader_plugin_manager_call(NSString *method,NSDictionary *arguments);
NSString *loader_plugin_manager_error_message(NSString *code);
void loader_apply_pending_plugin_updates(void);
