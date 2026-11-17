#pragma once
#include "native_string.h"
#include <string>


namespace native_widgets_detail {
inline bool caption(unsigned char *base,void *button,const char *value){if(!base||!button||!value)return false;void *text=nullptr;native_string::init(base,&text,value);if(!text)return false;reinterpret_cast<void(*)(void*,void*)>(base+0xc47cb0)(button,&text);auto p=static_cast<unsigned char*>(text);if(p==base+0x17903a0)return true;if(reinterpret_cast<uintptr_t>(p)<16||!native_widget_listener::readable(p-16,sizeof(LONG)))return false;if(InterlockedExchangeAdd(reinterpret_cast<LONG*>(p-16),-1)==0)reinterpret_cast<void(*)(void*)>(base+0x10baa40)(p-16);return true;}
}
namespace native_widgets_toggle {
struct Toggle {
 unsigned char *base=nullptr;
 void *toggle=nullptr,*parent=nullptr;
 DWORD thread=0;
 native_widget_listener::Binding listener;
 bool create(void *pane,bool(*owned)(void*),DWORD main_thread,const char *caption,native_widget_listener::Enqueue enqueue,void *context){
  if(toggle||!pane||!owned||!main_thread||GetCurrentThreadId()!=main_thread||!owned(pane)||!caption||!enqueue)return false;
  auto image=reinterpret_cast<unsigned char*>(GetModuleHandleW(nullptr));
  static const unsigned char ctor[]={0x48,0x89,0x4c,0x24,0x08,0x53,0x48,0x83,0xec,0x20};static const unsigned char text_set[]={0x48,0x89,0x5c,0x24,0x08,0x48,0x89,0x74,0x24,0x10,0x57,0x48};static const unsigned char allocate[]={0x40,0x53,0x48,0x83,0xec,0x20,0x48,0x8b,0xd9};static const unsigned char free_mem[]={0xe9,0x4b,0x93,0xd1,0xff};static const unsigned char string_init[]={0x40,0x53,0x56,0x48,0x83,0xec,0x28,0x48,0x8b,0xda};static const unsigned char add[]={0x48,0x89,0x5c,0x24,0x08,0x48,0x89,0x74,0x24,0x10,0x57,0x48};static const unsigned char bounds[]={0x48,0x89,0x5c,0x24,0x18,0x55,0x56,0x41,0x54,0x41,0x55,0x41};static const unsigned char visible[]={0x48,0x89,0x5c,0x24,0x10,0x48,0x89,0x74,0x24,0x18,0x57,0x48};using native_widget_listener::signature;
  if(!native_widget_listener::supported(image)||!native_widget_listener::readable(pane,0x38)||!signature(image,0xc4a8f0,ctor,sizeof ctor)||!signature(image,0xc47cb0,text_set,sizeof text_set)||!signature(image,0x10baa74,allocate,sizeof allocate)||!signature(image,0x10baa40,free_mem,sizeof free_mem)||!signature(image,0xe6fbd0,string_init,sizeof string_init)||!signature(image,0xc50620,add,sizeof add)||!signature(image,0xc4f6f0,bounds,sizeof bounds)||!signature(image,0xc4e5b0,visible,sizeof visible))return false;
  auto allocation=reinterpret_cast<void*(*)(size_t)>(image+0x10baa74)(0x238);
  if(!allocation)return false;
  reinterpret_cast<void*(*)(void*)>(image+0xc4a8f0)(allocation);
  void *table=nullptr;std::memcpy(&table,allocation,sizeof table);
  if(table!=image+0x14fc7b8)return false;
  base=image;toggle=allocation;parent=pane;thread=main_thread;
  void *text=nullptr;native_string::init(image,&text,caption);
  if(!text){destroy();return false;}
  reinterpret_cast<void(*)(void*,void*)>(image+0xc47cb0)(toggle,&text);
  auto p=static_cast<unsigned char*>(text);if(p!=image+0x17903a0&&InterlockedExchangeAdd(reinterpret_cast<LONG*>(p-16),-1)==0)reinterpret_cast<void(*)(void*)>(image+0x10baa40)(p-16);
  if(!native_widget_listener::bind_toggle(listener,toggle,enqueue,context)){destroy();return false;}
  reinterpret_cast<void(*)(void*,void*,int)>(image+0xc50620)(pane,toggle,-1);
  void *actual=nullptr;std::memcpy(&actual,static_cast<unsigned char*>(toggle)+0x30,sizeof actual);if(actual!=pane){destroy();return false;}
  return true;
 }
 void *component()const{return thread==GetCurrentThreadId()&&native_widget_listener::toggle_valid(base,toggle)?toggle:nullptr;}
 bool set_caption(const char *value){return component()&&native_widgets_detail::caption(base,toggle,value);}
 bool layout(int x,int y,int width,int height){if(thread!=GetCurrentThreadId()||!native_widget_listener::toggle_valid(base,toggle)||width<=0||height<=0)return false;void *actual=nullptr;std::memcpy(&actual,static_cast<unsigned char*>(toggle)+0x30,sizeof actual);if(actual!=parent)return false;reinterpret_cast<void(*)(void*,int,int,int,int)>(base+0xc4f6f0)(toggle,x,y,width,height);reinterpret_cast<void(*)(void*,bool)>(base+0xc4e5b0)(toggle,true);return true;}
 bool destroy(){if(!toggle)return true;if(thread!=GetCurrentThreadId()||!native_widget_listener::readable(toggle,sizeof(void*)))return false;auto table=*static_cast<void***>(toggle);if(table!=reinterpret_cast<void**>(base+0x14fc7b8))return false;native_widget_listener::deactivate(listener);reinterpret_cast<void*(*)(void*,unsigned)>(table[0])(toggle,1);toggle=nullptr;parent=nullptr;base=nullptr;thread=0;listener.image=nullptr;listener.button=nullptr;listener.thread=0;return true;}
};
}


