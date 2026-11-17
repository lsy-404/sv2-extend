#include <cassert>
#include <cstring>
#include <string>
#include "../loader/windows/javascript_gate.cpp"
namespace editor_gate {bool version_matches(){return false;}}
extern "C" MH_STATUS WINAPI MH_CreateHook(LPVOID,LPVOID,LPVOID*){return MH_ERROR_UNSUPPORTED_FUNCTION;}
extern "C" MH_STATUS WINAPI MH_RemoveHook(LPVOID){return MH_OK;}
extern "C" MH_STATUS WINAPI MH_QueueEnableHook(LPVOID){return MH_OK;}
extern "C" MH_STATUS WINAPI MH_ApplyQueued(){return MH_OK;}
static std::string pushed,request;static bool js=false;
static int fallback(void*){return 9;}
static const char *push_result(void*,const char *value,size_t length){pushed.assign(value,length);return pushed.c_str();}
static const char *router(const char *value,size_t length,bool javascript){request.assign(value,length);js=javascript;return "{\"ok\":true,\"result\":\"\xe5\xa3\xb0\"}";}
static void string_value(char *slot,char *object,const char *value){uint32_t tag=8,length=(uint32_t)strlen(value);memcpy(slot,&tag,4);memcpy(object+0x18,&length,4);memcpy(object+0x20,value,length);uintptr_t ptr=(uintptr_t)object;memcpy(slot+8,&ptr,8);}
int main(){using namespace javascript_gate;original=fallback;push=push_result;route=router;thread=GetCurrentThreadId();module_source="(function(){return 1;}())";
 char context[0x80]{},stack[32]{},first[128]{},second[128]{};uintptr_t bottom=(uintptr_t)stack,top=bottom+sizeof stack;memcpy(context+0x68,&bottom,8);memcpy(context+0x70,&top,8);
 string_value(stack,first,"unknown");string_value(stack+16,second,"{}");assert(intercept(context)==9);
 string_value(stack,first,"opensv2.module.v1");assert(intercept(context)==1&&pushed==module_source);
 string_value(stack,first,"opensv2.v1");assert(intercept(context)==1&&request=="{}"&&js&&pushed=="{\"ok\":true,\"result\":\"\\u58f0\"}");
 thread=0;assert(intercept(context)==1&&pushed.find("MAIN_THREAD_REQUIRED")!=std::string::npos);
}
