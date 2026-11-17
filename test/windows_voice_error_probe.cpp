#include <cassert>
#include "windows_voice_error_probe.h"
namespace editor_gate {bool version_matches(){return false;}}
namespace voice_adapter {bool is_observing(void*){return false;}}
extern "C" MH_STATUS WINAPI MH_CreateHook(LPVOID,LPVOID,LPVOID*){return MH_ERROR_UNSUPPORTED_FUNCTION;}
extern "C" MH_STATUS WINAPI MH_RemoveHook(LPVOID){return MH_OK;}
extern "C" MH_STATUS WINAPI MH_QueueEnableHook(LPVOID){return MH_OK;}
extern "C" MH_STATUS WINAPI MH_ApplyQueued(){return MH_OK;}
static int called=0;static void original_handler(void*){called++;}
int main(){
 using namespace voice_error_probe;Entry entries[]={{7,0,"Liam","Unable to load","C:\\private\\folder"}};
 alignas(8) unsigned char event[32]{};Entry *array=entries;int count=1;memcpy(event,&array,sizeof array);memcpy(event+0xc,&count,sizeof count);
 thread=GetCurrentThreadId();original=original_handler;intercept(event);assert(called==1&&latest["entries"][0]["code"]==7&&latest["entries"][0]["detail"]=="Unable to load"&&latest["entries"][0]["folderPresent"]==true);
 entries[0].detail="C:\\private\\file";intercept(event);assert(called==2&&latest["entries"][0]["detail"]=="[path redacted]");event[0x11]=1;intercept(event);assert(called==3&&latest["entries"][0]["detail"]=="[path redacted]");
}
