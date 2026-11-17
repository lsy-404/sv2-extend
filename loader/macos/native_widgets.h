#pragma once
#include <cstdint>
#include <cstring>
#include <functional>
#include <string>
#include <unordered_map>

namespace native_widgets { namespace {
static intptr_t slide;
inline void initialize(intptr_t value){slide=value;}
struct String {
 const char *value;
 explicit String(const char *text):value(((const char*(*)(const char*))(slide+0x100662a08ULL))(text)){}
 ~String(){((void(*)(void*))(slide+0x100699298ULL))(this);}
 String(const String&)=delete; String& operator=(const String&)=delete;
};
struct ReturnedString {
 const char *value;
 ~ReturnedString(){((void(*)(void*))(slide+0x100699298ULL))(this);}
};
inline void *allocate(size_t size){return ((void*(*)(size_t))(slide+0x101197690ULL))(size);}
inline void release(void *p){((void(*)(void*))(slide+0x101197660ULL))(p);}
inline void add(void *parent,void *child){((void(*)(void*,void*,int))(slide+0x100800dc4ULL))(parent,child,-1);}
inline void bounds(void *object,int x,int y,int w,int h){((void(*)(void*,int,int,int,int))(slide+0x1007ff7dcULL))(object,x,y,w,h);}
inline void visible(void *object,bool show){((void(*)(void*,bool))(slide+0x1007fc488ULL))(object,show);}
struct Component {
 void *object=nullptr;
 bool create(){if(object)return false;object=allocate(0xf0);if(!object)return false;std::memset(object,0,0xf0);*(void**)object=(void*)(slide+0x101570a40ULL);for(size_t offset=8;offset<0x30;offset+=8)*(const char**)((char*)object+offset)=((const char*(*)(const char*))(slide+0x100662a08ULL))("");return true;}
 void destroy(){if(object){((void(*)(void*))(slide+0x1007f0860ULL))(object);release(object);object=nullptr;}}
};
struct Viewport {
 void *object=nullptr;
 bool create(void *parent){if(object||!parent)return false;String name("");object=allocate(0x238);if(!object)return false;((void(*)(void*,void*))(slide+0x10082216cULL))(object,&name);if(*(uintptr_t*)object!=(uintptr_t)(slide+0x101572d90ULL))return false;add(parent,object);return true;}
 void content(void *child){((void(*)(void*,void*,bool))(slide+0x10082309cULL))(object,child,false);}
 void destroy(){if(object){((void(*)(void*))(slide+0x100822df4ULL))(object);release(object);object=nullptr;}}
};
struct Button {
 void *object=nullptr;void *table[60]{};bool toggle=false;std::function<void()> onClick;
 static std::unordered_map<void*,Button*>& bindings(){static std::unordered_map<void*,Button*> map;return map;}
 static void clicked(void *object){auto it=bindings().find(object);if(it!=bindings().end()&&it->second->onClick)it->second->onClick();}
 bool create(void *parent,const char *caption,bool isToggle,std::function<void()> action){if(object||!parent||!caption)return false;String name(caption);object=allocate(0x1f8);if(!object)return false;toggle=isToggle;
 if(toggle)((void(*)(void*,void*))(slide+0x1007f72a8ULL))(object,&name);else {((void(*)(void*,void*))(slide+0x1007f0448ULL))(object,&name);*(void**)object=(void*)(slide+0x1015704f0ULL);*(void**)((char*)object+0xf0)=(void*)(slide+0x1015706d0ULL);}
 uintptr_t vtable=slide+(toggle?0x101570700ULL:0x1015704f0ULL);if(*(uintptr_t*)object!=vtable)return false;std::memcpy(table,(void*)(vtable-16),sizeof table);table[2+0x1a8/8]=(void*)clicked;*(void***)object=table+2;onClick=std::move(action);bindings()[object]=this;add(parent,object);return true;}
 void set(bool state){((void(*)(void*,bool,int,int))(slide+0x1007f1784ULL))(object,state,0,0);}
 bool get()const{return *((const unsigned char*)object+0x1f1)!=0;}
 void destroy(){if(object){bindings().erase(object);onClick=nullptr;*(void**)object=(void*)(slide+(toggle?0x101570700ULL:0x1015704f0ULL));((void(*)(void*))(slide+0x1007f06a8ULL))(object);release(object);object=nullptr;}}
};
struct Label {
 void *object=nullptr;
 bool create(void *parent,const char *text){String name(""),value(text);object=allocate(0x218);if(!object)return false;((void(*)(void*,void*,void*))(slide+0x10085721cULL))(object,&name,&value);add(parent,object);return true;}
 void set(const char *text){String value(text);((void(*)(void*,void*,int))(slide+0x1008109ccULL))(object,&value,0);}
 void destroy(){if(object){((void(*)(void*))(slide+0x100857624ULL))(object);release(object);object=nullptr;}}
};
struct Choice {
 void *object=nullptr;
 bool create(void *parent){String name("");object=allocate(0x1d8);if(!object)return false;((void(*)(void*,void*))(slide+0x1008541e0ULL))(object,&name);add(parent,object);return true;}
 void item(const char *text,int id){String value(text);((void(*)(void*,void*,int))(slide+0x10081058cULL))(object,&value,id);}
 void set(int id){((void(*)(void*,int,int))(slide+0x100853fbcULL))(object,id,0);}
 int get()const{return *(const int*)((const char*)object+0x180);}
 void destroy(){if(object){((void(*)(void*))(slide+0x1008543e4ULL))(object);release(object);object=nullptr;}}
};
struct TextEditor {
 void *object=nullptr;
 bool create(void *parent,const char *text){String name("");object=allocate(0x350);if(!object)return false;((void(*)(void*,void*,bool))(slide+0x100867418ULL))(object,&name,false);add(parent,(char*)object+8);set(text);return true;}
 void set(const char *text){String value(text);((void(*)(void*,void*,bool))(slide+0x10080f0ccULL))(object,&value,false);}
 std::string get()const{auto storage=*(char**)((char*)object+0x330);ReturnedString value=((ReturnedString(*)(void*))(slide+0x1008c74d0ULL))(storage+0x60);return value.value?value.value:"";}
 void *component()const{return (char*)object+8;}
 void destroy(){if(object){((void(*)(void*))(slide+0x100868248ULL))(object);release(object);object=nullptr;}}
};
} }
