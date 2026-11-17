#pragma once
#include "native_widget_listener.h"
#include <string>
namespace native_string {
inline bool utf16(const char *input,std::wstring &out){if(!input)return false;int count=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,input,-1,nullptr,0);if(count<=0)return false;std::wstring converted(static_cast<size_t>(count),L'\0');if(MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,input,-1,converted.data(),count)!=count)return false;converted.pop_back();out=std::move(converted);return true;}
inline void *init(unsigned char *base,void *output,const char *input){if(!output)return nullptr;*static_cast<void**>(output)=nullptr;static constexpr unsigned char ctor[]={0x40,0x53,0x56,0x48,0x83,0xec,0x28,0x48,0x8b,0xda};if(!native_widget_listener::supported(base)||!native_widget_listener::signature(base,0xe6fbd0,ctor,sizeof ctor))return nullptr;std::wstring wide;if(!utf16(input,wide))return nullptr;return reinterpret_cast<void*(*)(void*,const wchar_t*)>(base+0xe6fbd0)(output,wide.c_str());}
}