namespace native_widgets_button {
using OwnedPane = bool (*)(void *);
struct Button {
 unsigned char *base=nullptr;
 DWORD thread=0;
 void *button=nullptr;
 void *parent=nullptr;
 native_widget_listener::Binding listener;
 Button()=default;
 Button(const Button&)=delete;
 Button& operator=(const Button&)=delete;
 bool create(void *pane,OwnedPane owned,DWORD main_thread,const char *caption,native_widget_listener::Enqueue enqueue,void *context){
  if(button||!pane||!owned||!main_thread||GetCurrentThreadId()!=main_thread||!owned(pane)||!caption||!enqueue)return false;
  auto image=reinterpret_cast<unsigned char*>(GetModuleHandleW(nullptr));
  static const unsigned char alloc[]={0x40,0x53,0x48,0x83,0xec,0x20,0x48,0x8b,0xd9};
  static const unsigned char free_mem[]={0xe9,0x4b,0x93,0xd1,0xff};
  static const unsigned char string_init[]={0x40,0x53,0x56,0x48,0x83,0xec,0x28,0x48,0x8b,0xda};
  static const unsigned char ctor[]={0x48,0x89,0x5c,0x24,0x18,0x48,0x89,0x4c,0x24,0x08,0x56,0x57};
  static const unsigned char dtor[]={0x48,0x89,0x5c,0x24,0x08,0x57,0x48,0x83,0xec,0x20};
  static const unsigned char add[]={0x48,0x89,0x5c,0x24,0x08,0x48,0x89,0x74,0x24,0x10,0x57,0x48};
  static const unsigned char text_set[]={0x48,0x89,0x5c,0x24,0x08,0x48,0x89,0x74,0x24,0x10,0x57,0x48};
  static const unsigned char bounds[]={0x48,0x89,0x5c,0x24,0x18,0x55,0x56,0x41,0x54,0x41,0x55,0x41};
  static const unsigned char visible[]={0x48,0x89,0x5c,0x24,0x10,0x48,0x89,0x74,0x24,0x18,0x57,0x48};
  using native_widget_listener::signature;using native_widget_listener::readable;
  if(!native_widget_listener::supported(image)||!readable(pane,0x38)||!signature(image,0x10baa74,alloc,sizeof alloc)||!signature(image,0x10baa40,free_mem,sizeof free_mem)||!signature(image,0xe6fbd0,string_init,sizeof string_init)||!signature(image,0xc47860,ctor,sizeof ctor)||!signature(image,0xc47cb0,text_set,sizeof text_set)||!signature(image,0xbdccd0,dtor,sizeof dtor)||!signature(image,0xc50620,add,sizeof add)||!signature(image,0xc4f6f0,bounds,sizeof bounds)||!signature(image,0xc4e5b0,visible,sizeof visible)||!readable(image+0x14fd090,0x1b0)||!readable(image+0x14f62d8,sizeof(void*)))return false;
  void *text=nullptr;native_string::init(image,&text,caption);if(!text)return false;
  void *allocation=reinterpret_cast<void *(*)(size_t)>(image+0x10baa74)(0x238);
  if(allocation)reinterpret_cast<void *(*)(void *,void *)>(image+0xc47860)(allocation,&text);
  auto chars=static_cast<unsigned char*>(text);if(chars!=image+0x17903a0&&readable(chars-16,sizeof(LONG))&&InterlockedExchangeAdd(reinterpret_cast<LONG*>(chars-16),-1)==0)reinterpret_cast<void (*)(void*)>(image+0x10baa40)(chars-16);
  if(!allocation)return false;
  auto bytes=static_cast<unsigned char*>(allocation);void *vtable=image+0x14fd090,*secondary=image+0x14f62d8;std::memcpy(bytes,&vtable,sizeof vtable);std::memcpy(bytes+0xf0,&secondary,sizeof secondary);
  base=image;thread=main_thread;button=allocation;parent=pane;
  if(!native_widget_listener::bind_text_button(listener,button,enqueue,context)){destroy();return false;}
  reinterpret_cast<void (*)(void *,void *,int)>(base+0xc50620)(pane,button,-1);
  void *actual=nullptr;std::memcpy(&actual,bytes+0x30,sizeof actual);if(actual!=pane){destroy();return false;}
  return true;
 }
 void *component()const{if(thread!=GetCurrentThreadId()||!base||!native_widget_listener::readable(button,0x38))return nullptr;void *table=nullptr;std::memcpy(&table,button,sizeof table);return table==base+0x14fd090?button:nullptr;}
 bool set_caption(const char *value){return component()&&native_widgets_detail::caption(base,button,value);}
 bool layout(int x,int y,int width,int height){if(!button||thread!=GetCurrentThreadId()||width<=0||height<=0)return false;void *actual=nullptr;if(!native_widget_listener::readable(button,0x38))return false;void *table=nullptr;std::memcpy(&table,button,sizeof table);if(table!=base+0x14fd090)return false;std::memcpy(&actual,static_cast<unsigned char*>(button)+0x30,sizeof actual);if(actual!=parent)return false;reinterpret_cast<void (*)(void *,int,int,int,int)>(base+0xc4f6f0)(button,x,y,width,height);reinterpret_cast<void (*)(void *,bool)>(base+0xc4e5b0)(button,true);return true;}
 bool destroy(){if(!button)return true;if(thread!=GetCurrentThreadId()||!native_widget_listener::readable(button,sizeof(void*)))return false;void *table=nullptr;std::memcpy(&table,button,sizeof table);if(table!=base+0x14fd090)return false;native_widget_listener::deactivate(listener);reinterpret_cast<void *(*)(void *,unsigned)>(base+0xbdccd0)(button,1);listener.image=nullptr;listener.button=nullptr;listener.thread=0;button=nullptr;parent=nullptr;base=nullptr;thread=0;return true;}
};
}

