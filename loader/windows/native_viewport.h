#pragma once
#include "native_widget_listener.h"

namespace native_viewport {
using OwnedPane=bool(*)(void*);
inline bool typed(void *object,unsigned char *base,size_t offset){if(!base||!native_widget_listener::readable(object,sizeof(void*)))return false;void *table=nullptr;std::memcpy(&table,object,sizeof table);return table==base+offset;}
inline bool supported(unsigned char *base){
 using native_widget_listener::signature;
 static const unsigned char alloc[]={0x40,0x53,0x48,0x83,0xec,0x20,0x48,0x8b,0xd9};
 static const unsigned char component[]={0x48,0x8d,0x05,0x29,0x80,0x8a,0x00,0x48,0x89,0x01};
 static const unsigned char component_destroy[]={0x48,0x89,0x5c,0x24,0x08,0x57,0x48,0x83,0xec,0x20};
 static const unsigned char viewport[]={0x48,0x89,0x5c,0x24,0x10,0x48,0x89,0x4c,0x24,0x08};
 static const unsigned char viewport_destroy[]={0x48,0x89,0x5c,0x24,0x08,0x57,0x48,0x83,0xec,0x20};
 static const unsigned char viewed[]={0x48,0x89,0x5c,0x24,0x10,0x48,0x89,0x6c,0x24,0x18};
 static const unsigned char string[]={0x40,0x57,0x41,0x56,0x48,0x83,0xec,0x28};
 static const unsigned char free_mem[]={0xe9,0x4b,0x93,0xd1,0xff};
 static const unsigned char add[]={0x48,0x89,0x5c,0x24,0x08,0x48,0x89,0x74,0x24,0x10,0x57,0x48};
 static const unsigned char bounds[]={0x48,0x89,0x5c,0x24,0x18,0x55,0x56,0x41,0x54,0x41,0x55,0x41};
 static const unsigned char visible[]={0x48,0x89,0x5c,0x24,0x10,0x48,0x89,0x74,0x24,0x18,0x57,0x48};
 return base&&base==reinterpret_cast<unsigned char*>(GetModuleHandleW(nullptr))&&native_widget_listener::supported(base)
  &&signature(base,0x10baa74,alloc,sizeof alloc)&&signature(base,0x10baa40,free_mem,sizeof free_mem)
  &&signature(base,0xc4e000,component,sizeof component)&&signature(base,0x9cf6f0,component_destroy,sizeof component_destroy)
  &&signature(base,0xc78110,viewport,sizeof viewport)&&signature(base,0xc78490,viewport_destroy,sizeof viewport_destroy)
  &&signature(base,0xc786b0,viewed,sizeof viewed)&&signature(base,0xe6fac0,string,sizeof string)
  &&signature(base,0xc50620,add,sizeof add)&&signature(base,0xc4f6f0,bounds,sizeof bounds)&&signature(base,0xc4e5b0,visible,sizeof visible);
}
struct Component {
 unsigned char *base=nullptr;DWORD thread=0;void *object=nullptr;
 Component()=default;Component(const Component&)=delete;Component& operator=(const Component&)=delete;
 bool create(DWORD main_thread){if(object||!main_thread||GetCurrentThreadId()!=main_thread)return false;auto image=reinterpret_cast<unsigned char*>(GetModuleHandleW(nullptr));if(!supported(image))return false;void *value=reinterpret_cast<void*(*)(size_t)>(image+0x10baa74)(0xf0);if(!value)return false;reinterpret_cast<void*(*)(void*)>(image+0xc4e000)(value);base=image;thread=main_thread;object=value;return typed(object,base,0x14f6030);}
 bool layout(int x,int y,int width,int height){if(!object||thread!=GetCurrentThreadId()||!typed(object,base,0x14f6030)||width<=0||height<=0)return false;reinterpret_cast<void(*)(void*,int,int,int,int)>(base+0xc4f6f0)(object,x,y,width,height);reinterpret_cast<void(*)(void*,bool)>(base+0xc4e5b0)(object,true);return true;}
 bool destroy(){if(!object)return true;if(thread!=GetCurrentThreadId()||!typed(object,base,0x14f6030)||!native_widget_listener::readable(object,0x38))return false;void *parent=nullptr;std::memcpy(&parent,static_cast<unsigned char*>(object)+0x30,sizeof parent);if(parent)return false;reinterpret_cast<void*(*)(void*,unsigned)>(base+0x9cf6f0)(object,1);object=nullptr;base=nullptr;thread=0;return true;}
};
struct Viewport {
 unsigned char *base=nullptr;DWORD thread=0;void *object=nullptr,*parent=nullptr,*content=nullptr;
 Viewport()=default;Viewport(const Viewport&)=delete;Viewport& operator=(const Viewport&)=delete;
 bool create(void *pane,OwnedPane owned,DWORD main_thread){
  if(object||!pane||!owned||!main_thread||GetCurrentThreadId()!=main_thread||!owned(pane)||!native_widget_listener::readable(pane,0x38))return false;
  auto image=reinterpret_cast<unsigned char*>(GetModuleHandleW(nullptr));if(!supported(image))return false;
  void *name=nullptr;reinterpret_cast<void*(*)(void*,const char*)>(image+0xe6fac0)(&name,"");if(!name)return false;
  void *value=reinterpret_cast<void*(*)(size_t)>(image+0x10baa74)(0x238);if(value)reinterpret_cast<void*(*)(void*,void*)>(image+0xc78110)(value,&name);
  auto chars=static_cast<unsigned char*>(name);if(chars!=image+0x17903a0&&native_widget_listener::readable(chars-16,sizeof(LONG))&&InterlockedExchangeAdd(reinterpret_cast<LONG*>(chars-16),-1)==0)reinterpret_cast<void(*)(void*)>(image+0x10baa40)(chars-16);
  if(!value)return false;base=image;thread=main_thread;object=value;parent=pane;if(!typed(object,base,0x14f9968))return false;
  reinterpret_cast<void(*)(void*,void*,int)>(base+0xc50620)(pane,object,-1);void *actual=nullptr;std::memcpy(&actual,static_cast<unsigned char*>(object)+0x30,sizeof actual);if(actual!=pane){destroy();return false;}return true;
 }
 bool set_content(void *child,bool take_ownership=false){if(take_ownership||!object||!child||content||thread!=GetCurrentThreadId()||!typed(object,base,0x14f9968)||!native_widget_listener::readable(child,0x38))return false;void *actual=nullptr;std::memcpy(&actual,static_cast<unsigned char*>(child)+0x30,sizeof actual);if(actual)return false;reinterpret_cast<void(*)(void*,void*,bool)>(base+0xc786b0)(object,child,take_ownership);std::memcpy(&actual,static_cast<unsigned char*>(child)+0x30,sizeof actual);if(actual!=static_cast<unsigned char*>(object)+0x110)return false;content=child;return true;}
 bool layout(int x,int y,int width,int height){if(!object||thread!=GetCurrentThreadId()||!typed(object,base,0x14f9968)||width<=0||height<=0)return false;reinterpret_cast<void(*)(void*,int,int,int,int)>(base+0xc4f6f0)(object,x,y,width,height);reinterpret_cast<void(*)(void*,bool)>(base+0xc4e5b0)(object,true);return true;}
 bool destroy(){if(!object)return true;if(thread!=GetCurrentThreadId()||!typed(object,base,0x14f9968))return false;reinterpret_cast<void*(*)(void*,unsigned)>(base+0xc78490)(object,1);object=nullptr;content=nullptr;parent=nullptr;base=nullptr;thread=0;return true;}
};
}
