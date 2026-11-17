#include "native_widget_types.h"
#include <cassert>
int main(){
 auto base=reinterpret_cast<unsigned char*>(GetModuleHandleW(nullptr));
 assert(!native_widget_types::scan(nullptr,base,GetCurrentThreadId()).ok);
 int object=0;assert(!native_widget_types::scan(&object,base,0).ok);
 assert(!native_widget_types::scan(&object,reinterpret_cast<unsigned char*>(1),GetCurrentThreadId()).ok);
}
