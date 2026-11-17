#include <cassert>
#include "../loader/windows/script_settings.h"
using namespace script_settings;
static json descriptor,state;static loader_plugin_ui_event handler;static void *context;
static int registration(const char *body,loader_plugin_ui_event event,void *owner){descriptor=json::parse(body);handler=event;context=owner;return 0;}
static int update(const char *,const char *body,void *){state=json::parse(body);return 0;}
int main(){loader_api_v1 api{};api.register_plugin_ui=registration;api.update_plugin_ui=update;Runtime lua{&api,"opensv2.lua-extend","Lua Extend",true};assert(register_runtime(lua,true)==0);assert(descriptor["settings"].size()==19);
 json values={{"enabled",false},{"showDetails",true},{"selection.read",false}};auto event=json{{"kind","settings"},{"settings",values}}.dump();handler(event.c_str(),context);assert(state["status"]=="已停用");assert(denial(lua,"call.submit",{{"method","selection.read"}})==std::string("EXTENSION_DISABLED"));assert(!denial(lua,"editor.next",json::object()));assert(!denial(lua,"call.submit",{{"method","http.cancel"}}));
 values["enabled"]=true;event=json{{"kind","settings"},{"settings",values}}.dump();handler(event.c_str(),context);assert(denial(lua,"call.submit",{{"method","selection.read"}})==std::string("FEATURE_DISABLED"));assert(!denial(lua,"call.submit",{{"method","transport.play"}}));
 Runtime js{&api,"opensv2.javascript-extend","JS Extend",false};assert(register_runtime(js,false)==0);assert(descriptor["status"]=="当前宿主不可用");assert(!denial(js,"call.submit",{{"method","selection.read"}}));}
