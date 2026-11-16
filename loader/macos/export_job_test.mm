#import <Foundation/Foundation.h>
#include "export_host_adapter.mm"
#if defined(__arm64__)
static NSData *testWav(){uint8_t bytes[46]={0};memcpy(bytes,"RIFF",4);uint32_t n=38;memcpy(bytes+4,&n,4);memcpy(bytes+8,"WAVEfmt ",8);n=16;memcpy(bytes+16,&n,4);uint16_t v=1;memcpy(bytes+20,&v,2);memcpy(bytes+22,&v,2);n=48000;memcpy(bytes+24,&n,4);n=96000;memcpy(bytes+28,&n,4);v=2;memcpy(bytes+32,&v,2);v=16;memcpy(bytes+34,&v,2);memcpy(bytes+36,"data",4);n=2;memcpy(bytes+40,&n,4);return [NSData dataWithBytes:bytes length:sizeof bytes];}
int main(int argc,const char **argv){@autoreleasepool{
 if(argc!=2)return 64;NSString *root=[[NSString stringWithUTF8String:argv[1]] stringByAppendingPathComponent:NSUUID.UUID.UUIDString];[NSFileManager.defaultManager createDirectoryAtPath:root withIntermediateDirectories:YES attributes:nil error:NULL];
 NSMutableData *renderer=[NSMutableData dataWithLength:0x2e4ab];NSData *wave=testWav();
 for(unsigned scenario=0;scenario<4;scenario++){
  NSString *stage=[root stringByAppendingPathComponent:[NSString stringWithFormat:@"stage-%u",scenario]],*target=[root stringByAppendingPathComponent:[NSString stringWithFormat:@"output-%u.wav",scenario]];[NSFileManager.defaultManager createDirectoryAtPath:stage withIntermediateDirectories:NO attributes:nil error:NULL];NSString *source=[stage stringByAppendingPathComponent:@"render_MixDown.wav"];[wave writeToFile:source atomically:YES];
  NSMutableDictionary *job=[@{@"state":@"running",@"stagingDirectory":stage,@"outputPath":target,@"channels":@1,@"sampleRate":@48000,@"bitDepth":@16} mutableCopy];
  ((uint8_t*)renderer.mutableBytes)[0x2e4aa]=scenario==2;
  if(scenario==1)[@"preserved" writeToFile:target atomically:YES encoding:NSUTF8StringEncoding error:NULL];
  if(scenario==3)job[@"channels"]=@2;
  finish(job,(uintptr_t)renderer.mutableBytes);
  if(scenario==0 && (![job[@"state"] isEqual:@"completed"] || ![[NSData dataWithContentsOfFile:target] isEqual:wave] || [NSFileManager.defaultManager fileExistsAtPath:stage]))return 1;
  if(scenario==1 && (![job[@"error"][@"code"] isEqual:@"OUTPUT_EXISTS"] || ![[NSString stringWithContentsOfFile:target encoding:NSUTF8StringEncoding error:NULL] isEqual:@"preserved"] || ![NSFileManager.defaultManager fileExistsAtPath:source]))return 2;
  if(scenario==2 && (![job[@"state"] isEqual:@"cancelled"] || [NSFileManager.defaultManager fileExistsAtPath:target] || ![NSFileManager.defaultManager fileExistsAtPath:source]))return 3;
  if(scenario==3 && (![job[@"error"][@"code"] isEqual:@"INVALID_RENDER_OUTPUT"] || [NSFileManager.defaultManager fileExistsAtPath:target]))return 4;
 }
 puts("4 export finalization scenarios passed: publish, race, native cancel, format mismatch");
}return 0;}
#else
int main(){return 77;}
#endif
