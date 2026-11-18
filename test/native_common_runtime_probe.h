#pragma once
#include "native_widget_types.h"
#include "native_string_host_probe.h"
namespace native_ui_probe {
using json=nlohmann::json;
static const char *id="test.nativeui";
static unsigned settings_events=0,action_events=0;
static int owner;
static json values;
static bool strings_ok=false;
static native_panel::Pane *pane(){for(auto &entry:native_panel::panes)if(entry.second.id==id)return &entry.second;return nullptr;}
static native_panel::Control *control(const char *key){auto p=pane();if(!p)return nullptr;auto it=p->controls.find(key);return it==p->controls.end()?nullptr:it->second.get();}
static bool mounted(void *component,void *parent){if(!native_widget_listener::readable(component,0x38))return false;void *actual=nullptr;std::memcpy(&actual,static_cast<unsigned char*>(component)+0x30,sizeof actual);return actual==parent;}
static json snapshot(){
 json views=json::array();auto p=pane();if(p){auto toggle=control("setting/enabled"),choice=control("setting/selection"),textbox=control("setting/notes");bool checked=false;int selected=-1;std::string text;bool valid=p->ready&&toggle&&choice&&textbox;
 bool native=valid&&toggle->toggle.component()&&choice->choice.component()&&textbox->textbox.component();
 bool visible=native&&(static_cast<unsigned char*>(toggle->component())[0xe8]&2)!=0;
 bool mounted_all=native&&mounted(p->viewport.object,p->identity)&&mounted(p->content.object,static_cast<unsigned char*>(p->viewport.object)+0x110);
 if(mounted_all)for(auto &entry:p->controls)mounted_all=mounted_all&&mounted(entry.second->component(),p->content.object);
 if(valid){native_widget_listener::get_toggle(toggle->toggle.listener,checked);choice->choice.get(selected);textbox->textbox.get_text(text);}
 int scroll_top=0;if(p->content.object)std::memcpy(&scroll_top,static_cast<unsigned char*>(p->content.object)+0x3c,sizeof scroll_top);
 views.push_back({{"order",p->order},{"ready",valid},{"mountedToPane",mounted_all},{"toggleNative",native},{"buttonNative",control("row/button/probe")&&control("row/button/probe")->button.component()!=nullptr},{"toggleVisible",visible},{"checked",checked},{"choiceReadable",valid},{"selected",selected},{"textReadable",valid},{"text",text},{"nativeControlCount",p->controls.size()},{"nativeLabelMeasured",control("status")&&control("status")->measured_width>0},{"scrollTop",scroll_top}});
 }
 return {{"hostPid",GetCurrentProcessId()},{"rootReady",editor_autostart::root_ready()},{"categoryReady",native_category_registry::prepared},{"views",views},{"settingsEvents",settings_events},{"actionEvents",action_events},{"settings",values},{"nativeStringRoundTrip",strings_ok},{"foreignContentWindows",0}};
}
static void event(const char *raw,void*){auto value=json::parse(raw,nullptr,false);if(!value.is_object())return;if(value.value("kind","")=="settings"){++settings_events;values=value["settings"];}if(value.value("kind","")=="action")++action_events;}
static void reply(loader_json_completion done,void *context,const json &value){auto body=value.dump();done(body.data(),body.size(),context);}
static void dispatch(const char *method,const char *raw,loader_json_completion done,void *context,void*){
 auto args=json::parse(raw,nullptr,false);std::string name=method?method:"";bool ok=args.is_object();json result;
 if(!ok){reply(done,context,{{"ok",false}});return;}
 if(name=="test.nativeui.state"){if(editor_autostart::root_ready()&&!strings_ok)strings_ok=native_string_host_probe::check();result=snapshot();}
 else if(name=="test.nativeui.open"){ok=native_category_registry::open_category(id)==0;result=snapshot();}
 else if(name=="test.nativeui.set"){
  auto c=control("setting/enabled");ok=c&&args.contains("checked")&&args["checked"].is_boolean();if(ok)reinterpret_cast<void(*)(void*,bool,int,int)>(c->toggle.base+0xc480d0)(c->toggle.toggle,args["checked"].get<bool>(),1,1);result=snapshot();
 }else if(name=="test.nativeui.inputs"){
  auto c=control("setting/selection"),t=control("setting/notes");ok=c&&t&&args.contains("selected")&&args["selected"].is_number_integer()&&args.contains("text")&&args["text"].is_string();if(ok)ok=c->choice.set(args["selected"].get<int>())&&t->textbox.set_text(args["text"].get_ref<const std::string&>().c_str());result=snapshot();
 }else if(name=="test.nativeui.refresh"){
  auto p=pane();ok=p!=nullptr;if(ok){native_category_registry::pane_resized(p->identity);if(native_panel::host){InvalidateRect(native_panel::host,nullptr,TRUE);UpdateWindow(native_panel::host);}}result=snapshot();
 }else if(name=="test.nativeui.update"){
  auto p=pane();ok=p!=nullptr;if(ok){std::map<std::string,void*> before;for(auto &item:p->controls)before[item.first]=item.second->component();std::string state=json{{"status",u8"Updated \u4e2d\u6587"}}.dump();ok=native_panel::update_ui(id,state.c_str(),&owner)==0;for(auto &item:p->controls)ok=ok&&before[item.first]==item.second->component();}result=snapshot();
 }else if(name=="test.nativeui.clear"){
  auto p=pane();ok=native_category_registry::close_category(id)==0;if(p)native_panel::dock_remove(p->identity);ok=ok&&!pane();result=snapshot();
 }else if(name=="test.nativeui.types"){
  auto p=pane();ok=p!=nullptr;json types=json::array();if(ok){auto found=native_widget_types::scan(p->identity,reinterpret_cast<unsigned char*>(GetModuleHandleW(nullptr)),native_category_registry::thread_id);ok=found.ok;for(auto &type:found.types)types.push_back({{"name",type.name},{"vtableRva",type.vtable_rva},{"count",type.count}});}result={{"types",types}};
 }else ok=false;
 reply(done,context,{{"ok",ok},{"result",result}});
}
static bool install(const loader_api_v1 &api,HWND){
 json methods=json::array(),tools=json::array();for(const char *operation:{"state","open","set","refresh","clear","types","inputs","update"}){std::string method=std::string("test.nativeui.")+operation;methods.push_back(method);tools.push_back({{"name",std::string("svext_nativeui_")+operation},{"method",method},{"description","Verify native host components in an isolated profile."},{"inputSchema",{{"type","object"},{"properties",json::object()}}}});}auto descriptor=json{{"methods",methods},{"tools",tools}}.dump();if(api.register_capability_provider(id,dispatch,&owner,descriptor.c_str())!=LOADER_OK)return false;
 auto settings=json::array({{{"key","enabled"},{"title",u8"Enable native setting \u539f\u751f\u9009\u9879"},{"type","boolean"},{"default",false}},{{"key","selection"},{"title",u8"\u9009\u62e9 Choice"},{"type","choice"},{"default","Second choice"},{"choices",json::array({"First choice","Second choice","Third choice"})}},{{"key","notes"},{"title",u8"\u6587\u672c Text"},{"type","text"},{"default","Native text"}}});
 auto rows=json::array({{{"type","button"},{"id","probe"},{"text","Native action"}}});
 auto ui=json{{"id",id},{"name","Native UI test"},{"version","1"},{"status",u8"Native components \u539f\u751f\u7ec4\u4ef6"},{"sidebar",{{"title","Native UI test"},{"rows",rows}}},{"settings",settings}}.dump();return api.register_plugin_ui(ui.c_str(),event,&owner)==LOADER_OK;
}
}
