// Guard native bindings to the supported host profile.
#import <AppKit/AppKit.h>
#import <objc/runtime.h>
#include <mach-o/dyld.h>
#include <mach-o/loader.h>
#include <mach/mach.h>
#include <mach/mach_vm.h>
#include <CommonCrypto/CommonDigest.h>
#include <libproc.h>
#include <unistd.h>
#include <stdint.h>
#include <cmath>
#include <string.h>
#include <unordered_map>
#include <string>
#include <sys/stat.h>
#include "../packages/builtin_icons.h"
#include "../packages/panel_icon.h"
extern "C" bool loader_native_panel_owns(void*,void*);
extern "C" void loader_native_panel_remove(void*);
extern "C" void loader_native_panel_capture(void*,NSString*,bool,intptr_t);
extern "C" NSDictionary *loader_plugin_ui_call(NSString*,NSDictionary*);
extern "C" const char *loader_host_data_directory(void);
#if defined(__arm64__)
namespace {
static intptr_t slide;
// Hide dormant handles to avoid occupying toolbar space.
struct Category {void *registry[8],*descriptorTable[28];void **object;char identifier[97],label[513],icon[4096],activeIcon[4096];bool present,visible,matchIconColors;uint64_t renderedGeneration,iconGeneration;};
struct ButtonIcon {const Category *category;uint64_t generation;bool active;void *copies[3];};
static std::unordered_map<void *,ButtonIcon> buttonIcons;
static Category categories[33];
static NSMutableSet<NSValue*> *panes;
static NSMutableSet<NSValue*> *bars;
static NSTimer *timer;
static NSNumber *lastGeneration;
static void ensureTimer();
struct HostString {uintptr_t p;~HostString(){((void(*)(void*))(slide+0x100699298ULL))(this);}};
struct Drawable {void *p;~Drawable(){if(p)((void(*)(void*))(*(void***)p)[1])(p);}};
static HostString string(const char *text){HostString v;v.p=((uintptr_t(*)(const char*))(slide+0x100662a08ULL))(text);return v;}
static Category *fromSelf(void *self){for(auto &c:categories)if(self==&c.object)return &c;return nullptr;}
static int modelIndex(void *model){if(!model)return -1;for(unsigned i=0;i<33;i++)if(*(void***)model==categories[i].descriptorTable+2)return (int)i;return -1;}
static int paneIndex(void *pane){return modelIndex(*(void**)((char*)pane+0x140));}
static HostString name(void *self){auto c=fromSelf(self);return string(c?c->label:"Extend");}
static HostString typeName(void*){return string("OpenSV2ExtendPanel");}
static int position(void *self){auto c=fromSelf(self);return 1000+(c?(int)(c-categories):0);}
static void noDestroy(void*){}
static void *getRegistry(void *model){int index=modelIndex(model);return index<0?nullptr:&categories[index].object;}
static void *descriptor(void *self){auto c=fromSelf(self);if(!c)return nullptr;void *p=((void*(*)(size_t))(slide+0x101197690ULL))(0x50);((void(*)(void*,int))(slide+0x100ecfee0ULL))(p,1);*(void**)p=(void*)(slide+0x1015be1a8ULL);((void(*)(void*,int,int))(slide+0x100ed3320ULL))(p,0xf0,1);*(void**)p=c->descriptorTable+2;return p;}
// The Loader owns the root icon; every other category shows its declared icon, falling back to the generic plugin mark.
static Drawable icon(void *self){
 auto c=fromSelf(self);
 const char *source=c==categories?OPENSV2_ICON_ROOT:(c&&c->icon[0])?c->icon:OPENSV2_ICON_PLUGIN;
 auto svg=c&&c!=categories?panel_icon::resolve(source,c->activeIcon,c->matchIconColors,false):std::string(source);
 return ((Drawable(*)(const void*,size_t))(slide+0x10080a154ULL))(svg.data(),svg.size());
}
static Drawable parseIcon(const std::string &svg){
 return ((Drawable(*)(const void*,size_t))(slide+0x10080a154ULL))(svg.data(),svg.size());
}
static void replaceDrawable(void **slot,Drawable &next){
 void *previous=*slot;*slot=next.p;next.p=nullptr;
 if(previous)((void(*)(void*))(*(void***)previous)[1])(previous);
}
static void refreshIcon(void *button,Category &category,bool active){
 if(&category==categories)return;
 auto copies=(void **)((char *)button+0x2a8);
 auto found=buttonIcons.find(button);
 if(found!=buttonIcons.end()&&found->second.category==&category&&found->second.generation==category.iconGeneration&&
    found->second.active==active&&found->second.copies[0]==copies[0]&&
    found->second.copies[1]==copies[1]&&found->second.copies[2]==copies[2])return;
 const char *source=category.icon[0]?category.icon:OPENSV2_ICON_PLUGIN;
 std::string svg=panel_icon::resolve(source,category.activeIcon,category.matchIconColors,active);
 Drawable normal=parseIcon(svg);
 if(!normal.p)return;
 bool raw=!category.matchIconColors||(active&&category.activeIcon[0]);
 Drawable first=raw?parseIcon(svg):Drawable{nullptr};
 Drawable second=raw?parseIcon(svg):Drawable{nullptr};
 Drawable third=raw?parseIcon(svg):Drawable{nullptr};
 if(raw&&(!first.p||!second.p||!third.p))return;
 replaceDrawable((void **)((char *)button+0x2a0),normal);
 ((void(*)(void*,bool))(slide+0x10101f2dcULL))(button,active);
 if(raw){
  replaceDrawable(&copies[0],first);replaceDrawable(&copies[1],second);replaceDrawable(&copies[2],third);
 }
 buttonIcons[button]={&category,category.iconGeneration,active,{copies[0],copies[1],copies[2]}};
}
static void remove(void *pane){
 loader_native_panel_remove(pane);
 [panes removeObject:[NSValue valueWithPointer:pane]];
}
static void layout(void *pane){
 int index=paneIndex(pane);bool script=index<0;
 int count=*(int*)((char*)pane+0x64);
 void **children=*(void***)((char*)pane+0x58);
 if(count<0||count>128)return;
 for(int i=0;i<count;i++){
  void *child=children[i];if(loader_native_panel_owns(pane,child))continue;
  if(!script){if(*(uint16_t*)((char*)child+0xe8)&2)
   ((void(*)(void*,bool))(*(void***)child)[0x60/8])(child,false);}
  else {
   int x=*(int*)((char*)child+0x38),y=*(int*)((char*)child+0x3c);
   int w=*(int*)((char*)child+0x40),h=*(int*)((char*)child+0x44);
   int paneHeight=*(int*)((char*)pane+0x44);
   int bottom=paneHeight-MIN(38,MAX(0,paneHeight-44));
   if(y+h>bottom)
    ((void(*)(void*,int,int,int,int))(slide+0x1007ff7dcULL))
      (child,x,y,w,MAX(0,bottom-y));
  }
 }
 NSString *identifier=index>0?@(categories[index].identifier):nil;
 loader_native_panel_capture(pane,identifier,script,slide);
}
static void capture(void *pane){
 if(!panes)panes=[NSMutableSet set];
 [panes addObject:[NSValue valueWithPointer:pane]];ensureTimer();layout(pane);
}
static void paint(void *pane,void *graphics){((void(*)(void*,void*))(slide+0x10101b198ULL))(pane,graphics);capture(pane);}
static void resized(void *pane){((void(*)(void*))(slide+0x1010352a8ULL))(pane);capture(pane);}
static void destroy(void *pane){remove(pane);((void(*)(void*))(slide+0x101035270ULL))(pane);}
static void deletingDestroy(void *pane){remove(pane);((void(*)(void*))(slide+0x10103527cULL))(pane);}
static void barLayout(void *bar){
 ((void(*)(void*))(slide+0x101020c84ULL))(bar);if(!bars)bars=[NSMutableSet set];[bars addObject:[NSValue valueWithPointer:bar]];ensureTimer();
 void **buttons=*(void***)((char*)bar+0x220);int count=*(int*)((char*)bar+0x22c);void **entries=*(void***)(slide+0x10161f218ULL);int entryCount=*(int*)(slide+0x10161f224ULL);if(count!=entryCount||count<1||count>64)return;
 int active=0;bool hasExtend=false;for(int i=0;i<count;i++){auto c=fromSelf(entries[i]);if(!c||(c->present&&c->visible)){active++;if(c)hasExtend=true;}}
 int width=*(int*)((char*)bar+0x40),height=*(int*)((char*)bar+0x44);int row=MIN(48,MAX(12,(height-(hasExtend?40:28))/MAX(1,active))),y=8;bool grouped=false;
 for(int i=0;i<count;i++){void *button=buttons[i];auto c=fromSelf(entries[i]);bool visible=!c||(c->present&&c->visible);((void(*)(void*,bool))(*(void***)button)[0x60/8])(button,visible);if(!visible){((void(*)(void*,int,int,int,int))(slide+0x1007ff7dcULL))(button,0,0,0,0);continue;}
 if(!c){const char *label=*(const char**)((char*)button+0x298);if(label&&!strcmp(label,"LICENSE"))y+=12;}
 else{if(!grouped){y+=12;grouped=true;}uintptr_t *label=(uintptr_t*)((char*)button+0x298);if(strcmp((const char*)*label,c->label)){((void(*)(void*))(slide+0x100699298ULL))(label);*label=((uintptr_t(*)(const char*))(slide+0x100662a08ULL))(c->label);}
 void *container=*(void**)((char*)bar+0xf8);void *model=((void*(*)(void*,void*))(slide+0x101024958ULL))(container,&c->object);
 refreshIcon(button,*c,model&&!*(uint8_t*)((char*)model+0x34));}
 ((void(*)(void*,int,int,int,int))(slide+0x1007ff7dcULL))(button,0,y,width,row);y+=row;
 }
}
struct LineTransform {float scale;int thickness;float first[6],second[6];uint64_t reserved[2];};
static_assert(sizeof(LineTransform)==0x48,"Invalid line transform size");
static void drawGroupLine(void *bar,void *graphics,int y){
 LineTransform t{};
 ((void(*)(void*,void*,uint64_t))(slide+0x1010a7f30ULL))(&t,bar,0);
 auto map=[](const float *m,float x,float y,float &ox,float &oy){
  ox=m[0]*x+m[1]*y+m[2];oy=m[3]*x+m[4]*y+m[5];
 };
 float x1,y1,x2,y2;
 map(t.first,6,(float)y,x1,y1);
 map(t.first,(float)(*(int*)((char*)bar+0x40)-6),(float)(y+1),x2,y2);
 float left=std::nearbyintf(MIN(x1,x2)),right=std::nearbyintf(MAX(x1,x2));
 float top=std::nearbyintf(MIN(y1,y2));
 map(t.second,left,top,x1,y1);
 map(t.second,right,top+MAX(0,t.thickness),x2,y2);
 float rect[]={MIN(x1,x2),MIN(y1,y2),std::fabs(x2-x1),std::fabs(y2-y1)};
 uint32_t color=0x40000000U|(*(uint32_t*)(slide+0x101617f0cULL)&0xffffffU);
 ((void(*)(void*,uint32_t))(slide+0x1006c66ecULL))(graphics,color);
 void *context=*(void**)((char*)graphics+8);
 ((void(*)(void*,const float*))(*(void***)context)[0xb8/8])(context,rect);
}
static void barPaint(void *bar,void *graphics){
 barLayout(bar);
 ((void(*)(void*,void*))(slide+0x1010206fcULL))(bar,graphics);
 void **buttons=*(void***)((char*)bar+0x220);
 void **entries=*(void***)(slide+0x10161f218ULL);
 int count=*(int*)((char*)bar+0x22c),entryCount=*(int*)(slide+0x10161f224ULL);
 if(count!=entryCount||count<1||count>64)return;
 for(int i=0;i<count;i++){
  auto c=fromSelf(entries[i]);if(!c||!c->present||!c->visible)continue;
  drawGroupLine(bar,graphics,*(int*)((char*)buttons[i]+0x3c)-6);break;
 }
}
static void forgetBarIcons(void *bar){
 void **buttons=*(void***)((char*)bar+0x220);int count=*(int*)((char*)bar+0x22c);
 if(count>=0&&count<=64)for(int i=0;i<count;++i)buttonIcons.erase(buttons[i]);
}
static void barDestroy(void *bar){forgetBarIcons(bar);[bars removeObject:[NSValue valueWithPointer:bar]];((void(*)(void*))(slide+0x101020e60ULL))(bar);}
static void barDeletingDestroy(void *bar){forgetBarIcons(bar);[bars removeObject:[NSValue valueWithPointer:bar]];((void(*)(void*))(slide+0x101020f4cULL))(bar);}
static void closeCategory(Category *category){for(NSValue *key in bars.allObjects){void *container=*(void**)((char*)key.pointerValue+0xf8);void *model=((void*(*)(void*,void*))(slide+0x101024958ULL))(container,&category->object);if(model&&!*(uint8_t*)((char*)model+0x34)){HostString reason=string("user");((void(*)(void*,void*,const void*))(slide+0x101024b0cULL))(container,&category->object,&reason);}}}
static void syncCategories(){NSDictionary *snapshot=loader_plugin_ui_call(@"ui.snapshot",@{})[@"result"];if(!snapshot||[snapshot[@"generation"] isEqual:lastGeneration])return;lastGeneration=snapshot[@"generation"];categories[0].visible=[snapshot[@"extendEntryVisible"] boolValue];if(!categories[0].visible)closeCategory(categories);
 NSMutableSet *identifiers=[NSMutableSet set];for(NSDictionary *item in snapshot[@"plugins"])[identifiers addObject:item[@"id"]];
 for(unsigned i=1;i<33;i++)categories[i].present=false;
 for(NSDictionary *plugin in snapshot[@"plugins"]){Category *category=nullptr;NSString *identifier=plugin[@"id"];for(unsigned i=1;i<33;i++)if(!strcmp(categories[i].identifier,identifier.UTF8String)){category=&categories[i];break;}
 if(!category){for(unsigned i=1;i<33;i++)if(!categories[i].identifier[0]){category=&categories[i];break;}}
 if(!category){for(unsigned i=1;i<33;i++)if(![identifiers containsObject:@(categories[i].identifier)]){category=&categories[i];closeCategory(category);for(NSValue *key in panes.allObjects)if(paneIndex(key.pointerValue)==(int)i)remove(key.pointerValue);break;}}
 if(!category)continue;
 const char *newIcon=[plugin[@"icon"] isKindOfClass:NSString.class]?[plugin[@"icon"] UTF8String]:"";
 const char *newActiveIcon=[plugin[@"activeIcon"] isKindOfClass:NSString.class]?[plugin[@"activeIcon"] UTF8String]:"";
 bool matching=plugin[@"matchIconColors"]?[plugin[@"matchIconColors"] boolValue]:true;
 if(strcmp(category->identifier,identifier.UTF8String)||strcmp(category->icon,newIcon)||
    strcmp(category->activeIcon,newActiveIcon)||category->matchIconColors!=matching)category->iconGeneration++;
 strlcpy(category->identifier,identifier.UTF8String,sizeof category->identifier);
 strlcpy(category->label,[plugin[@"name"] UTF8String],sizeof category->label);
 strlcpy(category->icon,newIcon,sizeof category->icon);strlcpy(category->activeIcon,newActiveIcon,sizeof category->activeIcon);
 category->matchIconColors=matching;category->present=true;category->visible=[plugin[@"visible"] boolValue];
 }
 for(unsigned i=1;i<33;i++)if(!categories[i].present||!categories[i].visible)closeCategory(&categories[i]);
 for(NSValue *key in bars.allObjects)barLayout(key.pointerValue);
}
static void ensureTimer(){if(timer)return;timer=[NSTimer timerWithTimeInterval:.25 repeats:YES block:^(NSTimer*){syncCategories();for(NSValue *key in bars.allObjects)barLayout(key.pointerValue);for(NSValue *key in panes.allObjects)layout(key.pointerValue);}];[NSRunLoop.mainRunLoop addTimer:timer forMode:NSRunLoopCommonModes];}
struct SlotChange {unsigned offset;void *replacement;uintptr_t expected;};
static bool changeSlots(uintptr_t address,const SlotChange *changes,size_t number){
 void **slots=(void**)(slide+address);for(size_t i=0;i<number;i++)if(slots[changes[i].offset/8]!=(void*)(slide+changes[i].expected))return false;
 mach_vm_address_t region=(uintptr_t)slots;mach_vm_size_t length=0;vm_region_basic_info_data_64_t info;mach_msg_type_number_t count=VM_REGION_BASIC_INFO_COUNT_64;mach_port_t port=MACH_PORT_NULL;kern_return_t rc=mach_vm_region(mach_task_self(),&region,&length,VM_REGION_BASIC_INFO_64,(vm_region_info_t)&info,&count,&port);if(port)mach_port_deallocate(mach_task_self(),port);if(rc!=KERN_SUCCESS||(uintptr_t)slots<region||(uintptr_t)slots+0x148>region+length)return false;
 mach_vm_address_t page=(uintptr_t)slots&~((mach_vm_address_t)vm_page_size-1);if(mach_vm_protect(mach_task_self(),page,vm_page_size,FALSE,info.protection|VM_PROT_WRITE|VM_PROT_COPY)!=KERN_SUCCESS)return false;
 for(size_t i=0;i<number;i++)slots[changes[i].offset/8]=changes[i].replacement;
 if(mach_vm_protect(mach_task_self(),page,vm_page_size,FALSE,info.protection)!=KERN_SUCCESS){for(size_t i=0;i<number;i++)slots[changes[i].offset/8]=(void*)(slide+changes[i].expected);return false;}return true;
}
static bool setSlots(){SlotChange pane[]={{0,(void*)destroy,0x101035270ULL},{8,(void*)deletingDestroy,0x10103527cULL},{0xf8,(void*)paint,0x10101b198ULL},{0x140,(void*)resized,0x1010352a8ULL}};SlotChange bar[]={{0,(void*)barDestroy,0x101020e60ULL},{8,(void*)barDeletingDestroy,0x101020f4cULL},{0xf8,(void*)barPaint,0x1010206fcULL},{0x140,(void*)barLayout,0x101020c84ULL}};return changeSlots(0x1015bf430ULL,pane,4)&&changeSlots(0x1015bdd50ULL,bar,4);}
// Native persistence cannot deserialize transient panel models.
static bool retireTransientLayout(){
 const char *directory=loader_host_data_directory();if(!directory)return true;NSString *file=[@(directory) stringByAppendingPathComponent:@"settings/settings.xml"];NSData *before=[NSData dataWithContentsOfFile:file];if(!before||![before length])return true;if(before.length>4*1024*1024)return false;
 NSString *text=[[NSString alloc]initWithData:before encoding:NSUTF8StringEncoding];if(![text containsString:@"OpenSV2ExtendPanel"])return true;
 struct stat info;if(lstat(file.fileSystemRepresentation,&info)||!S_ISREG(info.st_mode)||info.st_uid!=getuid())return false;
 NSXMLDocument *document=[[NSXMLDocument alloc]initWithData:before options:NSXMLNodePreserveAll|NSXMLNodeLoadExternalEntitiesNever error:NULL];if(!document||![document.rootElement.name isEqual:@"ApplicationSettings"])return false;
 NSArray *nodes=[document nodesForXPath:@"/ApplicationSettings/UILayout//OpenSV2ExtendPanel" error:NULL];if(!nodes.count)return true;for(NSXMLNode *node in nodes)[node detach];NSData *after=[document XMLDataWithOptions:NSXMLNodePreserveAll];
 const char *root=getenv("OPEN_SV2_LOADER_DATA_DIR");if(!root)return false;NSString *backups=[@(root) stringByAppendingPathComponent:@"preferences/layout-backups"];if(![NSFileManager.defaultManager createDirectoryAtPath:backups withIntermediateDirectories:YES attributes:@{NSFilePosixPermissions:@0700} error:NULL])return false;
 NSString *backup=[backups stringByAppendingPathComponent:[NSUUID.UUID.UUIDString stringByAppendingString:@".xml"]];if(![before writeToFile:backup options:NSDataWritingWithoutOverwriting error:NULL])return false;chmod(backup.fileSystemRepresentation,0600);
 if(![[NSData dataWithContentsOfFile:file] isEqual:before]||![after writeToFile:file options:NSDataWritingAtomic error:NULL])return false;chmod(file.fileSystemRepresentation,info.st_mode&0777);fprintf(stderr,"EXTEND_CATEGORY retired-transient-layouts=%lu\n",(unsigned long)nodes.count);return true;
}
static bool profile(){
 char path[PROC_PIDPATHINFO_MAXSIZE]={0};proc_pidpath(getpid(),path,sizeof path);if(!strstr(path,"/SV2 Extended.app/Contents/MacOS/synthv-studio-original"))return false;
 const mach_header_64 *h=nullptr;unsigned image=0;for(unsigned i=0;i<_dyld_image_count();i++){auto candidate=(const mach_header_64*)_dyld_get_image_header(i);if(candidate->filetype==MH_EXECUTE){h=candidate;image=i;break;}}if(!h)return false;
 const uint8_t uuid[]={0xac,0xfe,0xfc,0x17,0x25,0xc2,0x34,0xd5,0xb2,0x8a,0xe8,0xbf,0x6f,0xfb,0x32,0xb4};bool match=false;auto command=(const load_command*)(h+1);for(unsigned i=0;i<h->ncmds;i++){if(command->cmd==LC_UUID)match=!memcmp(((const uuid_command*)command)->uuid,uuid,16);command=(const load_command*)((char*)command+command->cmdsize);}if(!match)return false;slide=_dyld_get_image_vmaddr_slide(image);
 struct Check{uintptr_t address;size_t length;const char *hash;};
 static const Check checks[]={
 {0x10080a154ULL,384,"347b3531f2ca5d7ff957eaf6e1067a6f99966c28841a3b7a216f0914a17c90c5"},
 {0x1007ff7dcULL,652,"3c4b76937a224d2991e8b15dd1f421c6bb89829162131f131328e0480b83f7bf"},
 {0x101020c84ULL,328,"c569460bf8b906c8cb0d436e800575a8e609759b647aacb2e1463971647ad620"},
 {0x1010206fcULL,1272,"87c1fe1f70c255b593112b15e36c17fc6aef4b62296ff8a3a92c65e4c82cf30f"},
 {0x10101f2dcULL,704,"23473f554006b7e29efa5eb11a8afff51f4ff779ed7f32aa9261fe8197ca66db"},
 {0x101020e60ULL,236,"c0c1a7bbb6781784a99ed4fed27777b47c0742f65b6aa4bbf0a96327ffd9090f"},
 {0x101020f4cULL,240,"ac369b8d9c384c276853a210d6f6fb181b61744987889f5641818c69869932fe"},
 {0x101024958ULL,436,"242b677b57c546560277e2c19a975f226a6dbc6f78fe0a245f84f22f443864e6"},
 {0x101024b0cULL,2304,"59b9bfb0e9c1f97b00c7b20a1c3d55cbf18b902cfbe6fa2bd5c7fb4b37586df3"},
 {0x100662a08ULL,400,"9963a491a5f165175660f70d89ab96313f835d78533c254cb9bf55da99a6f147"},
 {0x100699298ULL,44,"f79a7305b5ef0e3115ede6956943945e726c6ea451df8a01f2dc4312ac25af1c"},
 {0x100ecfee0ULL,2816,"24dac859ea5659aa2ff32687e364de6122bcc85c4635fc9fd9f96bfe85cc4c24"},
 {0x100ed3320ULL,276,"ab0fc1409d6f2e83cadf1a43d8823b8c57b733458207a06696aa3174ffe2f3ca"},
 {0x1010243fcULL,580,"396431f3ca7fcbfd909a44b2b18493d7960ee4f7023f8b6ba8235e8381666baf"},
 {0x101035270ULL,4,"865db80d282e43e1df0e2ec6018effbee06edb7ab7cc98f5360c43c93dd262fa"},
 {0x10103527cULL,20,"41ff442662b944b219dee286c5dce5e5a9b62d106982d8c9a00d4c2381244cf9"},
 {0x10101b198ULL,2392,"4603c750be3f7012c826470c22d9ac49c307ef888f07e4abbf921d90df1280df"},
 {0x1010352a8ULL,460,"2216e0fdb8931ad5fb0ff88bf5337f227d9250f08a9641bbb78162c6d07c5e0f"},
 {0x101034834ULL,132,"b0fd83806f2d7ad456ac4282754efa725d0aa8d078f37425ee543644419d3860"},
 {0x1010345e4ULL,592,"d6de7a3da9b62b2d190e0472eef7567b0767e345acf973564eeb038ed1ccb574"},
 {0x1007fc488ULL,748,"eecebb6de935dbd541275503503e847101aa24bdd960aff2131072bde0e11971"},
 {0x1010193c8ULL,2340,"4ff5fbfd3c12dc68a468ca8b77d934ca906164e4a28bc7783678db093cd03852"},
 };
 for(const Check &check:checks){unsigned char digest[32];CC_SHA256((void*)(slide+check.address),(CC_LONG)check.length,digest);char hash[65];for(unsigned i=0;i<32;i++)snprintf(hash+2*i,3,"%02x",digest[i]);if(strcmp(hash,check.hash))return false;}
 void **table=(void**)(slide+0x1015bf430ULL);return table[0]==(void*)(slide+0x101035270ULL)&&table[1]==(void*)(slide+0x10103527cULL)&&table[0xf8/8]==(void*)(slide+0x10101b198ULL)&&table[0x140/8]==(void*)(slide+0x1010352a8ULL);
}
}
extern "C" int loader_extend_open_control_panel(void){
 if(!NSThread.isMainThread)return -1;syncCategories();if(!categories[0].visible)return -1;
 for(NSValue *key in bars.allObjects){void *container=*(void**)((char*)key.pointerValue+0xf8);void *model=((void*(*)(void*,void*))(slide+0x101024958ULL))(container,&categories[0].object);if(!model||*(uint8_t*)((char*)model+0x34)){HostString reason=string("user");((void(*)(void*,void*,const void*))(slide+0x101024b0cULL))(container,&categories[0].object,&reason);}return 0;}return -1;
}
extern "C" int loader_extend_open_plugin_panel(const char *identifier){
 if(!NSThread.isMainThread||!identifier)return -1;syncCategories();decltype(&categories[0]) category=nullptr;for(unsigned i=1;i<33;i++)if(!strcmp(categories[i].identifier,identifier)){category=&categories[i];break;}if(!category||!category->present||!category->visible)return -1;
 for(NSValue *key in bars.allObjects){void *container=*(void**)((char*)key.pointerValue+0xf8);void *model=((void*(*)(void*,void*))(slide+0x101024958ULL))(container,&category->object);if(!model||*(uint8_t*)((char*)model+0x34)){HostString reason=string("user");((void(*)(void*,void*,const void*))(slide+0x101024b0cULL))(container,&category->object,&reason);}return 0;}return -1;
}
#ifdef LOADER_CATEGORY_TEST
extern "C" int loader_category_test_retire_layout(void){return retireTransientLayout()?0:-1;}
#endif
extern "C" int loader_category_prepare(void){if(!profile()||!loader_host_data_directory()||!retireTransientLayout())return -1;
 for(unsigned i=0;i<33;i++){auto &c=categories[i];snprintf(c.label,sizeof c.label,i?"ExtendPluginSlot%u":"Extend",i);c.present=c.visible=i==0;memcpy(c.registry,(void*)(slide+0x1015bf668ULL),sizeof c.registry);c.registry[2]=(void*)noDestroy;c.registry[3]=(void*)noDestroy;c.registry[4]=(void*)name;c.registry[5]=(void*)icon;c.registry[6]=(void*)position;c.registry[7]=(void*)descriptor;c.object=c.registry+2;memcpy(c.descriptorTable,(void*)(slide+0x1015c09d8ULL),sizeof c.descriptorTable);c.descriptorTable[2+0xa8/8]=(void*)typeName;c.descriptorTable[2+0xc0/8]=(void*)getRegistry;}
 // Reserve built-in slots before toolbar drawables are created.
 strlcpy(categories[1].identifier,"opensv2.javascript-extend",sizeof categories[1].identifier);
 strlcpy(categories[2].identifier,"opensv2.lua-extend",sizeof categories[2].identifier);
 strlcpy(categories[3].identifier,"opensv2.mcp",sizeof categories[3].identifier);
 if(!setSlots())return -1;for(auto &c:categories)((void(*)(void*,void*,void*))(slide+0x1010243fcULL))((void*)(slide+0x10161f218ULL),nullptr,&c.object);return 0;
}
#else
extern "C" int loader_extend_open_control_panel(void){return -1;}
extern "C" int loader_extend_open_plugin_panel(const char*){return -1;}
extern "C" int loader_category_prepare(void){return -1;}
#endif
__attribute__((constructor)) static void categoryLoad(){fprintf(stderr,"EXTEND_CATEGORY registered=%d\n",loader_category_prepare()==0);}
