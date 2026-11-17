#include "../loader/windows/native_widget_listener.h"
#include <cassert>
namespace editor_gate { bool version_matches(){return false;} }
static int count=0;static void enqueue(void *){++count;}
int main(){using namespace native_widget_listener;Binding binding;int button=0;binding.button=&button;binding.thread=GetCurrentThreadId();binding.enqueue=enqueue;binding.enabled=true;binding.table->clicked(&binding,&button);assert(count==1);int other=0;binding.table->clicked(&binding,&other);binding.table->state_changed(&binding,&button);assert(count==1);deactivate(binding);binding.table->clicked(&binding,&button);assert(count==1);bool value=false;assert(!get_toggle(binding,value)&&!set_toggle(binding,true));}
