// Guarded SV2 2.3.0 arm64 native category. Only managed host copies are accepted.
// Uses registry objects and data-vtable slots; no executable code is rewritten.
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
#include <string.h>
#include <sys/stat.h>
extern "C" NSView *loader_extend_control_view(void);
extern "C" NSView *loader_extend_scripts_view(void);
extern "C" NSView *loader_extend_plugin_view(NSString*);
extern "C" NSDictionary *loader_plugin_ui_call(NSString*,NSDictionary*);
extern "C" const char *loader_host_data_directory(void);
extern "C" void loader_extend_control_refresh(NSView*);
#if defined(__arm64__)
namespace {
static intptr_t slide;
// Stable native category handles. Late plugin registrations bind an unused
// handle; dormant handles are hidden and consume no toolbar space.
struct Category {void *registry[8],*descriptorTable[28];void **object;char identifier[97],label[513];bool present,visible;uint64_t renderedGeneration;};
static Category categories[33];
static NSMutableDictionary<NSValue*,NSView*> *panes;
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
// Padded 18px monochrome glyphs in the host 24px toolbar cell.
static const char rootSVG[]="<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"24\" height=\"24\" viewBox=\"0 0 24 24\"><rect width=\"24\" height=\"24\" fill=\"#000000\" opacity=\"0\"/><g transform=\"translate(3 3) scale(.75)\" fill=\"none\" stroke=\"#000000\" stroke-width=\"1.5\" stroke-linecap=\"round\" stroke-linejoin=\"round\"><rect x=\"3\" y=\"3\" width=\"6\" height=\"6\" rx=\"1\"/><rect x=\"3\" y=\"15\" width=\"6\" height=\"6\" rx=\"1\"/><rect x=\"15\" y=\"15\" width=\"6\" height=\"6\" rx=\"1\"/><path d=\"M14 6h8M18 2v8\"/></g></svg>";
static const char pluginSVG[]="<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"24\" height=\"24\" viewBox=\"0 0 24 24\"><rect width=\"24\" height=\"24\" fill=\"#000000\" opacity=\"0\"/><g transform=\"translate(3 3) scale(.75)\" fill=\"none\" stroke=\"#000000\" stroke-width=\"1.5\" stroke-linecap=\"round\" stroke-linejoin=\"round\"><path d=\"M4 4h6V3a2 2 0 0 1 4 0v1h6v6h1a2 2 0 0 1 0 4h-1v6h-6v-1a2 2 0 0 0-4 0v1H4v-6h1a2 2 0 0 0 0-4H4z\"/></g></svg>";
static const char javascriptSVG[]="<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"24\" height=\"24\" viewBox=\"0 0 24 24\"><rect width=\"24\" height=\"24\" fill=\"#000000\" opacity=\"0\"/><g transform=\"translate(3 3) scale(.75)\"><rect x=\"2\" y=\"2\" width=\"20\" height=\"20\" rx=\"1\" fill=\"none\" stroke=\"#000000\" stroke-width=\"1.4\"/><path d=\"M11 9v7c0 3-4 3-5 1m13-7c-3-2-6 1-3 3l2 1c4 3-1 6-4 3\" fill=\"none\" stroke=\"#000000\" stroke-width=\"1.7\" stroke-linecap=\"round\"/></g></svg>";
static const char luaSVG[]="<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"24\" height=\"24\" viewBox=\"0 0 24 24\"><rect width=\"24\" height=\"24\" fill=\"#000000\" opacity=\"0\"/><g transform=\"translate(3 3) scale(.75)\"><path fill=\"#000000\" fill-rule=\"evenodd\" d=\"M19 13a8 8 0 1 1-16 0 8 8 0 0 1 16 0z M17 10a3 3 0 1 1-6 0 3 3 0 0 1 6 0z\"/><circle cx=\"21\" cy=\"3\" r=\"2\" fill=\"#000000\"/></g></svg>";
static const char mcpSVG[]="<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"24\" height=\"24\" viewBox=\"0 0 24 24\"><rect width=\"24\" height=\"24\" fill=\"#000000\" opacity=\"0\"/><g transform=\"translate(3 3) scale(.75)\" fill=\"none\" stroke=\"#000000\" stroke-width=\"1.5\" stroke-linecap=\"round\" stroke-linejoin=\"round\"><circle cx=\"5\" cy=\"12\" r=\"3\"/><circle cx=\"19\" cy=\"5\" r=\"3\"/><circle cx=\"19\" cy=\"19\" r=\"3\"/><path d=\"m8 10 8-4M8 14l8 4\"/></g></svg>";
static Drawable icon(void *self){auto c=fromSelf(self);const char *svg=c==categories?rootSVG:c&&!strcmp(c->identifier,"opensv2.javascript-extend")?javascriptSVG:c&&!strcmp(c->identifier,"opensv2.lua-extend")?luaSVG:c&&!strcmp(c->identifier,"opensv2.mcp")?mcpSVG:pluginSVG;return ((Drawable(*)(const void*,size_t))(slide+0x10080a154ULL))(svg,strlen(svg));}
static void remove(void *pane){NSValue *key=[NSValue valueWithPointer:pane];[panes[key] removeFromSuperview];[panes removeObjectForKey:key];}
static NSMutableDictionary<NSString*,NSValue*> *windowAXImplementations;
static id windowChildren(id window,SEL selector){IMP original=(IMP)[windowAXImplementations[NSStringFromClass([window class])] pointerValue];NSArray *base=original?((id(*)(id,SEL))original)(window,selector):@[];NSMutableArray *result=[base mutableCopy]?:[NSMutableArray array];for(NSView *view in panes.allValues)if(view.window==window&&!view.isHidden)[result addObject:view];return result;}
static void exposeAccessibility(NSWindow *window){Class klass=window.class;NSString *key=NSStringFromClass(klass);if(![key hasPrefix:@"JUCEWindow"]||windowAXImplementations[key])return;Method method=class_getInstanceMethod(klass,@selector(accessibilityChildren));if(!method)return;if(!windowAXImplementations)windowAXImplementations=[NSMutableDictionary dictionary];windowAXImplementations[key]=[NSValue valueWithPointer:(const void*)method_getImplementation(method)];if(!class_addMethod(klass,@selector(accessibilityChildren),(IMP)windowChildren,method_getTypeEncoding(method))){[windowAXImplementations removeObjectForKey:key];}}
static bool bounds(void *pane,NSRect *result,NSSize *rootSize){
 char *node=(char*)pane;int x=0,y=0;bool visible=true;unsigned depth=0;
 while(node&&depth++<128){visible=visible&&(*(uint16_t*)(node+0xe8)&2);char *parent=*(char**)(node+0x30);if(!parent){*rootSize=NSMakeSize(*(int*)(node+0x40),*(int*)(node+0x44));break;}x+=*(int*)(node+0x38);y+=*(int*)(node+0x3c);node=parent;}
 int width=*(int*)((char*)pane+0x40),height=*(int*)((char*)pane+0x44);*result=NSMakeRect(x,y+44,width,MAX(0,height-44));return visible&&depth<128&&width>0&&height>44;
}
static void layout(void *pane){bool script=paneIndex(pane)<0;int count=*(int*)((char*)pane+0x54);void **children=*(void***)((char*)pane+0x48);if(count>=0&&count<=128)for(int i=0;i<count;i++){void *child=children[i];if(!script){if(*(uint16_t*)((char*)child+0xe8)&2)((void(*)(void*,bool))(*(void***)child)[0x60/8])(child,false);}else{int x=*(int*)((char*)child+0x38),y=*(int*)((char*)child+0x3c),w=*(int*)((char*)child+0x40),h=*(int*)((char*)child+0x44);int bottom=*(int*)((char*)pane+0x44)-38;if(h>40&&y+h>bottom)((void(*)(void*,int,int,int,int))(slide+0x1007ff7dcULL))(child,x,y,w,MAX(0,bottom-y));}}
 NSView *view=panes[[NSValue valueWithPointer:pane]];if(!view)return;NSRect rect;NSSize root;bool showing=bounds(pane,&rect,&root);NSWindow *window=nil;
 for(NSWindow *candidate in NSApp.windows){NSSize size=candidate.contentView.bounds.size;if(fabs(size.width-root.width)<2&&fabs(size.height-root.height)<2&&candidate.isVisible){window=candidate;break;}}
 if(!window||!showing){view.hidden=YES;return;}exposeAccessibility(window);NSView *parent=window.contentView;if(view.superview!=parent){[view removeFromSuperview];[parent addSubview:view positioned:NSWindowAbove relativeTo:nil];}if(script){rect.origin.y+=MAX(0,rect.size.height-36);rect.size.height=36;}if(!parent.isFlipped)rect.origin.y=parent.bounds.size.height-NSMaxY(rect);view.frame=rect;view.hidden=NO;loader_extend_control_refresh(view);
 if(getenv("OPEN_SV2_UI_DIAGNOSTICS")&&!objc_getAssociatedObject(view,(const void*)layout)){objc_setAssociatedObject(view,(const void*)layout,@YES,OBJC_ASSOCIATION_RETAIN_NONATOMIC);NSPoint point=NSMakePoint(NSMidX(rect),NSMidY(rect));NSView *hit=[parent hitTest:[parent convertPoint:point toView:parent.superview]];fprintf(stderr,"EXTEND_UI parent=%s target=%s frame=%s\n",NSStringFromClass(parent.class).UTF8String,NSStringFromClass(hit.class).UTF8String,NSStringFromRect(rect).UTF8String);}

}
static void capture(void *pane){int index=paneIndex(pane);
 // Hide this category's built-in ScriptPanel children. The original Scripts
 // category keeps its own pane and remains untouched.
 int count=*(int*)((char*)pane+0x54);void **children=*(void***)((char*)pane+0x48);if(count<0||count>128)return;
 if(index>=0)for(int i=0;i<count;i++){void *child=children[i];auto table=*(void***)child;((void(*)(void*,bool))table[0x60/8])(child,false);}
 if(!panes)panes=[NSMutableDictionary dictionary];NSValue *key=[NSValue valueWithPointer:pane];if(!panes[key]){panes[key]=index<0?loader_extend_scripts_view():index==0?loader_extend_control_view():loader_extend_plugin_view(@(categories[index].identifier));fprintf(stderr,"EXTEND_CATEGORY pane-created pid=%d\n",getpid());}
 ensureTimer();layout(pane);
}
static void paint(void *pane,void *graphics){((void(*)(void*,void*))(slide+0x10101b198ULL))(pane,graphics);capture(pane);}
static void resized(void *pane){((void(*)(void*))(slide+0x1010352a8ULL))(pane);capture(pane);}
static void destroy(void *pane){remove(pane);((void(*)(void*))(slide+0x101035270ULL))(pane);}
static void deletingDestroy(void *pane){remove(pane);((void(*)(void*))(slide+0x10103527cULL))(pane);}
static void barLayout(void *bar){
 ((void(*)(void*))(slide+0x101020c84ULL))(bar);if(!bars)bars=[NSMutableSet set];[bars addObject:[NSValue valueWithPointer:bar]];ensureTimer();
 void **buttons=*(void***)((char*)bar+0x220);int count=*(int*)((char*)bar+0x22c);void **entries=*(void***)(slide+0x10161f218ULL);int entryCount=*(int*)(slide+0x10161f224ULL);if(count!=entryCount||count<1||count>64)return;
 int active=0;for(int i=0;i<count;i++){auto c=fromSelf(entries[i]);if(!c||(c->present&&c->visible))active++;}
 int width=*(int*)((char*)bar+0x40),height=*(int*)((char*)bar+0x44);int row=MIN(48,MAX(12,(height-28)/MAX(1,active))),y=8;
 for(int i=0;i<count;i++){void *button=buttons[i];auto c=fromSelf(entries[i]);bool visible=!c||(c->present&&c->visible);((void(*)(void*,bool))(*(void***)button)[0x60/8])(button,visible);if(!visible){((void(*)(void*,int,int,int,int))(slide+0x1007ff7dcULL))(button,0,0,0,0);continue;}
 if(!c){const char *label=*(const char**)((char*)button+0x298);if(label&&!strcmp(label,"LICENSE"))y+=12;}
 else{uintptr_t *label=(uintptr_t*)((char*)button+0x298);if(strcmp((const char*)*label,c->label)){((void(*)(void*))(slide+0x100699298ULL))(label);*label=((uintptr_t(*)(const char*))(slide+0x100662a08ULL))(c->label);}}
 ((void(*)(void*,int,int,int,int))(slide+0x1007ff7dcULL))(button,0,y,width,row);y+=row;
 }
}
static void barPaint(void *bar,void *graphics){barLayout(bar);((void(*)(void*,void*))(slide+0x1010206fcULL))(bar,graphics);}
static void barDestroy(void *bar){[bars removeObject:[NSValue valueWithPointer:bar]];((void(*)(void*))(slide+0x101020e60ULL))(bar);}
static void barDeletingDestroy(void *bar){[bars removeObject:[NSValue valueWithPointer:bar]];((void(*)(void*))(slide+0x101020f4cULL))(bar);}
static void closeCategory(Category *category){for(NSValue *key in bars.allObjects){void *container=*(void**)((char*)key.pointerValue+0xf8);void *model=((void*(*)(void*,void*))(slide+0x101024958ULL))(container,&category->object);if(model&&!*(uint8_t*)((char*)model+0x34)){HostString reason=string("user");((void(*)(void*,void*,const void*))(slide+0x101024b0cULL))(container,&category->object,&reason);}}}
static void syncCategories(){NSDictionary *snapshot=loader_plugin_ui_call(@"ui.snapshot",@{})[@"result"];if(!snapshot||[snapshot[@"generation"] isEqual:lastGeneration])return;lastGeneration=snapshot[@"generation"];categories[0].visible=[snapshot[@"extendEntryVisible"] boolValue];if(!categories[0].visible)closeCategory(categories);
 NSMutableSet *identifiers=[NSMutableSet set];for(NSDictionary *item in snapshot[@"plugins"])[identifiers addObject:item[@"id"]];
 for(unsigned i=1;i<33;i++)categories[i].present=false;
 for(NSDictionary *plugin in snapshot[@"plugins"]){Category *category=nullptr;NSString *identifier=plugin[@"id"];for(unsigned i=1;i<33;i++)if(!strcmp(categories[i].identifier,identifier.UTF8String)){category=&categories[i];break;}
 if(!category){for(unsigned i=1;i<33;i++)if(!categories[i].identifier[0]){category=&categories[i];break;}}
 if(!category){for(unsigned i=1;i<33;i++)if(![identifiers containsObject:@(categories[i].identifier)]){category=&categories[i];closeCategory(category);for(NSValue *key in panes.allKeys)if(paneIndex(key.pointerValue)==(int)i)remove(key.pointerValue);break;}}
 if(!category)continue;strlcpy(category->identifier,identifier.UTF8String,sizeof category->identifier);strlcpy(category->label,[plugin[@"name"] UTF8String],sizeof category->label);category->present=true;category->visible=[plugin[@"visible"] boolValue];
 }
 for(unsigned i=1;i<33;i++)if(!categories[i].present||!categories[i].visible)closeCategory(&categories[i]);
 for(NSValue *key in bars.allObjects)barLayout(key.pointerValue);
}
static void ensureTimer(){if(timer)return;timer=[NSTimer timerWithTimeInterval:.25 repeats:YES block:^(NSTimer*){syncCategories();for(NSValue *key in panes.allKeys)layout(key.pointerValue);}];[NSRunLoop.mainRunLoop addTimer:timer forMode:NSRunLoopCommonModes];}
struct SlotChange {unsigned offset;void *replacement;uintptr_t expected;};
static bool changeSlots(uintptr_t address,const SlotChange *changes,size_t number){
 void **slots=(void**)(slide+address);for(size_t i=0;i<number;i++)if(slots[changes[i].offset/8]!=(void*)(slide+changes[i].expected))return false;
 mach_vm_address_t region=(uintptr_t)slots;mach_vm_size_t length=0;vm_region_basic_info_data_64_t info;mach_msg_type_number_t count=VM_REGION_BASIC_INFO_COUNT_64;mach_port_t port=MACH_PORT_NULL;kern_return_t rc=mach_vm_region(mach_task_self(),&region,&length,VM_REGION_BASIC_INFO_64,(vm_region_info_t)&info,&count,&port);if(port)mach_port_deallocate(mach_task_self(),port);if(rc!=KERN_SUCCESS||(uintptr_t)slots<region||(uintptr_t)slots+0x148>region+length)return false;
 mach_vm_address_t page=(uintptr_t)slots&~((mach_vm_address_t)vm_page_size-1);if(mach_vm_protect(mach_task_self(),page,vm_page_size,FALSE,info.protection|VM_PROT_WRITE|VM_PROT_COPY)!=KERN_SUCCESS)return false;
 for(size_t i=0;i<number;i++)slots[changes[i].offset/8]=changes[i].replacement;
 if(mach_vm_protect(mach_task_self(),page,vm_page_size,FALSE,info.protection)!=KERN_SUCCESS){for(size_t i=0;i<number;i++)slots[changes[i].offset/8]=(void*)(slide+changes[i].expected);return false;}return true;
}
static bool setSlots(){SlotChange pane[]={{0,(void*)destroy,0x101035270ULL},{8,(void*)deletingDestroy,0x10103527cULL},{0xf8,(void*)paint,0x10101b198ULL},{0x140,(void*)resized,0x1010352a8ULL}};SlotChange bar[]={{0,(void*)barDestroy,0x101020e60ULL},{8,(void*)barDeletingDestroy,0x101020f4cULL},{0xf8,(void*)barPaint,0x1010206fcULL},{0x140,(void*)barLayout,0x101020c84ULL}};return changeSlots(0x1015bf430ULL,pane,4)&&changeSlots(0x1015bdd50ULL,bar,4);}
// Native model persistence has a fixed type table. Our transient pane models
// are recreated from plugin registrations, so omit only our own saved nodes on
// the next launch. Preserve the full previous settings file before normalization.
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
 // Generated from the supported official executable; a changed build fails closed.
 static const Check checks[]={
 {0x10080a154ULL,384,"347b3531f2ca5d7ff957eaf6e1067a6f99966c28841a3b7a216f0914a17c90c5"},
 {0x1007ff7dcULL,652,"3c4b76937a224d2991e8b15dd1f421c6bb89829162131f131328e0480b83f7bf"},
 {0x101020c84ULL,328,"c569460bf8b906c8cb0d436e800575a8e609759b647aacb2e1463971647ad620"},
 {0x1010206fcULL,1272,"87c1fe1f70c255b593112b15e36c17fc6aef4b62296ff8a3a92c65e4c82cf30f"},
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
 // Toolbar drawables are created before runtime registration. Reserve stable
 // built-in slots so each starts with its own semantic icon, even when disabled.
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
