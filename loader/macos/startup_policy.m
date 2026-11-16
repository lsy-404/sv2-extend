// Generic profile safety primitives. No account fields or private journals.
#import <Foundation/Foundation.h>
#include "startup_policy.h"
#include <sys/stat.h>
#include <sys/file.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
static NSMutableSet<NSString*> *bootstrapOwners;
static int leaseFD=-1;
static struct stat leaseIdentity;
static NSString *leasePath;
static uint32_t leaseMode;
static uint64_t leaseGeneration;
static NSString *exclusiveOwner,*exclusiveOperation;
static BOOL bootstrapApplied;
static BOOL name(const char *raw,NSUInteger limit){if(!raw||strnlen(raw,limit+1)>limit)return NO;NSString *s=[NSString stringWithUTF8String:raw];return s.length&&[s rangeOfString:@"^[A-Za-z0-9][A-Za-z0-9_.-]*$" options:NSRegularExpressionSearch].location!=NSNotFound;}
static NSString *profile(void){const char *raw=getenv("OPEN_SV2_LOADER_DATA_DIR");if(!raw)return nil;NSString *p=@(raw);struct stat st;return p.isAbsolutePath&&[p isEqual:p.stringByResolvingSymlinksInPath]&&lstat(p.fileSystemRepresentation,&st)==0&&S_ISDIR(st.st_mode)&&st.st_uid==getuid()&&!(st.st_mode&0022)?p:nil;}
static int privateFD(NSString *path){if(!path)return -1;int fd=open(path.fileSystemRepresentation,O_RDWR|O_CREAT|O_NOFOLLOW|O_CLOEXEC,0600);struct stat st;if(fd<0)return -1;if(fstat(fd,&st)||!S_ISREG(st.st_mode)||st.st_uid!=getuid()||st.st_nlink!=1||(st.st_mode&0077)){close(fd);return -1;}return fd;}
static BOOL leaseValid(void){struct stat fd,path;return leaseFD>=0&&fstat(leaseFD,&fd)==0&&lstat(leasePath.fileSystemRepresentation,&path)==0&&S_ISREG(fd.st_mode)&&fd.st_dev==leaseIdentity.st_dev&&fd.st_ino==leaseIdentity.st_ino&&path.st_dev==fd.st_dev&&path.st_ino==fd.st_ino;}
static int lockMode(short type){struct flock lock={.l_type=type,.l_whence=SEEK_SET,.l_start=0,.l_len=0,.l_pid=0};return fcntl(leaseFD,F_OFD_SETLK,&lock);}
int loader_profile_lease_bootstrap(void){if(leaseFD>=0)return LOADER_OK;NSString *root=profile();if(!root)return LOADER_ERR_STORAGE_NOT_CONFIGURED;leasePath=[root stringByAppendingPathComponent:@".profile-host.lock"];leaseFD=privateFD(leasePath);if(leaseFD>=0)fstat(leaseFD,&leaseIdentity);if(leaseFD<0)return LOADER_ERR_STORAGE_NOT_CONFIGURED;if(lockMode(F_RDLCK)){int error=errno;close(leaseFD);leaseFD=-1;return error==EAGAIN||error==EACCES?LOADER_ERR_PROFILE_BUSY:LOADER_ERR_NOT_SUPPORTED;}leaseMode=LOADER_PROFILE_LEASE_SHARED;leaseGeneration=1;return LOADER_OK;}
int loader_acquire_profile_exclusive(const char *owner,const char *operation){if(!NSThread.isMainThread||!name(owner,96)||!name(operation,128))return LOADER_ERR_INVALID;if(!leaseValid())return LOADER_ERR_NOT_SUPPORTED;if(leaseMode==LOADER_PROFILE_LEASE_EXCLUSIVE)return [exclusiveOwner isEqual:@(owner)]&&[exclusiveOperation isEqual:@(operation)]?LOADER_OK:LOADER_ERR_PROFILE_BUSY;if(lockMode(F_WRLCK))return LOADER_ERR_PROFILE_BUSY;exclusiveOwner=@(owner);exclusiveOperation=@(operation);leaseMode=LOADER_PROFILE_LEASE_EXCLUSIVE;leaseGeneration++;return LOADER_OK;}
int loader_release_profile_exclusive(const char *owner,const char *operation){if(!NSThread.isMainThread||!name(owner,96)||!name(operation,128))return LOADER_ERR_INVALID;if(!leaseValid())return LOADER_ERR_NOT_SUPPORTED;if(leaseMode==LOADER_PROFILE_LEASE_SHARED)return LOADER_OK;if(![exclusiveOwner isEqual:@(owner)]||![exclusiveOperation isEqual:@(operation)])return LOADER_ERR_INVALID;if(lockMode(F_RDLCK))return LOADER_ERR_INVALID;exclusiveOwner=nil;exclusiveOperation=nil;leaseMode=LOADER_PROFILE_LEASE_SHARED;leaseGeneration++;return LOADER_OK;}
int loader_get_profile_lease_state(loader_profile_lease_state_v1 *state){if(!NSThread.isMainThread||!state||state->abi_version!=LOADER_ABI_V1||state->struct_size<sizeof *state)return LOADER_ERR_INVALID;*state=(loader_profile_lease_state_v1){.abi_version=LOADER_ABI_V1,.struct_size=sizeof *state,.host_pid=getpid(),.mode=leaseValid()?leaseMode:LOADER_PROFILE_LEASE_NONE,.generation=leaseGeneration};return leaseValid()?LOADER_OK:LOADER_ERR_NOT_SUPPORTED;}
static NSString *preferences(void){NSString *root=profile();if(!root)return nil;NSString *path=[root stringByAppendingPathComponent:@"preferences"];struct stat st;if(mkdir(path.fileSystemRepresentation,0700)&&errno!=EEXIST)return nil;if(lstat(path.fileSystemRepresentation,&st)||!S_ISDIR(st.st_mode)||st.st_uid!=getuid()||(st.st_mode&0077))return nil;return path;}
static NSDictionary *readPolicy(BOOL *valid){*valid=NO;NSString *directory=preferences();if(!directory)return nil;NSString *path=[directory stringByAppendingPathComponent:@"startup-network-policy.json"];int fd=open(path.fileSystemRepresentation,O_RDONLY|O_NOFOLLOW|O_CLOEXEC);if(fd<0){if(errno==ENOENT){*valid=YES;return @{@"schema":@1,@"generation":@0,@"guards":@{}};}return nil;}struct stat st;if(fstat(fd,&st)||!S_ISREG(st.st_mode)||st.st_uid!=getuid()||st.st_nlink!=1||(st.st_mode&0077)||st.st_size<0||st.st_size>65536){close(fd);return nil;}NSMutableData *data=[NSMutableData dataWithLength:(NSUInteger)st.st_size];size_t readCount=0;while(readCount<data.length){ssize_t n=read(fd,(char*)data.mutableBytes+readCount,data.length-readCount);if(n<0&&errno==EINTR)continue;if(n<=0)break;readCount+=(size_t)n;}close(fd);if(readCount!=data.length)return nil;id p=[NSJSONSerialization JSONObjectWithData:data options:0 error:NULL];if(![p isKindOfClass:NSDictionary.class]||(![p[@"schema"] isEqual:@1]&&![p[@"schema"] isEqual:@2])||CFGetTypeID((__bridge CFTypeRef)p[@"schema"])==CFBooleanGetTypeID()||![p[@"generation"] isKindOfClass:NSNumber.class]||CFGetTypeID((__bridge CFTypeRef)p[@"generation"])==CFBooleanGetTypeID()||[p[@"generation"] doubleValue]<0||[p[@"generation"] doubleValue]!=[p[@"generation"] unsignedLongLongValue]||[p[@"generation"] unsignedLongLongValue]>9007199254740990ULL||![p[@"guards"] isKindOfClass:NSDictionary.class]||[p[@"guards"] count]>128)return nil;
 for(id key in p[@"guards"]){if(![key isKindOfClass:NSString.class])return nil;NSArray *parts=[key componentsSeparatedByString:@":"];id value=p[@"guards"][key];if(parts.count!=2||!name([parts[0] UTF8String],96)||!name([parts[1] UTF8String],128)||![value isKindOfClass:NSNumber.class]||CFGetTypeID((__bridge CFTypeRef)value)!=CFBooleanGetTypeID()||![value boolValue])return nil;}if([p[@"schema"] isEqual:@1]&&p[@"bootstrapGuards"])return nil;if([p[@"schema"] isEqual:@2]){id required=p[@"bootstrapGuards"];if(![required isKindOfClass:NSDictionary.class]||[required count]>128)return nil;for(id key in required){id value=required[key];if(!p[@"guards"][key]||![value isKindOfClass:NSNumber.class]||CFGetTypeID((__bridge CFTypeRef)value)!=CFBooleanGetTypeID()||![value boolValue])return nil;}}*valid=YES;return p;}
