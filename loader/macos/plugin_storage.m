// Bounded profile-local state, not an authentication or native-code sandbox.
#import <Foundation/Foundation.h>
#include "loader_api.h"
#include <CommonCrypto/CommonDigest.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#define STORAGE_LIMIT (1024u*1024u)
static BOOL component(const char *s,size_t limit,BOOL plugin){
 if(!s||!s[0]||strnlen(s,limit+1)>limit||!strcmp(s,".")||!strcmp(s,"..")||!strncasecmp(s,".loader-",8))return NO;
 if(plugin&&!((s[0]>='a'&&s[0]<='z')||(s[0]>='A'&&s[0]<='Z')))return NO;
 for(const char *p=s;*p;p++)if(!((*p>='a'&&*p<='z')||(*p>='A'&&*p<='Z')||(*p>='0'&&*p<='9')||*p=='.'||*p=='_'||*p=='-'))return NO;return YES;
}
// Canonical lower-case escape output prevents case-insensitive filesystems
// aliasing distinct SDK IDs/keys. Raw '%' is invalid, so escaping is injective.
static NSString *diskComponent(const char *raw,BOOL plugin){
 char escaped[3*128+1];size_t at=0;for(const unsigned char *p=(const unsigned char*)raw;*p;p++){if(*p>='A'&&*p<='Z'){snprintf(escaped+at,4,"%%%02x",*p);at+=3;}else escaped[at++]=(char)*p;}escaped[at]=0;
 if(at<=255)return @(escaped);unsigned char hash[CC_SHA256_DIGEST_LENGTH];CC_SHA256(raw,(CC_LONG)strlen(raw),hash);char hex[65];for(unsigned i=0;i<32;i++)snprintf(hex+2*i,3,"%02x",hash[i]);return [NSString stringWithFormat:@".loader-%@-%s",plugin?@"id":@"key",hex];
}
static BOOL regular(struct stat *s){return S_ISREG(s->st_mode)&&s->st_uid==getuid()&&s->st_nlink==1&&!(s->st_mode&0022)&&s->st_size>=0&&s->st_size<=STORAGE_LIMIT;}
static int directoryAt(int parent,const char *name){
 if(mkdirat(parent,name,0700)&&errno!=EEXIST)return -1;
 int fd=openat(parent,name,O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC);struct stat s;
 if(fd<0)return -1;if(fstat(fd,&s)||!S_ISDIR(s.st_mode)||s.st_uid!=getuid()||(s.st_mode&0022)||((s.st_mode&0077)&&fchmod(fd,0700))){close(fd);return -1;}return fd;
}
static int dataDirectory(const char *plugin,BOOL legacy){
 const char *raw=getenv("OPEN_SV2_LOADER_DATA_DIR");if(!raw||!*raw)return -1;
 NSString *path=[NSString stringWithUTF8String:raw];if(!path.isAbsolutePath||![path isEqual:path.stringByStandardizingPath]||![path isEqual:path.stringByResolvingSymlinksInPath])return -1;
 int root=open(raw,O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC);struct stat s;if(root<0)return -1;
 if(fstat(root,&s)||!S_ISDIR(s.st_mode)||s.st_uid!=getuid()||(s.st_mode&0022)){close(root);return -1;}
 int plugins=directoryAt(root,legacy?"plugins":"plugin-data");close(root);if(plugins<0)return -1;
 NSString *diskId=legacy?@(plugin):diskComponent(plugin,YES);int package=directoryAt(plugins,diskId.UTF8String);close(plugins);if(package<0)return -1;
 if(!legacy)return package;int data=directoryAt(package,"data");close(package);return data;
}
static int locked(int directory,int operation){
 int fd=openat(directory,".loader-storage.lock",O_RDWR|O_CREAT|O_NOFOLLOW|O_CLOEXEC,0600);if(fd<0)return -1;struct stat s;
 if(fstat(fd,&s)||!regular(&s)||flock(fd,operation)){close(fd);return -1;}return fd;
}
static int configured(void){const char *root=getenv("OPEN_SV2_LOADER_DATA_DIR");return root&&*root?LOADER_ERR_STORAGE_IO:LOADER_ERR_STORAGE_NOT_CONFIGURED;}
static int writeStorage(const char *plugin,const char *key,const uint8_t *data,size_t length,BOOL legacy){@autoreleasepool{
 if(!component(plugin,96,YES)||!component(key,128,NO)||length>STORAGE_LIMIT||(!data&&length))return LOADER_ERR_INVALID;
 NSString *diskKey=legacy?@(key):diskComponent(key,NO);const char *target=diskKey.UTF8String;
 int directory=dataDirectory(plugin,legacy);if(directory<0)return configured();int lock=locked(directory,LOCK_EX);if(lock<0){close(directory);return LOADER_ERR_STORAGE_IO;}
 int result=LOADER_ERR_STORAGE_IO;struct stat previous;int exists=fstatat(directory,target,&previous,AT_SYMLINK_NOFOLLOW);
 if((exists==0&&!regular(&previous))||(exists<0&&errno!=ENOENT))goto finish;
 {
  NSString *name=[@".loader-tmp-" stringByAppendingString:NSUUID.UUID.UUIDString];const char *temporary=name.UTF8String;
  int fd=openat(directory,temporary,O_WRONLY|O_CREAT|O_EXCL|O_NOFOLLOW|O_CLOEXEC,0600);if(fd<0)goto finish;
  size_t at=0;BOOL good=YES;while(at<length){ssize_t n=write(fd,data+at,length-at);if(n<0&&errno==EINTR)continue;if(n<=0){good=NO;break;}at+=(size_t)n;}
  if(fsync(fd))good=NO;if(close(fd))good=NO;
  if(good){exists=fstatat(directory,target,&previous,AT_SYMLINK_NOFOLLOW);if((exists==0&&!regular(&previous))||(exists<0&&errno!=ENOENT))good=NO;}
  if(good&&renameat(directory,temporary,directory,target)==0){result=fsync(directory)==0?LOADER_OK:LOADER_ERR_STORAGE_IO;}
  else unlinkat(directory,temporary,0);
 }
finish:flock(lock,LOCK_UN);close(lock);close(directory);return result;
}}
int loader_plugin_storage_read_scoped(const char *plugin,const char *key,uint8_t *output,size_t *length){@autoreleasepool{
 if(!component(plugin,96,YES)||!component(key,128,NO)||!length)return LOADER_ERR_INVALID;
 NSString *diskKey=diskComponent(key,NO);const char *target=diskKey.UTF8String;
 int directory=dataDirectory(plugin,NO);if(directory<0)return configured();int lock=locked(directory,LOCK_SH);if(lock<0){close(directory);return LOADER_ERR_STORAGE_IO;}
 int fd=openat(directory,target,O_RDONLY|O_NOFOLLOW|O_CLOEXEC|O_NONBLOCK),result=LOADER_ERR_STORAGE_IO;
 if(fd<0){if(errno==ENOENT)result=LOADER_ERR_NOT_FOUND;goto finish;}
 {
  struct stat before,after,named;if(fstat(fd,&before)||!regular(&before))goto finish;
  size_t needed=(size_t)before.st_size,capacity=*length;*length=needed;
  if(!output){result=LOADER_OK;goto finish;}if(capacity<needed){result=LOADER_ERR_BUFFER_TOO_SMALL;goto finish;}
  uint8_t *bytes=malloc(needed?needed:1);if(!bytes)goto finish;size_t at=0;BOOL good=YES;
  while(at<needed){ssize_t n=read(fd,bytes+at,needed-at);if(n<0&&errno==EINTR)continue;if(n<=0){good=NO;break;}at+=(size_t)n;}
  uint8_t tail;if(good&&(read(fd,&tail,1)!=0||fstat(fd,&after)||fstatat(directory,target,&named,AT_SYMLINK_NOFOLLOW)||!regular(&after)||!regular(&named)||before.st_dev!=after.st_dev||before.st_ino!=after.st_ino||after.st_dev!=named.st_dev||after.st_ino!=named.st_ino||before.st_size!=after.st_size||before.st_mtimespec.tv_sec!=after.st_mtimespec.tv_sec||before.st_mtimespec.tv_nsec!=after.st_mtimespec.tv_nsec||before.st_ctimespec.tv_sec!=after.st_ctimespec.tv_sec||before.st_ctimespec.tv_nsec!=after.st_ctimespec.tv_nsec))good=NO;
  if(good){if(needed)memcpy(output,bytes,needed);result=LOADER_OK;}free(bytes);
 }
finish:if(fd>=0)close(fd);flock(lock,LOCK_UN);close(lock);close(directory);return result;
}}
int loader_plugin_storage_write_scoped(const char *plugin,const char *key,const uint8_t *data,size_t length){return writeStorage(plugin,key,data,length,NO);}
// Compatibility path only. New plugins must use the explicit scoped methods.
int loader_plugin_storage_write(const char *key,const uint8_t *data,size_t length){if(!getenv("OPEN_SV2_LOADER_DATA_DIR"))return LOADER_ERR_STORAGE_NOT_CONFIGURED;return writeStorage("network-probe",key,data,length,YES);}
