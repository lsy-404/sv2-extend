#pragma once
#include <windows.h>
#include <cstdint>
#include <cstring>

namespace editor_gate { bool version_matches(); }
namespace native_widget_listener {
using Enqueue = void (*)(void *);
struct Binding;
struct ListenerTable {
 void (*destroy)(Binding *, unsigned char);
 void (*clicked)(Binding *, void *);
 void (*state_changed)(Binding *, void *);
};
struct Binding {
 const ListenerTable *table;
 unsigned char *image=nullptr;
 void *button=nullptr;
 void *context=nullptr;
 Enqueue enqueue=nullptr;
 DWORD thread=0;
 bool enabled=false;
 Binding(const Binding &)=delete;
 Binding &operator=(const Binding &)=delete;
 Binding();
};
inline void destroy(Binding *,unsigned char){}
inline void clicked(Binding *binding,void *button){if(binding->enabled&&binding->thread==GetCurrentThreadId()&&binding->button==button&&binding->enqueue)binding->enqueue(binding->context);}
inline void state_changed(Binding *,void *){}
inline const ListenerTable table{destroy,clicked,state_changed};
inline Binding::Binding():table(&native_widget_listener::table){}
inline bool readable(const void *p,size_t length){auto at=reinterpret_cast<uintptr_t>(p);if(!at||length>UINTPTR_MAX-at)return false;auto end=at+length;while(at<end){MEMORY_BASIC_INFORMATION page{};if(!VirtualQuery(reinterpret_cast<void*>(at),&page,sizeof page)||page.State!=MEM_COMMIT||(page.Protect&PAGE_GUARD))return false;DWORD flags=page.Protect&0xff;if(flags!=PAGE_READONLY&&flags!=PAGE_READWRITE&&flags!=PAGE_WRITECOPY&&flags!=PAGE_EXECUTE_READ&&flags!=PAGE_EXECUTE_READWRITE&&flags!=PAGE_EXECUTE_WRITECOPY)return false;auto begin=reinterpret_cast<uintptr_t>(page.BaseAddress);if(page.RegionSize>UINTPTR_MAX-begin||begin+page.RegionSize<=at)return false;at=begin+page.RegionSize;}return true;}
inline bool signature(unsigned char *base,size_t rva,const unsigned char *bytes,size_t length){MEMORY_BASIC_INFORMATION page{};auto p=base+rva;return VirtualQuery(p,&page,sizeof page)&&page.AllocationBase==base&&page.Type==MEM_IMAGE&&page.State==MEM_COMMIT&&(page.Protect&0xff)==PAGE_EXECUTE_READ&&readable(p,length)&&std::memcmp(p,bytes,length)==0;}
inline bool supported(unsigned char *base){static constexpr unsigned char add[]={0x48,0x89,0x5c,0x24,0x10,0x48,0x89,0x6c,0x24,0x18};static constexpr unsigned char set[]={0x40,0x53,0x55,0x56,0x57,0x41,0x56,0x48,0x83,0xec,0x50};static constexpr unsigned char get[]={0x40,0x53,0x48,0x83,0xec,0x40,0x48,0x8b,0x05};if(!base||base!=reinterpret_cast<unsigned char*>(GetModuleHandleW(nullptr)))return false;static const bool verified=editor_gate::version_matches()&&signature(base,0xce15a0,add,sizeof add)&&signature(base,0xc480d0,set,sizeof set)&&signature(base,0x8ad8a0,get,sizeof get);return verified;}
inline bool toggle_valid(unsigned char *base,void *button){if(!base||!readable(button,0x238))return false;void *vtable=nullptr;std::memcpy(&vtable,button,sizeof vtable);return vtable==base+0x14fc7b8;}
inline bool bind(Binding &binding,void *button,Enqueue enqueue,void *context,size_t size,size_t vtable_rva){auto base=reinterpret_cast<unsigned char*>(GetModuleHandleW(nullptr));if(binding.button||!enqueue||!supported(base)||!readable(button,size))return false;void *vtable=nullptr;std::memcpy(&vtable,button,sizeof vtable);if(vtable!=base+vtable_rva)return false;binding.image=base;binding.button=button;binding.context=context;binding.enqueue=enqueue;binding.thread=GetCurrentThreadId();binding.enabled=true;reinterpret_cast<void(*)(void*,void*)>(base+0xce15a0)(static_cast<unsigned char*>(button)+0x1a0,&binding);return true;}
inline bool bind_toggle(Binding &binding,void *button,Enqueue enqueue,void *context){return bind(binding,button,enqueue,context,0x238,0x14fc7b8);}
inline bool bind_text_button(Binding &binding,void *button,Enqueue enqueue,void *context){return bind(binding,button,enqueue,context,0x238,0x14fd090);}
inline void deactivate(Binding &binding){binding.enabled=false;binding.enqueue=nullptr;binding.context=nullptr;}
inline bool get_toggle(Binding &binding,bool &value){auto base=binding.image;if(!binding.enabled||binding.thread!=GetCurrentThreadId()||!toggle_valid(base,binding.button))return false;value=reinterpret_cast<bool(*)(void*)>(base+0x8ad8a0)(binding.button);return true;}
inline bool set_toggle(Binding &binding,bool value){auto base=binding.image;if(!binding.enabled||binding.thread!=GetCurrentThreadId()||!toggle_valid(base,binding.button))return false;reinterpret_cast<void(*)(void*,bool,int,int)>(base+0xc480d0)(binding.button,value,0,0);return true;}
}