namespace native_widgets_choice {
inline bool code(unsigned char *base,size_t rva,const unsigned char *bytes,size_t n) {
 MEMORY_BASIC_INFORMATION m{};
 return base && VirtualQuery(base+rva,&m,sizeof m) && m.Type==MEM_IMAGE && m.AllocationBase==base && m.State==MEM_COMMIT && (m.Protect&0xff)==PAGE_EXECUTE_READ && n<=m.RegionSize-(reinterpret_cast<uintptr_t>(base+rva)-reinterpret_cast<uintptr_t>(m.BaseAddress)) && !std::memcmp(base+rva,bytes,n);
}
struct Choice {
 unsigned char *base=nullptr; void *choice=nullptr,*parent=nullptr; DWORD thread=0; int count=0;
 Choice()=default; Choice(const Choice&)=delete; Choice& operator=(const Choice&)=delete;
 bool typed() const { if(!choice||!base||!native_widget_listener::readable(choice,0x38))return false; void *v=nullptr;std::memcpy(&v,choice,sizeof v);return v==base+0x14f58b0; }
 void release_string(void *value) const { if(!value||value==base+0x17903a0)return;auto p=static_cast<unsigned char*>(value);if(InterlockedExchangeAdd(reinterpret_cast<LONG*>(p-16),-1)==0)reinterpret_cast<void(*)(void*)>(base+0x10baa40)(p-16); }
 bool create(void *pane,bool(*owned)(void*),DWORD main_thread,const char *name,const char *const *items,int n) {
  if(choice||!pane||!owned||!main_thread||GetCurrentThreadId()!=main_thread||!owned(pane)||!name||!items||n<=0||n>128)return false;
  for(int i=0;i<n;++i)if(!items[i])return false;
  auto image=reinterpret_cast<unsigned char*>(GetModuleHandleW(nullptr));
  static const unsigned char alloc[]={0x40,0x53,0x48,0x83,0xec,0x20,0x48,0x8b,0xd9}, free_mem[]={0xe9,0x4b,0x93,0xd1,0xff};
  static const unsigned char string_init[]={0x40,0x53,0x56,0x48,0x83,0xec,0x28,0x48,0x8b,0xda}, ctor[]={0x48,0x89,0x5c,0x24,0x10,0x48,0x89,0x4c,0x24,0x08,0x55,0x56};
  static const unsigned char dtor[]={0x48,0x89,0x5c,0x24,0x08,0x57,0x48,0x83,0xec,0x20,0x8b,0xda}, additem[]={0x40,0x55,0x53,0x56,0x57,0x41,0x56,0x48,0x8d,0x6c,0x24,0xa0};
  static const unsigned char getter[]={0x48,0x89,0x5c,0x24,0x10,0x48,0x89,0x6c,0x24,0x18,0x56,0x57}, setter[]={0x48,0x89,0x5c,0x24,0x10,0x48,0x89,0x6c,0x24,0x18,0x48,0x89};
  static const unsigned char add[]={0x48,0x89,0x5c,0x24,0x08,0x48,0x89,0x74,0x24,0x10,0x57,0x48}, bounds[]={0x48,0x89,0x5c,0x24,0x18,0x55,0x56,0x41,0x54,0x41,0x55,0x41}, visible[]={0x48,0x89,0x5c,0x24,0x10,0x48,0x89,0x74,0x24,0x18,0x57,0x48};
  if(!image||!native_widget_listener::supported(image)||!code(image,0x10baa74,alloc,sizeof alloc)||!code(image,0x10baa40,free_mem,sizeof free_mem)||!code(image,0xe6fbd0,string_init,sizeof string_init)||!code(image,0xca9c80,ctor,sizeof ctor)||!code(image,0xca9e40,dtor,sizeof dtor)||!code(image,0xca2cf0,additem,sizeof additem)||!code(image,0xcaa460,getter,sizeof getter)||!code(image,0xcaa700,setter,sizeof setter)||!code(image,0xc50620,add,sizeof add)||!code(image,0xc4f6f0,bounds,sizeof bounds)||!code(image,0xc4e5b0,visible,sizeof visible))return false;
  base=image;thread=main_thread;auto make=[&](void *out,const char *value){return native_string::init(base,out,value);};void *title=nullptr;make(&title,name);if(!title){base=nullptr;thread=0;return false;}
  void *allocation=reinterpret_cast<void*(*)(size_t)>(base+0x10baa74)(0x1f8);
  if(allocation)reinterpret_cast<void*(*)(void*,void*)>(base+0xca9c80)(allocation,&title);
  release_string(title);if(!allocation){base=nullptr;thread=0;return false;}
  choice=allocation;parent=pane;if(!typed())return false;
  for(int i=0;i<n;++i){void *text=nullptr;make(&text,items[i]);if(!text){destroy();return false;}reinterpret_cast<void(*)(void*,int,void*,bool,bool)>(base+0xca2cf0)(static_cast<unsigned char*>(choice)+0x158,i+1,&text,true,false);release_string(text);}
  count=n;reinterpret_cast<void(*)(void*,void*,int)>(base+0xc50620)(pane,choice,-1);
  void *actual=nullptr;std::memcpy(&actual,static_cast<unsigned char*>(choice)+0x30,sizeof actual);if(actual!=pane){destroy();return false;}return true;
 }
 void *component()const{return thread==GetCurrentThreadId()&&typed()?choice:nullptr;}
 bool layout(int x,int y,int w,int h){if(thread!=GetCurrentThreadId()||!typed()||w<=0||h<=0)return false;void *p=nullptr;std::memcpy(&p,static_cast<unsigned char*>(choice)+0x30,sizeof p);if(p!=parent)return false;reinterpret_cast<void(*)(void*,int,int,int,int)>(base+0xc4f6f0)(choice,x,y,w,h);reinterpret_cast<void(*)(void*,bool)>(base+0xc4e5b0)(choice,true);return true;}
 bool set(int index){if(thread!=GetCurrentThreadId()||!typed()||index<0||index>=count)return false;reinterpret_cast<void(*)(void*,int,int)>(base+0xcaa700)(choice,index+1,3);return true;}
 bool get(int &index) const {if(thread!=GetCurrentThreadId()||!typed())return false;index=reinterpret_cast<int(*)(void*)>(base+0xcaa460)(choice);return index>=-1&&index<count;}
 bool destroy(){if(!choice)return true;if(thread!=GetCurrentThreadId()||!typed())return false;reinterpret_cast<void*(*)(void*,unsigned)>(base+0xca9e40)(choice,1);base=nullptr;choice=nullptr;parent=nullptr;thread=0;count=0;return true;}
};
}