int loader_startup_policy_should_block(void){if(!getenv("OPEN_SV2_LOADER_DATA_DIR"))return 0;BOOL valid=NO;NSDictionary *p=readPolicy(&valid);return !valid||[p[@"guards"] count]>0;}
void loader_startup_policy_mark_applied(void){bootstrapApplied=YES;}
int32_t loader_startup_owner_guard_count(const char *owner){
 if(!NSThread.isMainThread||!name(owner,96))return LOADER_ERR_INVALID;
 BOOL valid=NO;NSDictionary *p=readPolicy(&valid);
 if(!valid)return LOADER_ERR_POLICY_INVALID;
 NSString *prefix=[@(owner) stringByAppendingString:@":"];int32_t count=0;
 for(NSString *key in p[@"guards"])if([key hasPrefix:prefix])count++;
 return count;
}
int loader_get_startup_network_policy(loader_startup_network_state_v1 *state){if(!NSThread.isMainThread||!state||state->abi_version!=LOADER_ABI_V1||state->struct_size<sizeof *state)return LOADER_ERR_INVALID;BOOL valid=NO;NSDictionary *p=readPolicy(&valid);*state=(loader_startup_network_state_v1){.abi_version=LOADER_ABI_V1,.struct_size=sizeof *state,.generation=[p[@"generation"] unsignedLongLongValue],.requested_blocked=!valid||[p[@"guards"] count]>0,.active_guards=(uint32_t)[p[@"guards"] count],.policy_valid=valid,.bootstrap_applied=bootstrapApplied};return LOADER_OK;}
void loader_startup_register_bootstrap_owner(const char *owner){if(NSThread.isMainThread&&name(owner,96)){if(!bootstrapOwners)bootstrapOwners=[NSMutableSet set];[bootstrapOwners addObject:@(owner)];}}
NSArray<NSString*> *loader_startup_required_bootstrap_owners(void){BOOL valid=NO;NSDictionary *p=readPolicy(&valid);if(!valid)return nil;NSMutableSet *owners=[NSMutableSet set];for(NSString *key in p[@"bootstrapGuards"])[owners addObject:[key componentsSeparatedByString:@":"][0]];return [owners.allObjects sortedArrayUsingSelector:@selector(compare:)];}
int loader_set_startup_network_guard(const char *owner,const char *guard,int enabled){
 if(!NSThread.isMainThread||!name(owner,96)||!name(guard,128)||(enabled!=0&&enabled!=1))return LOADER_ERR_INVALID;
 NSString *directory=preferences();if(!directory)return LOADER_ERR_STORAGE_NOT_CONFIGURED;
 int fd=privateFD([directory stringByAppendingPathComponent:@".startup-network.lock"]);if(fd<0)return LOADER_ERR_STORAGE_NOT_CONFIGURED;
 if(flock(fd,LOCK_EX)){close(fd);return LOADER_ERR_INVALID;}
 BOOL valid=NO;NSDictionary *p=readPolicy(&valid);int result=LOADER_ERR_POLICY_INVALID;
 if(valid){
  NSMutableDictionary *guards=[p[@"guards"] mutableCopy],*required=[p[@"bootstrapGuards"] mutableCopy]?:[NSMutableDictionary dictionary];
  NSString *key=[NSString stringWithFormat:@"%s:%s",owner,guard];BOOL existed=guards[key]!=nil;
  BOOL needsBootstrap=[bootstrapOwners containsObject:@(owner)];
  if((enabled&&existed&&(!needsBootstrap||required[key]))||(!enabled&&!existed))result=LOADER_OK;
  else if(!enabled||existed||guards.count<128){
   if(enabled){guards[key]=@YES;if(needsBootstrap)required[key]=@YES;}
   else{[guards removeObjectForKey:key];[required removeObjectForKey:key];}
   NSDictionary *next=@{@"schema":@2,@"generation":@([p[@"generation"] unsignedLongLongValue]+1),@"guards":guards,@"bootstrapGuards":required};
   NSData *data=[NSJSONSerialization dataWithJSONObject:next options:NSJSONWritingSortedKeys error:NULL];NSString *path=[directory stringByAppendingPathComponent:@"startup-network-policy.json"];
   if([data writeToFile:path options:NSDataWritingAtomic error:NULL]&&chmod(path.fileSystemRepresentation,0600)==0){int persisted=open(path.fileSystemRepresentation,O_RDONLY|O_NOFOLLOW|O_CLOEXEC);if(persisted>=0){int syncResult=fsync(persisted);close(persisted);int directoryFD=open(directory.fileSystemRepresentation,O_RDONLY|O_DIRECTORY|O_CLOEXEC);if(directoryFD>=0){if(fsync(directoryFD))syncResult=-1;close(directoryFD);}else syncResult=-1;result=syncResult==0?LOADER_OK:LOADER_ERR_INVALID;}}
  }
 }
 flock(fd,LOCK_UN);close(fd);return result;
}
void loader_list_startup_network_guards(const char *owner,loader_json_completion complete,void *context){if(!complete)return;BOOL valid=NO;NSDictionary *p=readPolicy(&valid);NSMutableArray *ids=[NSMutableArray array];NSString *prefix=name(owner,96)?[@(owner) stringByAppendingString:@":"]:nil;if(NSThread.isMainThread&&valid&&prefix)for(NSString *key in [p[@"guards"] allKeys])if([key hasPrefix:prefix])[ids addObject:[key substringFromIndex:prefix.length]];NSDictionary *reply=NSThread.isMainThread&&valid&&prefix?@{@"ok":@YES,@"result":@{@"guards":[ids sortedArrayUsingSelector:@selector(compare:)]}}:@{@"ok":@NO,@"error":@{@"code":@"STARTUP_POLICY_UNAVAILABLE"}};NSData *data=[NSJSONSerialization dataWithJSONObject:reply options:0 error:NULL];complete(data.bytes,data.length,context);}
