#include "../loader/windows/native_label.h"
namespace editor_gate { bool version_matches() { return false; } }
static bool owned(void *) { return true; }
int main(){native_widgets_label::Label label;float w=2,h=3;int x=4,y=5;return label.create(reinterpret_cast<void*>(1),owned,GetCurrentThreadId(),"name","text") || label.update("next") || label.measure("text",w,h,x,y) || w!=2 || h!=3 || x!=4 || y!=5 || label.component()!=nullptr || !label.destroy();}
