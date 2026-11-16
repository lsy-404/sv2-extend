// Track application completion delivery separately from NSURLSessionTask.state.
// Only small metadata/tokens persist; no request bodies or account/session data.
#import <Foundation/Foundation.h>
#import <objc/runtime.h>
#include <stdint.h>
#include <string.h>
static NSRecursiveLock *metadataLock;
static NSMapTable<NSURLSessionTask*,NSMutableDictionary*> *records;
static NSMutableDictionary<NSString*,NSMutableDictionary*> *pending;
static NSMutableSet<NSString*> *installed;
static BOOL uncertain;
static _Thread_local unsigned factoryDepth;
struct DelegateFrame {__unsafe_unretained NSURLSessionTask *task;struct DelegateFrame *previous;};
static _Thread_local struct DelegateFrame *frames;
static void initialize(void){static dispatch_once_t once;dispatch_once(&once,^{metadataLock=[NSRecursiveLock new];records=[NSMapTable weakToStrongObjectsMapTable];pending=[NSMutableDictionary dictionary];installed=[NSMutableSet set];});}
static NSString *methodKey(Class cls,SEL selector){return [NSString stringWithFormat:@"%p:%s",cls,sel_getName(selector)];}
static void acknowledge(NSMutableDictionary *record){if(!record)return;[metadataLock lock];record[@"acknowledged"]=@YES;[pending removeObjectForKey:record[@"token"]];[metadataLock unlock];}
static void acknowledgeDelegate(NSURLSessionTask *task){[metadataLock lock];NSMutableDictionary *record=[records objectForKey:task];if([record[@"kind"] isEqual:@"delegate"])acknowledge(record);[metadataLock unlock];}
static BOOL instrumentDelegate(id delegate){
 if(!delegate)return YES;initialize();Class cls=object_getClass(delegate);SEL selector=@selector(URLSession:task:didCompleteWithError:);NSString *key=methodKey(cls,selector);[metadataLock lock];if([installed containsObject:key]){[metadataLock unlock];return YES;}
 for(Class c=cls;c;c=class_getSuperclass(c))if(c==NSProxy.class){[metadataLock unlock];return NO;}
 Method method=class_getInstanceMethod(cls,selector);// Never call a custom respondsToSelector: implementation under this lock.
 if(!method && class_getMethodImplementation(cls,@selector(respondsToSelector:))!=class_getMethodImplementation(NSObject.class,@selector(respondsToSelector:))){[metadataLock unlock];return NO;}
 IMP original=method?method_getImplementation(method):NULL;
 if(method){NSMethodSignature *signature=[NSMethodSignature signatureWithObjCTypes:method_getTypeEncoding(method)];if(signature.numberOfArguments!=5 || strcmp(signature.methodReturnType,@encode(void)) || [signature getArgumentTypeAtIndex:2][0]!='@' || [signature getArgumentTypeAtIndex:3][0]!='@' || [signature getArgumentTypeAtIndex:4][0]!='@'){[metadataLock unlock];return NO;}}
 IMP replacement=imp_implementationWithBlock(^(id object,NSURLSession *session,NSURLSessionTask *task,NSError *error){
  BOOL nested=NO;for(struct DelegateFrame *frame=frames;frame;frame=frame->previous)if(frame->task==task){nested=YES;break;}
  struct DelegateFrame frame={task,frames};frames=&frame;
  @try{if(original)((void(*)(id,SEL,id,id,id))original)(object,selector,session,task,error);}
  @finally{frames=frame.previous;if(!nested)acknowledgeDelegate(task);}
 });
 const char *types=method?method_getTypeEncoding(method):"v@:@@@";
 if(!class_addMethod(cls,selector,replacement,types))method_setImplementation(class_getInstanceMethod(cls,selector),replacement);
 [installed addObject:key];[metadataLock unlock];return YES;
}
static BOOL ownMethod(Class cls,SEL selector){unsigned count=0;Method *methods=class_copyMethodList(cls,&count);BOOL found=NO;for(unsigned i=0;i<count;i++)if(method_getName(methods[i])==selector){found=YES;break;}free(methods);return found;}
static void instrumentTaskDelegate(Class cls){
 SEL selector=@selector(setDelegate:);if(!ownMethod(cls,selector))return;NSString *key=methodKey(cls,selector);[metadataLock lock];if([installed containsObject:key]){[metadataLock unlock];return;}
 Method method=class_getInstanceMethod(cls,selector);IMP original=method_getImplementation(method);
 IMP replacement=imp_implementationWithBlock(^(NSURLSessionTask *task,id delegate){BOOL known=instrumentDelegate(delegate);[metadataLock lock];NSMutableDictionary *record=[records objectForKey:task];if(record && ![record[@"kind"] isEqual:@"block"]){record[@"kind"]=delegate?@"delegate":record[@"baseKind"];record[@"unknown"]=delegate?@(!known || ![record[@"serialQueue"] boolValue]):record[@"baseUnknown"];}[metadataLock unlock];((void(*)(id,SEL,id))original)(task,selector,delegate);});
 method_setImplementation(method,replacement);[installed addObject:key];[metadataLock unlock];
}
static void registerTask(NSURLSessionTask *task,NSURLSession *session,NSMutableDictionary *record,BOOL handler){
 if(!task)return;initialize();id delegate=session.delegate;BOOL known=instrumentDelegate(delegate);
 [metadataLock lock];record[@"kind"]=handler?@"block":(delegate?@"delegate":@"none");record[@"acknowledged"]=@NO;record[@"serialQueue"]=@(session.delegateQueue.maxConcurrentOperationCount==1);
 if(!known || (!handler && delegate && session.delegateQueue.maxConcurrentOperationCount!=1))record[@"unknown"]=@YES;
 record[@"baseKind"]=record[@"kind"];record[@"baseUnknown"]=record[@"unknown"]?:@NO;
 [records setObject:record forKey:task];for(Class cls=object_getClass(task);cls && cls!=NSObject.class;cls=class_getSuperclass(cls))instrumentTaskDelegate(cls);[metadataLock unlock];
}
static NSMutableDictionary *newRecord(void){return [@{@"token":NSUUID.UUID.UUIDString} mutableCopy];}
void loader_urlsession_callback_resumed(NSURLSessionTask *task){
 initialize();[metadataLock lock];NSMutableDictionary *record=[records objectForKey:task];
 if(!record || [record[@"unknown"] boolValue])uncertain=YES;
 else if(![record[@"kind"] isEqual:@"none"] && ![record[@"acknowledged"] boolValue]){if(pending.count>=4096)uncertain=YES;else pending[record[@"token"]]=record;}
 [metadataLock unlock];
}
uint32_t loader_urlsession_callbacks_pending(void){initialize();[metadataLock lock];uint32_t count=(uint32_t)pending.count+(uncertain?1:0);[metadataLock unlock];return count;}
static void installFactory(Class cls,SEL selector,unsigned arity,BOOL handler){
 if(!ownMethod(cls,selector))return;NSString *key=methodKey(cls,selector);[metadataLock lock];if([installed containsObject:key]){[metadataLock unlock];return;}
 Method method=class_getInstanceMethod(cls,selector);NSMethodSignature *signature=[NSMethodSignature signatureWithObjCTypes:method_getTypeEncoding(method)];if(signature.numberOfArguments!=arity+2 || signature.methodReturnType[0]!='@'){[metadataLock unlock];return;}IMP original=method_getImplementation(method);IMP replacement=NULL;
 if(handler && arity==2)replacement=imp_implementationWithBlock(^id(NSURLSession *session,id argument,void(^completion)(id,id,id)){
  if(factoryDepth)return ((id(*)(id,SEL,id,id))original)(session,selector,argument,completion);
  NSMutableDictionary *record=newRecord();void(^wrapped)(id,id,id)=completion?^(id a,id b,id c){@try{completion(a,b,c);}@finally{acknowledge(record);}}:nil;
  factoryDepth++;id task=nil;@try{task=((id(*)(id,SEL,id,id))original)(session,selector,argument,wrapped);}@finally{factoryDepth--;}
  registerTask(task,session,record,completion!=nil);return task;
 });
 if(handler && arity==3)replacement=imp_implementationWithBlock(^id(NSURLSession *session,id a,id b,void(^completion)(id,id,id)){
  if(factoryDepth)return ((id(*)(id,SEL,id,id,id))original)(session,selector,a,b,completion);
  NSMutableDictionary *record=newRecord();void(^wrapped)(id,id,id)=completion?^(id x,id y,id z){@try{completion(x,y,z);}@finally{acknowledge(record);}}:nil;
  factoryDepth++;id task=nil;@try{task=((id(*)(id,SEL,id,id,id))original)(session,selector,a,b,wrapped);}@finally{factoryDepth--;}
  registerTask(task,session,record,completion!=nil);return task;
 });
 if(!handler && arity==1)replacement=imp_implementationWithBlock(^id(NSURLSession *session,id argument){
  if(factoryDepth)return ((id(*)(id,SEL,id))original)(session,selector,argument);factoryDepth++;id task=nil;@try{task=((id(*)(id,SEL,id))original)(session,selector,argument);}@finally{factoryDepth--;}registerTask(task,session,newRecord(),NO);return task;
 });
 if(!handler && arity==2)replacement=imp_implementationWithBlock(^id(NSURLSession *session,id a,id b){
  if(factoryDepth)return ((id(*)(id,SEL,id,id))original)(session,selector,a,b);factoryDepth++;id task=nil;@try{task=((id(*)(id,SEL,id,id))original)(session,selector,a,b);}@finally{factoryDepth--;}registerTask(task,session,newRecord(),NO);return task;
 });
 if(replacement){method_setImplementation(method,replacement);[installed addObject:key];}[metadataLock unlock];
}
static BOOL isSession(Class cls){for(Class c=cls;c;c=class_getSuperclass(c))if(c==NSURLSession.class)return YES;return NO;}
static void installSessionClass(Class cls){
 for(Class c=cls;c && isSession(c);c=class_getSuperclass(c)){
  for(NSString *name in @[@"dataTaskWithURL:",@"dataTaskWithRequest:",@"downloadTaskWithURL:",@"downloadTaskWithRequest:",@"downloadTaskWithResumeData:",@"uploadTaskWithResumeData:",@"uploadTaskWithStreamedRequest:",@"webSocketTaskWithURL:",@"webSocketTaskWithRequest:"])installFactory(c,NSSelectorFromString(name),1,NO);
  for(NSString *name in @[@"uploadTaskWithRequest:fromData:",@"uploadTaskWithRequest:fromFile:",@"webSocketTaskWithURL:protocols:"])installFactory(c,NSSelectorFromString(name),2,NO);
  for(NSString *name in @[@"dataTaskWithURL:completionHandler:",@"dataTaskWithRequest:completionHandler:",@"downloadTaskWithURL:completionHandler:",@"downloadTaskWithRequest:completionHandler:",@"downloadTaskWithResumeData:completionHandler:",@"uploadTaskWithResumeData:completionHandler:"])installFactory(c,NSSelectorFromString(name),2,YES);
  for(NSString *name in @[@"uploadTaskWithRequest:fromData:completionHandler:",@"uploadTaskWithRequest:fromFile:completionHandler:"])installFactory(c,NSSelectorFromString(name),3,YES);
 }
}
int loader_urlsession_callbacks_install(void){
 initialize();SEL selector=@selector(sessionWithConfiguration:delegate:delegateQueue:);Class meta=object_getClass(NSURLSession.class);Method method=class_getInstanceMethod(meta,selector);NSString *key=methodKey(meta,selector);
 [metadataLock lock];if(![installed containsObject:key]){IMP original=method_getImplementation(method);IMP replacement=imp_implementationWithBlock(^id(id cls,id config,id delegate,id queue){instrumentDelegate(delegate);id session=((id(*)(id,SEL,id,id,id))original)(cls,selector,config,delegate,queue);installSessionClass(object_getClass(session));return session;});method_setImplementation(method,replacement);[installed addObject:key];}[metadataLock unlock];
 unsigned count=0;Class *classes=objc_copyClassList(&count);for(unsigned i=0;i<count;i++)if(isSession(classes[i]))installSessionClass(classes[i]);free(classes);return 0;
}
