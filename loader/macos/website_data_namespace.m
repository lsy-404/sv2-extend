#import "website_data_namespace.h"
#import <Foundation/Foundation.h>
#import <objc/runtime.h>
#include <CommonCrypto/CommonDigest.h>
static NSString *namespaceRoot;
static NSMutableSet<NSUUID*> *mappedIdentifiers;
static IMP originalNamed,originalDefault;
static _Thread_local BOOL creating;
static WKWebsiteDataStore *named(NSUUID *identifier) API_AVAILABLE(macos(14.0));
static WKWebsiteDataStore *named(NSUUID *identifier) {
    if(creating)return ((id(*)(id,SEL,id))originalNamed)(WKWebsiteDataStore.class,@selector(dataStoreForIdentifier:),identifier);
    NSUUID *mapped=nil;
    @synchronized(mappedIdentifiers) {
        if(identifier && [mappedIdentifiers containsObject:identifier])mapped=identifier;
        else {
            NSString *key=[NSString stringWithFormat:@"OpenSV2.WebsiteData.v1\n%@\n%@",namespaceRoot,identifier.UUIDString?:@"default"];
            NSData *data=[key dataUsingEncoding:NSUTF8StringEncoding];unsigned char digest[CC_SHA256_DIGEST_LENGTH];
            CC_SHA256(data.bytes,(CC_LONG)data.length,digest);digest[6]=(digest[6]&15)|128;digest[8]=(digest[8]&63)|128;
            mapped=[[NSUUID alloc]initWithUUIDBytes:digest];[mappedIdentifiers addObject:mapped];
        }
    }
    creating=YES;
    @try {return ((id(*)(id,SEL,id))originalNamed)(WKWebsiteDataStore.class,@selector(dataStoreForIdentifier:),mapped);}
    @finally {creating=NO;}
}
WKWebsiteDataStore *loader_website_data_namespace_map(WKWebsiteDataStore *store) {
    if(!namespaceRoot || !store.isPersistent)return store;
    if(@available(macOS 14.0,*))return named(store.identifier);
    return store;
}
int loader_website_data_namespace_install(const char *root) {
    if(!root || !NSThread.isMainThread)return -1;
    if(@available(macOS 14.0,*)) {} else return -1;
    NSString *path=[[NSString stringWithUTF8String:root] stringByStandardizingPath].stringByResolvingSymlinksInPath;
    BOOL directory=NO;if(!path.isAbsolutePath || ![NSFileManager.defaultManager fileExistsAtPath:path isDirectory:&directory] || !directory)return -1;
    if(namespaceRoot)return [namespaceRoot isEqual:path]?0:-1;
    Method defaults=class_getClassMethod(WKWebsiteDataStore.class,@selector(defaultDataStore));
    Method identifiers=class_getClassMethod(WKWebsiteDataStore.class,@selector(dataStoreForIdentifier:));
    Method setter=class_getInstanceMethod(WKWebViewConfiguration.class,@selector(setWebsiteDataStore:));
    if(!defaults || !identifiers || !setter)return -1;
    namespaceRoot=[path copy];mappedIdentifiers=[NSMutableSet set];
    originalDefault=method_getImplementation(defaults);originalNamed=method_getImplementation(identifiers);
    IMP previousSetter=method_getImplementation(setter);
    method_setImplementation(defaults,imp_implementationWithBlock(^id(id cls){
        if(creating)return ((id(*)(id,SEL))originalDefault)(cls,@selector(defaultDataStore));
        if(@available(macOS 14.0,*))return named(nil);
        return ((id(*)(id,SEL))originalDefault)(cls,@selector(defaultDataStore));
    }));
    method_setImplementation(identifiers,imp_implementationWithBlock(^id(id cls,NSUUID *identifier){
        (void)cls;
        // Preserve WebKit's invalid-identifier behavior; never map nil to default.
        uuid_t bytes={0},zero={0};if(identifier)[identifier getUUIDBytes:bytes];
        if(!identifier || !memcmp(bytes,zero,sizeof bytes))return ((id(*)(id,SEL,id))originalNamed)(WKWebsiteDataStore.class,@selector(dataStoreForIdentifier:),identifier);
        if(@available(macOS 14.0,*))return named(identifier);
        return ((id(*)(id,SEL,id))originalNamed)(WKWebsiteDataStore.class,@selector(dataStoreForIdentifier:),identifier);
    }));
    method_setImplementation(setter,imp_implementationWithBlock(^(WKWebViewConfiguration *config,WKWebsiteDataStore *store){
        ((void(*)(id,SEL,id))previousSetter)(config,@selector(setWebsiteDataStore:),loader_website_data_namespace_map(store));
    }));
    fprintf(stderr,"WEBSITE_DATA_NAMESPACE enabled=1 public-webkit-profiles=1\n");
    return 0;
}
