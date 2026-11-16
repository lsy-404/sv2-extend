#ifndef LOADER_VOICE_NATIVE_INTERNAL_H
#define LOADER_VOICE_NATIVE_INTERNAL_H
#import <Foundation/Foundation.h>
#include <stdint.h>
// Private supported-build implementation; plugins use capability methods.
NSDictionary *loader_voice_identity(NSArray<NSString*> *fields);
NSDictionary *loader_voice_reference_identity(void *reference);
BOOL loader_voice_catalog_initialize(intptr_t slide);
NSDictionary *loader_voice_catalog_request(NSString *identifier,void *reference);
#endif
