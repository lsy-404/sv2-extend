#include "../loader/windows/editor_lua_service.h"
#include <cassert>
using editor_lua::json;
static loader_editor_dispatch callback;static void *owner;static json capabilities,result;static bool rejectRegistration;
static int reg(const char*,loader_editor_dispatch fn,void *ctx,const char *raw){if(rejectRegistration)return -1;callback=fn;owner=ctx;capabilities=json::parse(raw);return 0;}
static int unreg(const char*,void*){callback=nullptr;return 0;}
static void done(const char *raw,size_t length,void*){result=json::parse(raw,raw+length);}
int main(){
 loader_api_v1 api{};api.register_capability_provider=reg;api.unregister_capability_provider=unreg;editor_lua::Service service(&api);
 auto attached=service.call("editor.attach",{{"methods",json::array({"selection.read","notes.set_pitch"})}});assert(attached["ok"]);auto epoch=attached["result"]["epoch"];assert(capabilities["tools"].size()==2);
 callback("selection.read","{}",done,nullptr,owner);auto next=service.call("editor.next",{{"epoch",epoch}});auto request=next["result"]["request"];assert(request["method"]=="selection.read");
 auto response=service.call("editor.reply",{{"epoch",epoch},{"id",request["id"]},{"response",{{"ok",true},{"result",{{"noteCount",1}}}}}});assert(response["ok"]);assert(result["result"]["noteCount"]==1);
 assert(service.call("editor.reply",{{"epoch",epoch},{"id",request["id"]},{"response",{{"ok",true}}}})["error"]["code"]=="EDITOR_REQUEST_EXPIRED");
 auto original=capabilities;auto methods=json::array({"selection.read","notes.set_pitch"});
 assert(service.call("editor.attach",{{"methods",methods}})["error"]["code"]=="EDITOR_PROVIDER_ALREADY_ATTACHED");
 callback("selection.read","{}",done,nullptr,owner);Sleep(2100);service.tick();
 assert(callback==nullptr&&!service.registered&&service.epoch==epoch);
 assert(result["error"]["code"]=="EDITOR_PROVIDER_OFFLINE"&&service.jobs.empty());
 rejectRegistration=true;assert(service.call("editor.next",{{"epoch",epoch}})["error"]["code"]=="EDITOR_REGISTRATION_REJECTED");assert(callback==nullptr);
 rejectRegistration=false;assert(service.call("editor.next",{{"epoch",epoch}})["ok"]);assert(callback&&service.registered&&capabilities==original);
 callback("selection.read","{}",done,nullptr,owner);service.jobs.begin()->second.deadline=GetTickCount64()-1;
 assert(service.call("editor.next",{{"epoch",epoch}})["ok"]);assert(result["error"]["code"]=="EDITOR_TIMEOUT"&&service.jobs.empty());
 service.heartbeat=GetTickCount64()-3000;service.tick();assert(callback==nullptr);
 auto replacement=service.call("editor.attach",{{"methods",methods}});assert(replacement["ok"]);auto newEpoch=replacement["result"]["epoch"];assert(newEpoch!=epoch);
 assert(service.call("editor.next",{{"epoch",epoch}})["error"]["code"]=="EDITOR_EPOCH_MISMATCH");assert(service.call("editor.next",{{"epoch",newEpoch}})["ok"]);
 callback("selection.read","{}",done,nullptr,owner);service.reset();
 assert(callback==nullptr&&!service.registered&&service.epoch.empty()&&service.descriptor.empty()&&service.jobs.empty());
 assert(result["error"]["code"]=="EDITOR_PROVIDER_OFFLINE");assert(service.call("editor.next",{{"epoch",newEpoch}})["error"]["code"]=="EDITOR_EPOCH_MISMATCH");assert(callback==nullptr);
}
