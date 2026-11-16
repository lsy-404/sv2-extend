#import <Foundation/Foundation.h>
#include "loader_api.h"
#include "socket_gate.h"
#include <arpa/inet.h>
#include <unistd.h>
static dispatch_semaphore_t slots;
@interface LoaderJSONListener : NSObject
@property int fd;
@property BOOL active;
@property NSString *token,*discovery;
@property loader_http_json_handler handler;
@property void *context;
@end
@implementation LoaderJSONListener @end
static NSMutableDictionary<NSString*,LoaderJSONListener*> *listeners;

@interface LoaderJSONJob : NSObject
@property dispatch_semaphore_t completed;
@property NSData *response;
@end
@implementation LoaderJSONJob
@end
static void jobComplete(const char *json,size_t length,void *context) {
    LoaderJSONJob *job=CFBridgingRelease(context);
    if(json && length<=2*1024*1024)job.response=[NSData dataWithBytes:json length:length];
    dispatch_semaphore_signal(job.completed);
}
static BOOL writeBytes(int fd,const void *bytes,size_t length) {const char *p=bytes;while(length){ssize_t n=socket_gate_send(fd,p,length,0);if(n<=0)return NO;p+=n;length-=(size_t)n;}return YES;}
static void httpReply(int fd,int code,NSData *data) {
    NSData *body=data?:[NSData data];NSString *header=[NSString stringWithFormat:@"HTTP/1.1 %d Response\r\nContent-Type: application/json\r\nCache-Control: no-store\r\nConnection: close\r\nContent-Length: %lu\r\n\r\n",code,(unsigned long)body.length];
    writeBytes(fd,header.UTF8String,[header lengthOfBytesUsingEncoding:NSUTF8StringEncoding]);writeBytes(fd,body.bytes,body.length);
}
static void handleClient(int fd,NSString *token,loader_http_json_handler handler,void *handlerContext,LoaderJSONListener *state) {
    struct timeval timeout={5,0};setsockopt(fd,SOL_SOCKET,SO_RCVTIMEO,&timeout,sizeof timeout);
    NSMutableData *header=[NSMutableData data];char byte;
    while(header.length<16384){if(recv(fd,&byte,1,0)!=1)return;[header appendBytes:&byte length:1];if(header.length>=4 && !memcmp((const char*)header.bytes+header.length-4,"\r\n\r\n",4))break;}
    NSString *text=[[NSString alloc] initWithData:header encoding:NSUTF8StringEncoding];if(!text)return;
    NSArray *lines=[text componentsSeparatedByString:@"\r\n"];NSArray *first=[lines.firstObject componentsSeparatedByString:@" "];
    if(first.count!=3 || ![first[0] isEqualToString:@"POST"] || ![first[1] isEqualToString:@"/mcp"]){httpReply(fd,405,nil);return;}
    NSMutableDictionary *headers=[NSMutableDictionary dictionary];
    for(NSString *line in lines){NSRange colon=[line rangeOfString:@":"];if(colon.location!=NSNotFound){NSString *key=[line substringToIndex:colon.location].lowercaseString;if(headers[key]){httpReply(fd,400,nil);return;}headers[key]=[[line substringFromIndex:colon.location+1] stringByTrimmingCharactersInSet:NSCharacterSet.whitespaceCharacterSet];}}
    NSString *host=headers[@"host"]?:@"";
    if(![host hasPrefix:@"127.0.0.1:"] || headers[@"origin"] || headers[@"transfer-encoding"] || ![headers[@"authorization"] isEqualToString:[@"Bearer " stringByAppendingString:token]]){httpReply(fd,403,nil);return;}
    NSString *protocol=headers[@"mcp-protocol-version"];
    if(protocol && ![@[@"2025-11-25",@"2025-06-18",@"2025-03-26"] containsObject:protocol]){httpReply(fd,400,nil);return;}
    if(![headers[@"content-type"] hasPrefix:@"application/json"]){httpReply(fd,415,nil);return;}
    NSString *lengthText=headers[@"content-length"];
    NSCharacterSet *digits=NSCharacterSet.decimalDigitCharacterSet;
    if(!lengthText.length || [lengthText rangeOfCharacterFromSet:digits.invertedSet].location!=NSNotFound){httpReply(fd,411,nil);return;}
    NSUInteger length=lengthText.integerValue;if(length>262144){httpReply(fd,413,nil);return;}
    NSMutableData *body=[NSMutableData dataWithLength:length];size_t read=0;
    while(read<length){ssize_t n=recv(fd,(char*)body.mutableBytes+read,length-read,0);if(n<=0)return;read+=(size_t)n;}
    if(!state.active){httpReply(fd,503,nil);return;}
    LoaderJSONJob *job=[LoaderJSONJob new];job.completed=dispatch_semaphore_create(0);
    handler(body.bytes,body.length,jobComplete,(__bridge_retained void*)job,handlerContext);
    if(dispatch_semaphore_wait(job.completed,dispatch_time(DISPATCH_TIME_NOW,8000000000))){httpReply(fd,504,nil);return;}
    httpReply(fd,job.response?200:202,job.response);
}
int loader_listen_local_json(const char *name,loader_http_json_handler handler,void *context,loader_http_endpoint_v1 *endpoint) {
    if(!NSThread.isMainThread || !name || !handler || !endpoint || endpoint->struct_size<sizeof(*endpoint))return LOADER_ERR_INVALID;
    for(const char *p=name;*p;p++)if(!((*p>='a'&&*p<='z')||(*p>='0'&&*p<='9')||*p=='-'))return LOADER_ERR_INVALID;
    @synchronized(LoaderJSONListener.class){if(!listeners)listeners=[NSMutableDictionary dictionary];if(listeners[@(name)])return LOADER_ERR_INVALID;}
    int previous=socket_gate_scope(SOCKET_SCOPE_CONTROL);int listener=socket_gate_open(AF_INET,SOCK_STREAM,0);
    struct sockaddr_in address={0};address.sin_len=sizeof address;address.sin_family=AF_INET;address.sin_addr.s_addr=htonl(INADDR_LOOPBACK);
    if(listener<0 || bind(listener,(struct sockaddr*)&address,sizeof address)<0 || listen(listener,16)<0){if(listener>=0)socket_gate_close(listener);socket_gate_scope(previous);return LOADER_ERR_INVALID;}
    socklen_t size=sizeof address;getsockname(listener,(struct sockaddr*)&address,&size);
    NSString *token=[NSUUID.UUID.UUIDString stringByAppendingString:NSUUID.UUID.UUIDString];
    endpoint->port=ntohs(address.sin_port);snprintf(endpoint->token,sizeof endpoint->token,"%s",token.UTF8String);
    LoaderJSONListener *state=[LoaderJSONListener new];state.fd=listener;state.active=YES;state.token=token;state.handler=handler;state.context=context;
    @synchronized(LoaderJSONListener.class){if(listeners[@(name)]){socket_gate_close(listener);socket_gate_scope(previous);return LOADER_ERR_INVALID;}listeners[@(name)]=state;}
    const char *root=getenv("OPEN_SV2_LOADER_DATA_DIR");
    if(root){NSString *directory=[[NSString stringWithUTF8String:root] stringByAppendingPathComponent:@"endpoints"];[[NSFileManager defaultManager] createDirectoryAtPath:directory withIntermediateDirectories:YES attributes:@{NSFilePosixPermissions:@0700} error:NULL];NSDictionary *entry=@{@"url":[NSString stringWithFormat:@"http://127.0.0.1:%u/mcp",endpoint->port],@"token":token,@"pid":@(getpid()),@"plugin":[NSString stringWithUTF8String:name]};NSData *json=[NSJSONSerialization dataWithJSONObject:entry options:0 error:NULL];NSString *path=[directory stringByAppendingPathComponent:[NSString stringWithFormat:@"%s-%d.json",name,getpid()]];state.discovery=path;[json writeToFile:path atomically:YES];[[NSFileManager defaultManager] setAttributes:@{NSFilePosixPermissions:@0600} ofItemAtPath:path error:NULL];}
    if(!slots)slots=dispatch_semaphore_create(16);
    dispatch_async(dispatch_get_global_queue(QOS_CLASS_UTILITY,0),^{int listenerPrevious=socket_gate_scope(SOCKET_SCOPE_CONTROL);for(;;){if(!state.active)break;int client=accept(listener,NULL,NULL);if(client<0)break;if(!state.active){socket_gate_close(client);break;}socket_gate_adopt(client,SOCKET_SCOPE_CONTROL);if(dispatch_semaphore_wait(slots,DISPATCH_TIME_NOW)){socket_gate_close(client);continue;}dispatch_async(dispatch_get_global_queue(QOS_CLASS_UTILITY,0),^{int old=socket_gate_scope(SOCKET_SCOPE_CONTROL);@autoreleasepool{handleClient(client,token,handler,context,state);}socket_gate_close(client);socket_gate_scope(old);dispatch_semaphore_signal(slots);});}@synchronized(state){socket_gate_close(listener);state.fd=-1;}socket_gate_scope(listenerPrevious);});
    socket_gate_scope(previous);return LOADER_OK;
}

int loader_stop_local_json(const char *name,loader_http_json_handler handler,void *context){
 if(!NSThread.isMainThread||!name||!handler)return LOADER_ERR_INVALID;
 @synchronized(LoaderJSONListener.class){LoaderJSONListener *state=listeners[@(name)];if(!state||state.handler!=handler||state.context!=context)return LOADER_ERR_INVALID;
 @synchronized(state){state.active=NO;if(state.fd>=0)shutdown(state.fd,SHUT_RDWR);}[listeners removeObjectForKey:@(name)];
 if(state.discovery){NSData *data=[NSData dataWithContentsOfFile:state.discovery];id entry=data?[NSJSONSerialization JSONObjectWithData:data options:0 error:NULL]:nil;if([entry[@"token"] isEqual:state.token])[NSFileManager.defaultManager removeItemAtPath:state.discovery error:NULL];}
 }
 return LOADER_OK;
}
