#import <Foundation/Foundation.h>
#include "loader_api.h"
// Private transport token. No plugin ABI change; public calls use capability JSON.
@interface ResidentHTTPCancellation : NSObject
@property(atomic) BOOL cancelled;
@end
int resident_http_request(const char *url,const char *method,NSDictionary *headers,NSData *payload,ResidentHTTPCancellation *cancellation,loader_http_completion completion,void *context);
