#pragma once
#include "native_string.h"
namespace native_string_host_probe {
inline bool check(){auto base=reinterpret_cast<unsigned char*>(GetModuleHandleW(nullptr));const char sample[]="ASCII \xe4\xb8\xad \xf0\x9f\x8e\xb5";void *value=nullptr;if(!native_string::init(base,&value,sample)||!value)return false;auto text=static_cast<const char*>(value);bool match=true;for(size_t i=0;i<sizeof sample;++i){if(!native_widget_listener::readable(text+i,1)||text[i]!=sample[i]){match=false;break;}}auto p=static_cast<unsigned char*>(value);if(p!=base+0x17903a0){if(!native_widget_listener::readable(p-16,sizeof(LONG)))return false;if(InterlockedExchangeAdd(reinterpret_cast<LONG*>(p-16),-1)==0)reinterpret_cast<void(*)(void*)>(base+0x10baa40)(p-16);}void *bad=nullptr;return match&&!native_string::init(base,&bad,"\xc3\x28")&&!bad;}
}