namespace native_widgets_textbox {
struct TextBox {
 unsigned char *base=nullptr;
 void *object=nullptr,*parent=nullptr;
 DWORD thread=0;
 TextBox()=default;TextBox(const TextBox&)=delete;TextBox& operator=(const TextBox&)=delete;
 bool typed()const{if(!base||!native_widget_listener::readable(object,0x3b8))return false;auto p=static_cast<unsigned char*>(object);void *first=nullptr,*second=nullptr;std::memcpy(&first,p,sizeof first);std::memcpy(&second,p+8,sizeof second);return first==base+0x14d2c90&&second==base+0x14d2d20;}
 bool release(void *text)const{if(!text)return false;auto p=static_cast<unsigned char*>(text);if(p==base+0x17903a0)return true;if(reinterpret_cast<uintptr_t>(p)<16||!native_widget_listener::readable(p-16,sizeof(LONG)))return false;if(InterlockedExchangeAdd(reinterpret_cast<LONG*>(p-16),-1)==0)reinterpret_cast<void(*)(void*)>(base+0x10baa40)(p-16);return true;}
 bool create(void *pane,bool(*owned)(void*),DWORD main_thread,const char *initial){
  if(object||!pane||!owned||!owned(pane)||!main_thread||GetCurrentThreadId()!=main_thread||!initial)return false;
  auto image=reinterpret_cast<unsigned char*>(GetModuleHandleW(nullptr));
  static const unsigned char alloc[]={0x40,0x53,0x48,0x83,0xec,0x20,0x48,0x8b,0xd9};static const unsigned char free_mem[]={0xe9,0x4b,0x93,0xd1,0xff};static const unsigned char string_init[]={0x40,0x53,0x56,0x48,0x83,0xec,0x28,0x48,0x8b,0xda};static const unsigned char ctor[]={0x48,0x89,0x5c,0x24,0x10,0x48,0x89,0x4c,0x24,0x08,0x55,0x56};static const unsigned char dtor[]={0x48,0x89,0x5c,0x24,0x08,0x57,0x48,0x83,0xec,0x20,0x8b,0xda};static const unsigned char set[]={0x48,0x89,0x5c,0x24,0x18,0x55,0x56,0x57,0x41,0x54,0x41,0x55};static const unsigned char get[]={0x48,0x89,0x5c,0x24,0x08,0x48,0x89,0x74,0x24,0x10,0x48,0x89};static const unsigned char add[]={0x48,0x89,0x5c,0x24,0x08,0x48,0x89,0x74,0x24,0x10,0x57,0x48};static const unsigned char bounds[]={0x48,0x89,0x5c,0x24,0x18,0x55,0x56,0x41,0x54,0x41,0x55,0x41};static const unsigned char visible[]={0x48,0x89,0x5c,0x24,0x10,0x48,0x89,0x74,0x24,0x18,0x57,0x48};
  using native_widget_listener::signature;using native_widget_listener::readable;
  if(!native_widget_listener::supported(image)||!readable(pane,0x38)||!signature(image,0x10baa74,alloc,sizeof alloc)||!signature(image,0x10baa40,free_mem,sizeof free_mem)||!signature(image,0xe6fbd0,string_init,sizeof string_init)||!signature(image,0xae08b0,ctor,sizeof ctor)||!signature(image,0xae0ab0,dtor,sizeof dtor)||!signature(image,0xcc08b0,set,sizeof set)||!signature(image,0xcbd5c0,get,sizeof get)||!signature(image,0xc50620,add,sizeof add)||!signature(image,0xc4f6f0,bounds,sizeof bounds)||!signature(image,0xc4e5b0,visible,sizeof visible))return false;
  base=image;thread=main_thread;parent=pane;void *text=nullptr;native_string::init(base,&text,initial);if(!text){base=nullptr;thread=0;parent=nullptr;return false;}
  auto allocation=reinterpret_cast<void*(*)(size_t)>(base+0x10baa74)(0x3d0);if(!allocation){release(text);base=nullptr;thread=0;parent=nullptr;return false;}
  object=reinterpret_cast<void*(*)(void*,void*)>(base+0xae08b0)(allocation,&text);release(text);if(object!=allocation||!typed())return false;
  if(!set_text(initial)){destroy();return false;}auto component=static_cast<unsigned char*>(object)+8;reinterpret_cast<void(*)(void*,void*,int)>(base+0xc50620)(pane,component,-1);void *actual=nullptr;std::memcpy(&actual,component+0x30,sizeof actual);if(actual!=pane){destroy();return false;}return true;
 }
 bool set_text(const char *value){if(!value||thread!=GetCurrentThreadId()||!typed())return false;void *text=nullptr;native_string::init(base,&text,value);if(!text)return false;reinterpret_cast<void(*)(void*,void*,bool)>(base+0xcc08b0)(object,&text,false);return release(text);}
 bool get_text(std::string &out)const{if(thread!=GetCurrentThreadId()||!typed())return false;void *document=nullptr;std::memcpy(&document,static_cast<unsigned char*>(object)+0x3b0,sizeof document);if(!native_widget_listener::readable(document,0x90))return false;void *text=nullptr;reinterpret_cast<void*(*)(void*,void*)>(base+0xcbd5c0)(static_cast<unsigned char*>(document)+0x60,&text);if(!text)return false;std::string copy;auto p=static_cast<const char*>(text);bool terminated=false;for(size_t i=0;i<65536;++i){if(!native_widget_listener::readable(p+i,1))break;if(!p[i]){terminated=true;break;}copy.push_back(p[i]);}bool released=release(text);if(!terminated||!released)return false;out=std::move(copy);return true;}
 void *component()const{return thread==GetCurrentThreadId()&&typed()?static_cast<unsigned char*>(object)+8:nullptr;}
 bool layout(int x,int y,int width,int height){if(thread!=GetCurrentThreadId()||!typed()||width<=0||height<=0)return false;auto component=static_cast<unsigned char*>(object)+8;void *actual=nullptr;std::memcpy(&actual,component+0x30,sizeof actual);if(actual!=parent)return false;reinterpret_cast<void(*)(void*,int,int,int,int)>(base+0xc4f6f0)(component,x,y,width,height);reinterpret_cast<void(*)(void*,bool)>(base+0xc4e5b0)(component,true);return true;}
 bool destroy(){if(!object)return true;if(thread!=GetCurrentThreadId()||!typed())return false;reinterpret_cast<void*(*)(void*,unsigned)>(base+0xae0ab0)(object,1);object=nullptr;parent=nullptr;base=nullptr;thread=0;return true;}
};
}

namespace native_widgets {
using Toggle=native_widgets_toggle::Toggle;
using Button=native_widgets_button::Button;
using Choice=native_widgets_choice::Choice;
using TextBox=native_widgets_textbox::TextBox;
}
