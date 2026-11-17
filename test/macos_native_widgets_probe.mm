#import <Foundation/Foundation.h>
#import <mach-o/dyld.h>
#import <mach-o/loader.h>
#include "../loader/macos/native_widgets.h"
#include <cstdio>
#include <cstdlib>

namespace {
void emit(bool ok){const char *path=getenv("EXTEND_WIDGET_PROBE_RESULT");if(path){FILE *f=fopen(path,"w");if(f){fprintf(f,"%s\n",ok?"passed":"failed");fclose(f);}}}
void run(){
 const uint8_t expected[]={0xac,0xfe,0xfc,0x17,0x25,0xc2,0x34,0xd5,0xb2,0x8a,0xe8,0xbf,0x6f,0xfb,0x32,0xb4};bool found=false;
 for(unsigned i=0;i<_dyld_image_count();++i){auto h=(const mach_header_64*)_dyld_get_image_header(i);if(h->filetype!=MH_EXECUTE)continue;auto c=(const load_command*)(h+1);for(unsigned n=0;n<h->ncmds;++n){if(c->cmd==LC_UUID&&memcmp(((const uuid_command*)c)->uuid,expected,sizeof expected)==0){native_widgets::initialize(_dyld_get_image_vmaddr_slide(i));found=true;break;}c=(const load_command*)((const char*)c+c->cmdsize);}break;}
 if(!found){emit(false);return;}
 using namespace native_widgets;native_widgets::Component parent,content;Viewport viewport;Button button,toggle;Label label;Choice choice;TextEditor editor;int clicks=0;
 bool ok=parent.create()&&content.create()&&viewport.create(parent.object)&&button.create(content.object,"Probe",false,[&]{clicks++;})&&toggle.create(content.object,"Toggle",true,[]{})&&label.create(content.object,"Text")&&choice.create(content.object)&&editor.create(content.object,"文本 ü");
 if(!ok){emit(false);return;}toggle.set(true);choice.item("One",1);choice.item("Two",2);choice.set(1);ok=ok&&choice.get()==1; ((void(*)(int,void*))(slide+0x100856428ULL))(2,choice.object);ok=ok&&choice.get()==2;viewport.content(content.object);bounds(parent.object,0,0,240,300);bounds(viewport.object,0,0,240,300);bounds(content.object,0,0,240,600);bounds(toggle.object,10,10,180,25);visible(viewport.object,true);visible(toggle.object,true);
 ok=ok&&toggle.get()&&choice.get()==2&&editor.get()=="文本 ü"&&*(void**)((char*)content.object+0x30)==(char*)viewport.object+0x110&&*(void**)((char*)toggle.object+0x30)==content.object;
 ((void(*)(void*,void*))(*(void***)button.object)[0x1b0/8])(button.object,nullptr);ok=ok&&clicks==1;
 editor.destroy();choice.destroy();label.destroy();toggle.destroy();button.destroy();viewport.destroy();ok=ok&&*(void**)((char*)content.object+0x30)==nullptr;content.destroy();parent.destroy();emit(ok);
}
}
__attribute__((constructor)) static void load(){dispatch_async(dispatch_get_main_queue(),^{run();});}
