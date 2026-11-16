// Weak ownership observation: do not prolong a private store's cookie lifetime.
#import "web_store_registry.h"
#import <objc/runtime.h>
static NSLock *registryLock;
static NSHashTable<WKWebsiteDataStore*> *stores;
static BOOL installed,overflow;
static BOOL signature(Method method,NSUInteger count,char result){if(!method)return NO;NSMethodSignature *s=[NSMethodSignature signatureWithObjCTypes:method_getTypeEncoding(method)];if(s.numberOfArguments!=count||s.methodReturnType[0]!=result)return NO;return count==2||[s getArgumentTypeAtIndex:2][0]=='@';}
void loader_web_store_register(WKWebsiteDataStore *store){
 if(!registryLock||![store isKindOfClass:WKWebsiteDataStore.class])return;
 [registryLock lock];if(![stores containsObject:store]){if(stores.allObjects.count>=256)overflow=YES;else [stores addObject:store];}[registryLock unlock];
}
BOOL loader_web_store_tracking_supported(void){if(!registryLock)return NO;[registryLock lock];BOOL result=installed&&!overflow;[registryLock unlock];return result;}
NSArray<WKWebsiteDataStore*> *loader_web_stores_snapshot(void){
 if(!NSThread.isMainThread||!registryLock)return nil;[registryLock lock];NSArray *snapshot=installed&&!overflow?stores.allObjects:nil;[registryLock unlock];return snapshot;
}
static void factory(SEL selector,NSUInteger count){
 Method method=class_getClassMethod(WKWebsiteDataStore.class,selector);IMP previous=method_getImplementation(method);
 if(count==2)method_setImplementation(method,imp_implementationWithBlock(^id(id cls){id result=((id(*)(id,SEL))previous)(cls,selector);loader_web_store_register(result);return result;}));
 else method_setImplementation(method,imp_implementationWithBlock(^id(id cls,id value){id result=((id(*)(id,SEL,id))previous)(cls,selector,value);loader_web_store_register(result);return result;}));
}
int loader_web_store_tracking_install(void){
 if(!NSThread.isMainThread)return -1;if(installed)return 0;
 SEL ordinary=@selector(defaultDataStore),ephemeral=@selector(nonPersistentDataStore),named=NSSelectorFromString(@"dataStoreForIdentifier:"),privateFactory=NSSelectorFromString(@"_storeWithConfiguration:"),setter=@selector(setWebsiteDataStore:);
 if(!signature(class_getClassMethod(WKWebsiteDataStore.class,ordinary),2,'@')||!signature(class_getClassMethod(WKWebsiteDataStore.class,ephemeral),2,'@')||!signature(class_getClassMethod(WKWebsiteDataStore.class,named),3,'@')||!signature(class_getInstanceMethod(WKWebViewConfiguration.class,setter),3,'v'))return -1;
 registryLock=[NSLock new];stores=[NSHashTable weakObjectsHashTable];factory(ordinary,2);factory(ephemeral,2);factory(named,3);
 if(signature(class_getClassMethod(WKWebsiteDataStore.class,privateFactory),3,'@'))factory(privateFactory,3);
 Method method=class_getInstanceMethod(WKWebViewConfiguration.class,setter);IMP previous=method_getImplementation(method);
 method_setImplementation(method,imp_implementationWithBlock(^(WKWebViewConfiguration *config,WKWebsiteDataStore *store){((void(*)(id,SEL,id))previous)(config,setter,store);loader_web_store_register(config.websiteDataStore);}));
 installed=YES;return 0;
}
