#include "../loader/windows/export_native_service.h"
#include <cassert>
#include <vector>
static int finish(void *renderer){auto p=static_cast<unsigned char*>(renderer);p[0x2e4d0]=0;p[0x2e4d1]=0;p[0x2e4d2]=0;return 10;}
static int incomplete(void*){return 10;}
int main(){
 using namespace export_native;
 std::vector<unsigned char> renderer(0x2e4d3);auto p=renderer.data();original_step=finish;
 watch(p);watched_prepared=true;p[0x2e4d0]=1;p[0x2e4d1]=1;p[0x2e4d2]=1;
 on_step(p);assert(watched_finished&&!watched_cancelled);
 watch(p);watched_prepared=true;cancel_signal_seen=true;p[0x2e4d0]=1;p[0x2e4d1]=1;p[0x2e4d2]=1;
 on_step(p);assert(watched_finished&&watched_cancelled);
 watch(p);watched_prepared=true;cancel_signal_seen=true;original_step=incomplete;p[0x2e4d0]=1;p[0x2e4d1]=1;p[0x2e4d2]=1;
 on_step(p);assert(!watched_finished&&!watched_cancelled);
 watch(p);watched_prepared=true;cancel_signal_seen=true;original_step=finish;p[0x2e4d0]=1;p[0x2e4d1]=1;p[0x2e4d2]=0;
 on_step(p);assert(!watched_finished&&!watched_cancelled);
 unwatch();
}
