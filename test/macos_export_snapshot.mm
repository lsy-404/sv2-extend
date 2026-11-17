#if defined(__APPLE__) && defined(__arm64__)
#import <Foundation/Foundation.h>
#include "../loader/macos/export_host_adapter.mm"
static uint32_t fixtureLease=LOADER_PROFILE_LEASE_SHARED;
extern "C" int loader_get_profile_lease_state(loader_profile_lease_state_v1 *state){*state={LOADER_ABI_V1,sizeof *state,(int32_t)getpid(),fixtureLease,1};return LOADER_OK;}
extern "C" void loader_export_return_trampoline(void){}
static void store(NSMutableData *object,size_t offset,void *pointer){memcpy((uint8_t*)object.mutableBytes+offset,&pointer,sizeof pointer);}
int main(){@autoreleasepool{
 NSMutableData *closure=[NSMutableData dataWithLength:16],*context=[NSMutableData dataWithLength:16],*root=[NSMutableData dataWithLength:0x2b58],*project=[NSMutableData dataWithLength:0x200],*owner=[NSMutableData dataWithLength:0x1c8],*renderer=[NSMutableData dataWithLength:0x2e4ab];
 store(closure,8,context.mutableBytes);store(context,8,root.mutableBytes);store(root,0x2b50,project.mutableBytes);store(root,0x2ab8,owner.mutableBytes);store(owner,0x1b0,renderer.mutableBytes);store(renderer,0x10,owner.mutableBytes);store(owner,0x1c0,project.mutableBytes);
 salt=@"fixture";jobs=[NSMutableDictionary dictionary];operation=@{@"operation":@"snapshot",@"projectContext":@"fixture",@"arguments":@{}};fixtureLease=LOADER_PROFILE_LEASE_EXCLUSIVE;
 NSDictionary *value=perform(closure.mutableBytes)[@"result"];if(![value[@"busy"] boolValue]||![value[@"profileExclusive"] boolValue]||[value[@"actualExportBusy"] boolValue])return 1;
 ((uint8_t*)renderer.mutableBytes)[0x2e4a8]=1;value=perform(closure.mutableBytes)[@"result"];if(![value[@"actualExportBusy"] boolValue])return 2;((uint8_t*)renderer.mutableBytes)[0x2e4a8]=0;
 ((uint8_t*)renderer.mutableBytes)[0x2e4aa]=1;value=perform(closure.mutableBytes)[@"result"];if(![value[@"actualExportBusy"] boolValue])return 3;((uint8_t*)renderer.mutableBytes)[0x2e4aa]=0;
 ((uint8_t*)renderer.mutableBytes)[0x2e4a9]=1;value=perform(closure.mutableBytes)[@"result"];if(![value[@"pending"] boolValue]||[value[@"actualExportBusy"] boolValue])return 4;((uint8_t*)renderer.mutableBytes)[0x2e4a9]=0;
 activeJob=@"fixture";jobs[activeJob]=[@{@"state":@"running"} mutableCopy];value=perform(closure.mutableBytes)[@"result"];if(![value[@"actualExportBusy"] boolValue])return 5;activeJob=nil;
 operation=@{@"operation":@"start",@"projectContext":@"fixture",@"arguments":@{}};if(![perform(closure.mutableBytes)[@"error"][@"code"] isEqual:@"PROFILE_BUSY"])return 6;
 fixtureLease=LOADER_PROFILE_LEASE_SHARED;operation=@{@"operation":@"snapshot",@"projectContext":@"fixture",@"arguments":@{}};value=perform(closure.mutableBytes)[@"result"];if([value[@"busy"] boolValue]||[value[@"actualExportBusy"] boolValue]||[value[@"profileExclusive"] boolValue])return 7;
 }return 0;}
#else
int main(){return 77;}
#endif
