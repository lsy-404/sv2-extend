#include "../loader/windows/native_viewport.h"
#include <cassert>
namespace editor_gate {bool version_matches(){return false;}}
static bool owned(void*){return true;}
int main(){
 native_viewport::Viewport viewport;native_viewport::Component content;alignas(void*) unsigned char pane[0xf0]{};
 assert(!viewport.create(pane,owned,GetCurrentThreadId()));assert(!content.create(GetCurrentThreadId()));
 assert(!viewport.set_content(pane)&&!viewport.layout(0,0,100,100));assert(viewport.destroy()&&content.destroy());
}
