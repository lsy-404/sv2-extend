#include <cassert>
#include <string>
#include "../loader/windows/script_runtime.h"
using namespace script_runtime;
static loader_json_completion saved;static void *saved_context;static std::string name,payload;
static void request(const char *method,const char *arguments,loader_json_completion done,void *context){name=method;payload=arguments;saved=done;saved_context=context;}
int main(){
 loader_api_v1 api{};api.request_capability_json=request;Service service(&api);
 auto invalid=service.call("call.submit",{{"method","selection.read"},{"arguments",json::array()}});assert(invalid["error"]["code"]=="INVALID_ARGUMENT");
 auto sent=service.call("call.submit",{{"method","selection.read"},{"arguments",{{"value",7}}}});assert(sent["ok"]&&name=="selection.read"&&json::parse(payload)["value"]==7);
 auto id=sent["result"]["ticket"];auto args=json{{"ticket",id}};assert(service.call("call.result",args)["result"]["pending"]);
 std::string result=R"({"ok":true,"result":{"value":7}})";saved(result.data(),result.size(),saved_context);
 auto received=service.call("call.result",args);assert(!received["result"]["pending"]&&received["result"]["response"]["result"]["value"]==7);
 assert(service.call("call.result",args)["error"]["code"]=="CALL_TICKET_EXPIRED");saved(result.data(),result.size(),saved_context);
 sent=service.call("call.submit",{{"method","selection.read"}});id=sent["result"]["ticket"];args={{"ticket",id}};result="not json";saved(result.data(),result.size(),saved_context);assert(service.call("call.result",args)["result"]["response"]["error"]["code"]=="INVALID_NATIVE_RESPONSE");
 for(int i=0;i<64;i++){assert(service.call("call.submit",{{"method","selection.read"}})["ok"]);}
 assert(service.call("call.submit",{{"method","selection.read"}})["error"]["code"]=="CAPABILITY_BUSY");
}
