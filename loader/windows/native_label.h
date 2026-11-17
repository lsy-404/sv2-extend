#pragma once
#include "native_string.h"
#include <cmath>
namespace native_widgets_label {
struct Label {
 unsigned char *base=nullptr; void *label=nullptr,*parent=nullptr; DWORD thread=0;
 Label()=default; Label(const Label&)=delete; Label& operator=(const Label&)=delete;
 bool typed() const {if(!label||!base||!native_widget_listener::readable(label,0x278))return false;void *v=nullptr;std::memcpy(&v,label,sizeof v);return v==base+0x14fe760;}
 void release(void *s) const {if(!s||s==base+0x17903a0)return;auto p=static_cast<unsigned char*>(s);if(InterlockedExchangeAdd(reinterpret_cast<LONG*>(p-16),-1)==0)reinterpret_cast<void(*)(void*)>(base+0x10baa40)(p-16);}
 bool create(void *pane,bool(*owned)(void*),DWORD main_thread,const char *name,const char *text) {
  if(label||!pane||!owned||!main_thread||GetCurrentThreadId()!=main_thread||!owned(pane)||!native_widget_listener::readable(pane,0x38)||!name||!text)return false;
  auto image=reinterpret_cast<unsigned char*>(GetModuleHandleW(nullptr)); using native_widget_listener::signature;
  static const unsigned char alloc[]={0x40,0x53,0x48,0x83,0xec,0x20,0x48,0x8b,0xd9}, free_mem[]={0xe9,0x4b,0x93,0xd1,0xff}, string_init[]={0x40,0x53,0x56,0x48,0x83,0xec,0x28,0x48,0x8b,0xda};
  static const unsigned char ctor[]={0x48,0x89,0x5c,0x24,0x18,0x55,0x56,0x57,0x41,0x54,0x41,0x55}, dtor[]={0x48,0x89,0x5c,0x24,0x08,0x57,0x48,0x83,0xec,0x20,0x8b,0xda}, setter[]={0x48,0x89,0x5c,0x24,0x18,0x55,0x56,0x57,0x48,0x83,0xec,0x40};
  static const unsigned char add[]={0x48,0x89,0x5c,0x24,0x08,0x48,0x89,0x74,0x24,0x10,0x57,0x48}, bounds[]={0x48,0x89,0x5c,0x24,0x18,0x55,0x56,0x41,0x54,0x41,0x55,0x41}, visible[]={0x48,0x89,0x5c,0x24,0x10,0x48,0x89,0x74,0x24,0x18,0x57,0x48};
  if(!image||!native_widget_listener::supported(image)||!signature(image,0x10baa74,alloc,sizeof alloc)||!signature(image,0x10baa40,free_mem,sizeof free_mem)||!signature(image,0xe6fbd0,string_init,sizeof string_init)||!signature(image,0xcac1c0,ctor,sizeof ctor)||!signature(image,0xc82830,dtor,sizeof dtor)||!signature(image,0xcac6b0,setter,sizeof setter)||!signature(image,0xc50620,add,sizeof add)||!signature(image,0xc4f6f0,bounds,sizeof bounds)||!signature(image,0xc4e5b0,visible,sizeof visible))return false;
  base=image;thread=main_thread;auto make=[&](void *out,const char *value){return native_string::init(base,out,value);};void *a=nullptr,*b=nullptr;make(&a,name);make(&b,text);
  if(!a||!b){release(a);release(b);base=nullptr;thread=0;return false;}
  void *allocation=reinterpret_cast<void*(*)(size_t)>(base+0x10baa74)(0x278);if(allocation)reinterpret_cast<void*(*)(void*,void*,void*)>(base+0xcac1c0)(allocation,&a,&b);release(a);release(b);
  if(!allocation){base=nullptr;thread=0;return false;}label=allocation;parent=pane;if(!typed())return false;
  reinterpret_cast<void(*)(void*,void*,int)>(base+0xc50620)(pane,label,-1);void *actual=nullptr;std::memcpy(&actual,static_cast<unsigned char*>(label)+0x30,sizeof actual);if(actual!=pane){destroy();return false;}return true;
 }
 bool update(const char *text){if(thread!=GetCurrentThreadId()||!typed()||!text)return false;void *value=nullptr;native_string::init(base,&value,text);if(!value)return false;reinterpret_cast<void(*)(void*,void*,int)>(base+0xcac6b0)(label,&value,0);release(value);return true;}
 bool measure(const char *text,float &width,float &height,int &horizontal_border,int &vertical_border) const {
  if(thread!=GetCurrentThreadId()||!typed()||!text)return false;
  static const unsigned char width_code[]={0x48,0x89,0x5c,0x24,0x08,0x57,0x48,0x83,0xec,0x60,0x48,0x8b};
  static const unsigned char height_code[]={0x40,0x53,0x48,0x83,0xec,0x40,0x0f,0x29,0x74,0x24,0x30,0x0f};
  static const unsigned char default_laf[]={0x48,0x89,0x5c,0x24,0x08,0x57,0x48,0x83,0xec,0x20,0x48,0x8b};
  using native_widget_listener::readable;using native_widget_listener::signature;
  if(!signature(base,0x8ad7c0,width_code,sizeof width_code)||!signature(base,0xd45170,height_code,sizeof height_code)||!signature(base,0xc57960,default_laf,sizeof default_laf))return false;
  auto executable=[&](void *p){MEMORY_BASIC_INFORMATION m{};return p&&VirtualQuery(p,&m,sizeof m)&&m.Type==MEM_IMAGE&&m.AllocationBase==base&&m.State==MEM_COMMIT&&(m.Protect&0xff)==PAGE_EXECUTE_READ;};
  void *look=nullptr,*node=label;
  for(unsigned depth=0;node&&depth<128;++depth){
   if(!readable(node,0x70))return false;
   void *state=nullptr;std::memcpy(&state,static_cast<unsigned char*>(node)+0x68,sizeof state);
   if(state){if(!readable(state,0x18))return false;std::memcpy(&look,static_cast<unsigned char*>(state)+0x10,sizeof look);if(look)break;}
   void *next=nullptr;std::memcpy(&next,static_cast<unsigned char*>(node)+0x30,sizeof next);if(next==node)return false;node=next;
  }
  if(!look){
   if(node||!readable(base+0x17f0f38,sizeof(void*)))return false;
   void *fallback=nullptr;std::memcpy(&fallback,base+0x17f0f38,sizeof fallback);if(!fallback)return false;
   look=reinterpret_cast<void*(*)(void*)>(base+0xc57960)(fallback);
  }
  if(!readable(look,0x58))return false;
  void *methods=nullptr;std::memcpy(&methods,static_cast<unsigned char*>(look)+0x50,sizeof methods);
  if(!readable(methods,0x20))return false;
  auto table=static_cast<void**>(methods);if(!executable(table[2])||!executable(table[3]))return false;
  void *value=nullptr;if(!native_string::init(base,&value,text)||!value)return false;
  void *font=nullptr;struct Border {int top,left,bottom,right;} border{};
  reinterpret_cast<void*(*)(void*,void*,void*)>(table[2])(static_cast<unsigned char*>(look)+0x50,&font,label);
  reinterpret_cast<void*(*)(void*,void*,void*)>(table[3])(static_cast<unsigned char*>(look)+0x50,&border,label);
  if(!readable(font,16)){release(value);return false;}
  void *font_table=nullptr;std::memcpy(&font_table,font,sizeof font_table);if(!readable(font_table,sizeof(void*))||!executable(*static_cast<void**>(font_table))){release(value);return false;}
  float measured_width=reinterpret_cast<float(*)(void*,void*)>(base+0x8ad7c0)(&font,value);
  float measured_height=reinterpret_cast<float(*)(void*)>(base+0xd45170)(&font);
  release(value);
  if(InterlockedExchangeAdd(reinterpret_cast<LONG*>(static_cast<unsigned char*>(font)+8),-1)==1)reinterpret_cast<void(*)(void*,unsigned)>(*static_cast<void**>(font_table))(font,1);
  if(!std::isfinite(measured_width)||!std::isfinite(measured_height)||measured_width<0||measured_height<=0||border.left<0||border.right<0||border.top<0||border.bottom<0||border.left>32768||border.right>32768||border.top>32768||border.bottom>32768)return false;
  width=measured_width;height=measured_height;horizontal_border=border.left+border.right;vertical_border=border.top+border.bottom;return true;
 }
 bool layout(int x,int y,int w,int h){if(thread!=GetCurrentThreadId()||!typed()||w<=0||h<=0)return false;void *p=nullptr;std::memcpy(&p,static_cast<unsigned char*>(label)+0x30,sizeof p);if(p!=parent)return false;reinterpret_cast<void(*)(void*,int,int,int,int)>(base+0xc4f6f0)(label,x,y,w,h);reinterpret_cast<void(*)(void*,bool)>(base+0xc4e5b0)(label,true);return true;}
 void *component() const {return thread==GetCurrentThreadId()&&typed()?label:nullptr;}
 bool destroy(){if(!label)return true;if(thread!=GetCurrentThreadId()||!typed())return false;reinterpret_cast<void*(*)(void*,unsigned)>(base+0xc82830)(label,1);base=nullptr;label=nullptr;parent=nullptr;thread=0;return true;}
};
}
