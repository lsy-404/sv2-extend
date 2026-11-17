#include "../loader/windows/native_string.h"
#include <cassert>
#include "native_string_host_probe.h"
namespace editor_gate {bool version_matches(){return false;}}
int main(){assert(!native_string_host_probe::check());std::wstring value;assert(native_string::utf16("ASCII \xe4\xb8\xad \xf0\x9f\x8e\xb5",value));assert(value==L"ASCII \u4e2d \U0001f3b5");assert(!native_string::utf16("\xc3\x28",value));assert(!native_string::utf16(nullptr,value));void *out=reinterpret_cast<void*>(1);assert(!native_string::init(reinterpret_cast<unsigned char*>(GetModuleHandleW(nullptr)),&out,"text")&&!out);}
