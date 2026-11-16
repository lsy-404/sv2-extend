#import <Foundation/Foundation.h>
// Core-only bootstrap registration; never inspects private plugin journals.
void loader_startup_register_bootstrap_owner(const char *owner);
// nil means malformed/unreadable policy, never an empty required-owner set.
NSArray<NSString*> *loader_startup_required_bootstrap_owners(void);
