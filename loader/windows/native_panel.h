#ifndef EXTEND_NATIVE_PANEL_H
#define EXTEND_NATIVE_PANEL_H
#include <windows.h>
#include <map>
#include <memory>
#include <string>
#include <vector>
#include <set>
#include <filesystem>
#include <fstream>
#include <cmath>
#include "vendor/json.hpp"
#include "native_widgets.h"
#include "native_label.h"
#include "native_viewport.h"
#include "../../sdk/include/loader_api.h"

namespace native_panel {
using json=nlohmann::json;
struct Plugin { json descriptor, values; loader_plugin_ui_event event; void *owner; };
struct Category { std::string id, title; bool visible; };
struct Control {
 std::string plugin,key,type,caption,observed;
 json value,definition;
 native_widgets::Toggle toggle;
 native_widgets::Button button;
 native_widgets::Choice choice;
 native_widgets::TextBox textbox;
 native_widgets_label::Label label;
 unsigned pending=0;
 ULONGLONG edited_at=0;
 int measured_width=-1,label_height=26;
 void *component() const {
  if(type=="label")return label.component();
  if(type=="boolean"||type=="visible")return toggle.component();
  if(type=="choice")return choice.component();
  if(type=="text"||type=="integer")return textbox.component();
  return button.component();
 }
 bool destroy(){return toggle.destroy()&&button.destroy()&&choice.destroy()&&textbox.destroy()&&label.destroy();}
 bool layout(int x,int y,int w,int h){
  if(type=="label")return label.layout(x,y,w,h);
  if(type=="boolean"||type=="visible")return toggle.layout(x,y,w,h);
  if(type=="choice")return choice.layout(x,y,w,h);
  if(type=="text"||type=="integer")return textbox.layout(x,y,w,h);
  return button.layout(x,y,w,h);
 }
};
struct Pane {
 void *identity;std::string id;HWND window;
 RECT bounds{};bool visible=false,ready=false;
 native_viewport::Viewport viewport;
 native_viewport::Component content;
 std::map<std::string,std::unique_ptr<Control>> controls;
 std::vector<std::string> order;
 Pane(void *key,const char *value):identity(key),id(value),window(nullptr){}
};
static std::map<std::string,Plugin> plugins;
static std::map<void*,Pane> panes;
static HWND dispatcher=nullptr,host=nullptr;
static DWORD thread_id=0;
static std::filesystem::path preference_file,diagnostic_file;
static std::string last_diagnostic;
static json preferences=json::object(),visibility=json::object(),preference_document=json::object();
static bool preferences_writable=false;
static uint64_t generation=1;
static void (*categories_changed)()=nullptr;
static int (*open_category)(const char*)=nullptr;
static std::wstring wide(const std::string &s) {
 int n=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,s.data(),(int)s.size(),nullptr,0);
 if(n<=0)return {}; std::wstring out((size_t)n,L'\0'); MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,s.data(),(int)s.size(),out.data(),n); return out;
}
static bool text(const json &v,size_t max) {return v.is_string() && v.get_ref<const std::string&>().size()<=max && (v.get_ref<const std::string&>().empty() || !wide(v.get_ref<const std::string&>()).empty());}
static bool name(const json &v) {
 if(!text(v,96))return false; auto s=v.get<std::string>(); if(s.empty() || !((s[0]>='A'&&s[0]<='Z')||(s[0]>='a'&&s[0]<='z')))return false;
 for(char c:s)if(!((c>='A'&&c<='Z')||(c>='a'&&c<='z')||(c>='0'&&c<='9')||c=='_'||c=='.'||c=='-'))return false; return true;
}
static bool field_value(const json &f,const json &v) {
 std::string t=f.value("type",std::string());
 if(t=="boolean")return v.is_boolean(); if(t=="text")return text(v,1024);
 if(t=="choice") {if(!text(v,256)||!f.contains("choices")||!f["choices"].is_array())return false; for(auto &c:f["choices"])if(c==v)return true; return false;}
 if(t=="integer")return v.is_number_integer()&&f.contains("minimum")&&f.contains("maximum")&&f["minimum"].is_number_integer()&&f["maximum"].is_number_integer()&&v>=f["minimum"]&&v<=f["maximum"];
 return false;
}
static bool valid(const json &d) {
 if(!d.is_object()||!d.contains("id")||!name(d["id"])||!d.contains("name")||!text(d["name"],128)||d["name"].get<std::string>().empty()||!d.contains("version")||!text(d["version"],64)||!d.contains("status")||!text(d["status"],128)||!d.contains("sidebar")||!d["sidebar"].is_object()||!d["sidebar"].contains("title")||!text(d["sidebar"]["title"],128)||!d["sidebar"].contains("rows")||!d["sidebar"]["rows"].is_array()||d["sidebar"]["rows"].size()>32||!d.contains("settings")||!d["settings"].is_array()||d["settings"].size()>64)return false;
 std::set<std::string> ids;
 for(auto &r:d["sidebar"]["rows"]) {if(!r.is_object()||!r.contains("type")||!r["type"].is_string()||!r.contains("text")||!text(r["text"],512))return false; auto t=r["type"].get<std::string>(); if(t=="label")continue; if(t!="button"||!r.contains("id")||!name(r["id"])||!ids.insert(r["id"].get<std::string>()).second)return false;}
 ids.clear(); for(auto &f:d["settings"]) {if(!f.is_object()||!f.contains("key")||!name(f["key"])||!ids.insert(f["key"].get<std::string>()).second||!f.contains("title")||!text(f["title"],128)||!f.contains("type")||!f["type"].is_string()||!f.contains("default"))return false;
 if(f["type"]=="choice") {if(!f.contains("choices")||!f["choices"].is_array()||f["choices"].empty()||f["choices"].size()>32)return false;for(auto &c:f["choices"])if(!text(c,256))return false;}
 if(!field_value(f,f["default"]))return false; } return true;
}
static json parse(const char *raw) {if(!raw||strnlen(raw,65537)>65536)return json();return json::parse(raw,nullptr,false);}
static bool save_state(const json &next,const json &next_visibility) {
 if(!preferences_writable)return false;auto document=preference_document;document["schema"]=1;document["plugins"]=next;document["categoryVisibility"]=next_visibility;std::string data=document.dump();if(data.size()>262144)return false;
 auto temp=preference_file;temp+=L".tmp-"+std::to_wstring(GetCurrentProcessId())+L"-"+std::to_wstring(GetTickCount64());HANDLE file=CreateFileW(temp.c_str(),GENERIC_WRITE,0,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr);if(file==INVALID_HANDLE_VALUE)return false;
 DWORD written=0;bool ok=WriteFile(file,data.data(),(DWORD)data.size(),&written,nullptr)&&written==data.size()&&FlushFileBuffers(file);if(!CloseHandle(file))ok=false;if(ok)ok=MoveFileExW(temp.c_str(),preference_file.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)!=0;if(!ok){DeleteFileW(temp.c_str());return false;}preferences=next;visibility=next_visibility;preference_document=std::move(document);++generation;return true;
}
static bool save_values(const std::string &id,const json &values){auto next=preferences;next[id]=values;return save_state(next,visibility);}
static void notify(const std::string &id,const json &event) {auto it=plugins.find(id);if(it==plugins.end()||!it->second.event)return; auto callback=it->second.event;auto owner=it->second.owner;std::string body=event.dump();callback(body.c_str(),owner);}
[[maybe_unused]] static std::vector<Category> categories(){std::vector<Category> result;for(const auto &p:plugins)result.push_back({p.first,p.second.descriptor["name"].get<std::string>(),!visibility.contains(p.first)||!visibility[p.first].is_boolean()||visibility[p.first].get<bool>()});return result;}
[[maybe_unused]] static void set_categories_changed(void(*callback)()){categories_changed=callback;}
[[maybe_unused]] static void set_open_category(int(*callback)(const char*)){open_category=callback;}

static bool owned(void *pane){if(panes.count(pane))return true;for(auto &item:panes)if(item.second.content.object==pane)return true;return false;}
static bool owns_child(void *pane,void *child){auto found=panes.find(pane);if(found==panes.end())return false;return found->second.viewport.object==child;}
static void enqueue(void *context){auto control=static_cast<Control*>(context);if(control->pending<1024)++control->pending;if(dispatcher)PostMessageW(dispatcher,WM_APP+2,0,0);}
static bool create(Pane &pane,Control &c,const json &field){
 const auto &caption=c.caption;bool ok=false;
 if(c.type=="label")ok=c.label.create(pane.content.object,owned,thread_id,c.key.c_str(),caption.c_str());
 else if(c.type=="boolean"||c.type=="visible")ok=c.toggle.create(pane.content.object,owned,thread_id,caption.c_str(),enqueue,&c)&&native_widget_listener::set_toggle(c.toggle.listener,c.value.get<bool>());
 else if(c.type=="choice"){
  std::vector<std::string> storage;std::vector<const char*> items;for(auto &item:field["choices"])storage.push_back(item.get<std::string>());for(auto &item:storage)items.push_back(item.c_str());
  int selected=-1;for(size_t i=0;i<storage.size();++i)if(storage[i]==c.value.get<std::string>())selected=(int)i;
  ok=c.choice.create(pane.content.object,owned,thread_id,caption.c_str(),items.data(),(int)items.size())&&c.choice.set(selected);
 }else if(c.type=="text"||c.type=="integer"){
  auto value=c.type=="text"?c.value.get<std::string>():c.value.dump();c.observed=value;ok=c.textbox.create(pane.content.object,owned,thread_id,value.c_str());
 }else ok=c.button.create(pane.content.object,owned,thread_id,caption.c_str(),enqueue,&c);
 if(!ok)c.destroy();return ok;
}
static bool reconcile(Pane &pane){
 std::set<std::string> wanted;std::vector<std::string> order;bool ok=true;
 auto add=[&](const std::string &identity,const std::string &plugin,const std::string &key,const std::string &type,const std::string &caption,const json &value,const json &field=json::object()){
  wanted.insert(identity);order.push_back(identity);auto found=pane.controls.find(identity);
  if(found!=pane.controls.end()&&(found->second->type!=type||(type=="choice"&&found->second->definition.value("choices",json())!=field.value("choices",json())))){if(!found->second->destroy()){ok=false;return;}pane.controls.erase(found);found=pane.controls.end();}
  if(found==pane.controls.end()){
   auto control=std::make_unique<Control>();control->plugin=plugin;control->key=key;control->type=type;control->caption=caption;control->value=value;control->definition=field;
   auto raw=control.get();pane.controls.emplace(identity,std::move(control));if(!create(pane,*raw,field)){ok=false;if(raw->destroy())pane.controls.erase(identity);}
  }else {auto &control=*found->second;
   if(control.caption!=caption){bool changed=type=="label"?control.label.update(caption.c_str()):(type=="boolean"||type=="visible")?control.toggle.set_caption(caption.c_str()):(type=="action"||type=="open")?control.button.set_caption(caption.c_str()):true;if(changed){control.caption=caption;control.measured_width=-1;}else ok=false;}
  }
 };
 if(pane.id.empty()){
  for(const auto &item:plugins){auto &id=item.first;auto &plugin=item.second;add(id+"/title",id,"title","label",plugin.descriptor["name"],json());for(const auto &field:plugin.descriptor["settings"])if(field["key"]=="enabled"&&field["type"]=="boolean")add(id+"/enabled",id,"enabled","boolean",u8"启用插件",plugin.values["enabled"],field);bool visible=!visibility.contains(id)||visibility[id].get<bool>();add(id+"/visible",id,"visible","visible",u8"\u663e\u793a\u53f3\u4fa7\u9762\u677f\u5165\u53e3",visible);add(id+"/open",id,"open","open",u8"\u6253\u5f00\u63d2\u4ef6\u9762\u677f",json());}
 }else {
  auto found=plugins.find(pane.id);if(found!=plugins.end()){auto &plugin=found->second;
   add("title",pane.id,"title","label",plugin.descriptor["sidebar"]["title"],json());add("status",pane.id,"status","label",plugin.descriptor["status"],json());
   size_t index=0;for(const auto &row:plugin.descriptor["sidebar"]["rows"]){auto type=row["type"].get<std::string>();auto key=type=="button"?row["id"].get<std::string>():std::to_string(index);add("row/"+type+"/"+key,pane.id,key,type=="button"?"action":"label",row["text"],json());++index;}
   for(const auto &field:plugin.descriptor["settings"]){auto key=field["key"].get<std::string>(),type=field["type"].get<std::string>();if(type!="boolean")add("caption/"+key,pane.id,key,"label",field["title"],json());add("setting/"+key,pane.id,key,type,field["title"],plugin.values[key],field);}
  }
 }
 for(auto it=pane.controls.begin();it!=pane.controls.end();){if(!wanted.count(it->first)){if(it->second->destroy())it=pane.controls.erase(it);else{ok=false;++it;}}else ++it;}
 pane.order=std::move(order);pane.ready=ok;return ok;
}
static bool measure_label(Control &control,int width){
 if(control.measured_width==width)return true;
 float measured=0,font_height=0;int horizontal=0,vertical=0;
 if(!control.label.measure("M",measured,font_height,horizontal,vertical))return false;
 float available=(float)std::max(1,width-horizontal);size_t paragraph=0;int lines=0;
 while(true){
  auto end=control.caption.find('\n',paragraph);if(end==std::string::npos)end=control.caption.size();
  std::vector<size_t> boundaries{paragraph};for(size_t i=paragraph;i<end;){++i;while(i<end&&(static_cast<unsigned char>(control.caption[i])&0xc0)==0x80)++i;boundaries.push_back(i);}
  size_t begin=0;if(boundaries.size()==1)++lines;
  while(begin+1<boundaries.size()){
   size_t low=begin+1,high=boundaries.size()-1,fitting=begin+1;
   while(low<=high){size_t middle=low+(high-low)/2;auto part=control.caption.substr(boundaries[begin],boundaries[middle]-boundaries[begin]);float actual=0,unused_height=0;int hb=0,vb=0;if(!control.label.measure(part.c_str(),actual,unused_height,hb,vb))return false;if(actual<=available){fitting=middle;low=middle+1;}else{if(!middle)break;high=middle-1;}}
   size_t next=fitting;if(fitting+1<boundaries.size())for(size_t i=fitting;i>begin+1;--i)if(control.caption[boundaries[i-1]]==' '||control.caption[boundaries[i-1]]=='\t'){next=i;break;}
   begin=next;while(begin+1<boundaries.size()&&(control.caption[boundaries[begin]]==' '||control.caption[boundaries[begin]]=='\t'))++begin;++lines;
  }
  if(end==control.caption.size())break;paragraph=end+1;
 }
 control.label_height=std::max(26,(int)std::ceil(lines*font_height)+vertical+2);control.measured_width=width;return true;
}
static void layout(Pane &pane){
 int viewport_width=pane.bounds.right-pane.bounds.left,viewport_height=pane.bounds.bottom-pane.bounds.top;
 auto image=reinterpret_cast<unsigned char*>(GetModuleHandleW(nullptr));
 if(!pane.viewport.object||!pane.content.object)return;
 if(!pane.visible||viewport_width<=0||viewport_height<=0){reinterpret_cast<void(*)(void*,bool)>(image+0xc4e5b0)(pane.viewport.object,false);return;}
 pane.viewport.layout(pane.bounds.left,pane.bounds.top,viewport_width,viewport_height);
 int available=0;std::memcpy(&available,static_cast<unsigned char*>(pane.viewport.object)+0x150,sizeof available);
 int width=available-24,y=10;
 for(const auto &identity:pane.order){auto found=pane.controls.find(identity);if(found==pane.controls.end())continue;auto &control=*found->second;auto child=control.component();if(!child)continue;
  if(control.type=="label"&&!measure_label(control,width)){pane.ready=false;continue;}int height=control.type=="label"?control.label_height:28;
  if(pane.visible&&width>0){control.layout(12,y,width,height);}else reinterpret_cast<void(*)(void*,bool)>(image+0xc4e5b0)(child,false);
  y+=height+6;
 }
 int x=0,top=0;std::memcpy(&x,static_cast<unsigned char*>(pane.content.object)+0x38,sizeof x);std::memcpy(&top,static_cast<unsigned char*>(pane.content.object)+0x3c,sizeof top);pane.content.layout(x,top,std::max(1,available),std::max(viewport_height,y+8));
}
static void refresh_plugin(const std::string &id,bool center){for(auto &item:panes)if(item.second.id==id||(center&&item.second.id.empty())){reconcile(item.second);layout(item.second);}}
static void commit_value(const std::string &plugin,const std::string &key,const std::string &type,const json &value,bool restore){
 for(auto &pane:panes)for(auto &item:pane.second.controls){auto &c=*item.second;if(c.plugin!=plugin||c.key!=key||c.type!=type)continue;
  if(restore){if(type=="boolean"||type=="visible")native_widget_listener::set_toggle(c.toggle.listener,c.value.get<bool>());else if(type=="choice"){auto found=plugins.find(plugin);if(found!=plugins.end())for(auto &field:found->second.descriptor["settings"])if(field["key"]==key)for(size_t i=0;i<field["choices"].size();++i)if(field["choices"][i]==c.value)c.choice.set((int)i);}else if(type=="text"||type=="integer"){auto text=type=="text"?c.value.get<std::string>():c.value.dump();c.textbox.set_text(text.c_str());c.observed=text;c.edited_at=0;}}
  else {if(c.value!=value){if(type=="boolean"||type=="visible")native_widget_listener::set_toggle(c.toggle.listener,value.get<bool>());else if(type=="text"||type=="integer"){auto text=type=="text"?value.get<std::string>():value.dump();c.textbox.set_text(text.c_str());c.observed=text;c.edited_at=0;}else if(type=="choice"){auto p=plugins.find(plugin);if(p!=plugins.end())for(auto &f:p->second.descriptor["settings"])if(f["key"]==key)for(size_t i=0;i<f["choices"].size();++i)if(f["choices"][i]==value)c.choice.set((int)i);}}c.value=value;}
 }
}
static void drain(){
 if(GetCurrentThreadId()!=thread_id)return;
 struct Event {std::string plugin,key,type;json value;};std::vector<Event> events;auto now=GetTickCount64();
 for(auto &pane:panes)for(auto &item:pane.second.controls){auto &control=*item.second;if(!control.component())continue;bool changed=false;json value;
  if(control.type=="action"||control.type=="open"){while(control.pending){--control.pending;events.push_back({control.plugin,control.key,control.type,json()});}continue;}
  if(control.type=="boolean"||control.type=="visible"){if(control.pending){control.pending=0;bool checked=false;if(native_widget_listener::get_toggle(control.toggle.listener,checked)){value=checked;changed=value!=control.value;}}}
  else if(control.type=="choice"){int selected=-1;if(control.choice.get(selected)&&selected>=0){auto plugin=plugins.find(control.plugin);if(plugin!=plugins.end())for(const auto &field:plugin->second.descriptor["settings"])if(field["key"]==control.key&&selected<(int)field["choices"].size()){value=field["choices"][selected];changed=value!=control.value;break;}}}
  else if(control.type=="text"||control.type=="integer"){
   std::string current;if(control.textbox.get_text(current)){if(current!=control.observed){control.observed=current;control.edited_at=now;}if(control.edited_at&&now-control.edited_at>=500){control.edited_at=0;if(control.type=="text"){value=current;changed=value!=control.value;}else try{size_t used=0;auto number=std::stoll(current,&used);if(used==current.size()){value=number;changed=value!=control.value;}}catch(...){}}}
  }
  if(changed){events.push_back({control.plugin,control.key,control.type,value});}
 }
 for(auto &event:events){auto found=plugins.find(event.plugin);if(found==plugins.end())continue;
  if(event.type=="action"){notify(event.plugin,{{"kind","action"},{"action",event.key}});continue;}
  if(event.type=="open"){if(open_category)open_category(event.plugin.c_str());continue;}
  if(event.type=="visible"){auto next=visibility;next[event.plugin]=event.value;if(save_state(preferences,next)){commit_value(event.plugin,event.key,event.type,event.value,false);if(categories_changed)categories_changed();refresh_plugin(event.plugin,true);}else commit_value(event.plugin,event.key,event.type,event.value,true);continue;}
  for(const auto &field:found->second.descriptor["settings"])if(field["key"]==event.key&&field_value(field,event.value)){auto values=found->second.values;values[event.key]=event.value;if(save_values(event.plugin,values)){found->second.values=values;commit_value(event.plugin,event.key,event.type,event.value,false);notify(event.plugin,{{"kind","settings"},{"settings",values}});}else commit_value(event.plugin,event.key,event.type,event.value,true);break;}
 }
}
static void diagnostic(){
 if(diagnostic_file.empty())return;json items=json::array();for(const auto &item:panes){const auto &pane=item.second;items.push_back({{"id",pane.id},{"visible",pane.visible},{"ready",pane.ready},{"width",pane.bounds.right-pane.bounds.left},{"height",pane.bounds.bottom-pane.bounds.top},{"nativeControls",pane.controls.size()}});}
 auto body=json{{"processId",GetCurrentProcessId()},{"mainThreadId",thread_id},{"renderer","sv-components"},{"foreignContentWindows",0},{"panes",items},{"categoryCount",plugins.size()}}.dump(2);if(body==last_diagnostic)return;std::error_code ec;std::filesystem::create_directories(diagnostic_file.parent_path(),ec);if(ec)return;auto temp=diagnostic_file;temp+=L".tmp-"+std::to_wstring(GetCurrentProcessId());HANDLE file=CreateFileW(temp.c_str(),GENERIC_WRITE,0,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr);if(file==INVALID_HANDLE_VALUE)return;DWORD written=0;bool ok=WriteFile(file,body.data(),(DWORD)body.size(),&written,nullptr)&&written==body.size();if(!CloseHandle(file))ok=false;if(ok)ok=MoveFileExW(temp.c_str(),diagnostic_file.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)!=FALSE;if(ok)last_diagnostic=body;else DeleteFileW(temp.c_str());
}
[[maybe_unused]] static bool dock_update(void *identity,const char *id,HWND window,const RECT &bounds,bool visible){
 if(GetCurrentThreadId()!=thread_id||!identity||!id||!window||!IsWindow(window)||bounds.right<bounds.left||bounds.bottom<bounds.top||(*id&&!plugins.count(id)))return false;
 DWORD pid=0;if(GetWindowThreadProcessId(window,&pid)!=thread_id||pid!=GetCurrentProcessId())return false;host=window;
 auto found=panes.find(identity);if(found==panes.end()){found=panes.try_emplace(identity,identity,id).first;found->second.window=window;auto &created=found->second;if(!created.viewport.create(identity,owned,thread_id)||!created.content.create(thread_id)||!created.viewport.set_content(created.content.object,false)||!reconcile(created)){bool clean=true;for(auto &item:created.controls)if(!item.second->destroy())clean=false;if(clean&&created.viewport.destroy()&&created.content.destroy())panes.erase(found);return false;}}
 auto &pane=found->second;if(pane.id!=id||pane.window!=window)return false;pane.bounds=bounds;pane.visible=visible;layout(pane);diagnostic();return pane.ready;
}
[[maybe_unused]] static void dock_remove(void *identity){if(GetCurrentThreadId()!=thread_id)return;auto found=panes.find(identity);if(found==panes.end())return;bool ok=true;for(auto &item:found->second.controls)if(!item.second->destroy())ok=false;if(ok)ok=found->second.viewport.destroy()&&found->second.content.destroy();if(ok)panes.erase(found);diagnostic();}
static BOOL CALLBACK find_host(HWND w,LPARAM result){if(!IsWindowVisible(w)||GetWindow(w,GW_OWNER)||(GetWindowLongPtrW(w,GWL_EXSTYLE)&WS_EX_TOOLWINDOW))return TRUE;wchar_t name[256]{};if(!GetClassNameW(w,name,256)||wcsncmp(name,L"JUCE_",5)!=0)return TRUE;auto style=GetWindowLongPtrW(w,GWL_STYLE);RECT rect{};GetClientRect(w,&rect);if(!(style&WS_THICKFRAME)||!(style&WS_SYSMENU)||rect.right<400||rect.bottom<300)return TRUE;*reinterpret_cast<HWND*>(result)=w;return FALSE;}
static int install(HWND window,const wchar_t *profile_root){if(!window||!profile_root||!*profile_root)return LOADER_ERR_INVALID;dispatcher=window;thread_id=GetCurrentThreadId();diagnostic_file=std::filesystem::path(profile_root)/L"outputs"/L"windows-panel.json";std::filesystem::path dir=std::filesystem::path(profile_root)/L"preferences";std::error_code ec;std::filesystem::create_directories(dir,ec);preference_file=dir/L"windows-plugin-ui.json";
 if(!ec){DWORD attr=GetFileAttributesW(preference_file.c_str());if(attr!=INVALID_FILE_ATTRIBUTES&&(attr&(FILE_ATTRIBUTE_REPARSE_POINT|FILE_ATTRIBUTE_DIRECTORY)))return LOADER_ERR_INVALID;auto size=std::filesystem::file_size(preference_file,ec);if(ec==std::errc::no_such_file_or_directory){ec.clear();preferences_writable=true;}else if(!ec&&size<=262144){std::ifstream input(preference_file,std::ios::binary);json saved=json::parse(input,nullptr,false);if(saved.is_object()&&saved.contains("schema")&&saved["schema"].is_number_integer()&&saved["schema"]==1&&saved.contains("plugins")&&saved["plugins"].is_object()&&saved["plugins"].size()<=128){preference_document=saved;preferences=saved["plugins"];if(saved.contains("categoryVisibility")){if(!saved["categoryVisibility"].is_object())return LOADER_ERR_INVALID;for(auto it=saved["categoryVisibility"].begin();it!=saved["categoryVisibility"].end();++it)if(!it.value().is_boolean())return LOADER_ERR_INVALID;visibility=saved["categoryVisibility"]; }preferences_writable=true;}}}
 return LOADER_OK;}
static int register_ui(const char *raw,loader_plugin_ui_event event,void *owner){if(GetCurrentThreadId()!=thread_id)return LOADER_ERR_INVALID;json d=parse(raw);if(!valid(d)||plugins.size()>=32)return LOADER_ERR_INVALID;std::string id=d["id"];if(plugins.count(id))return LOADER_ERR_INVALID;json values=json::object();for(auto &f:d["settings"]){std::string key=f["key"];json value=f["default"];if(preferences.contains(id)&&preferences[id].is_object()&&preferences[id].contains(key)&&field_value(f,preferences[id][key]))value=preferences[id][key];values[key]=value;}plugins.emplace(id,Plugin{d,values,event,owner});++generation;refresh_plugin(id,true);if(categories_changed)categories_changed();notify(id,json{{"kind","settings"},{"settings",values}});return LOADER_OK;}
static int unregister_ui(const char *raw,void *owner){if(GetCurrentThreadId()!=thread_id||!raw)return LOADER_ERR_INVALID;auto it=plugins.find(raw);if(it==plugins.end()||it->second.owner!=owner)return LOADER_ERR_INVALID;std::string id=it->first;plugins.erase(it);++generation;refresh_plugin(id,true);if(categories_changed)categories_changed();return LOADER_OK;}
static int update_ui(const char *raw,const char *body,void *owner){if(GetCurrentThreadId()!=thread_id||!raw)return LOADER_ERR_INVALID;auto it=plugins.find(raw);json state=parse(body);if(it==plugins.end()||it->second.owner!=owner||!state.is_object())return LOADER_ERR_INVALID;json next=it->second.descriptor;for(auto i=state.begin();i!=state.end();++i){if(i.key()!="status"&&i.key()!="sidebar"&&i.key()!="settings")return LOADER_ERR_INVALID;next[i.key()]=i.value();}if(!valid(next))return LOADER_ERR_INVALID;if(next==it->second.descriptor)return LOADER_OK;auto old_values=it->second.values;json values=json::object();for(const auto &field:next["settings"]){auto key=field["key"].get<std::string>();values[key]=old_values.contains(key)&&field_value(field,old_values[key])?old_values[key]:field["default"];}it->second.descriptor=next;it->second.values=values;++generation;refresh_plugin(raw,true);for(const auto &field:next["settings"]){auto key=field["key"].get<std::string>();commit_value(raw,key,field["type"],values[key],false);}if(values!=old_values)notify(raw,{{"kind","settings"},{"settings",values}});return LOADER_OK;}

static json snapshot(){json items=json::array();for(auto &entry:plugins){auto item=entry.second.descriptor;item["visible"]=!visibility.contains(entry.first)||visibility[entry.first].get<bool>();item["values"]=entry.second.values;items.push_back(std::move(item));}return {{"generation",generation},{"plugins",items},{"canPersist",preferences_writable},{"persistenceError",preferences_writable?"":"UI_STORAGE_UNAVAILABLE"},{"hostPid",GetCurrentProcessId()},{"extendEntryVisible",true},{"manager",{{"status","Native packages are installed through the Windows installer."},{"installAvailable",false},{"updateAvailable",false}}}};}
static json call(const std::string &method,const json &args){
 auto fail=[](const char *code){return json{{"ok",false},{"error",{{"code",code}}}};};auto success=[](){return json{{"ok",true},{"result",snapshot()}};};
 if(GetCurrentThreadId()!=thread_id)return fail("MAIN_THREAD_REQUIRED");if(!args.is_object())return fail("INVALID_ARGUMENT");
 if(method=="ui.snapshot")return args.empty()?success():fail("INVALID_ARGUMENT");
 if(method!="ui.action"&&method!="ui.set_visible"&&method!="ui.set_setting"&&method!="ui.set_settings")return fail("UNSUPPORTED_UI_METHOD");
 if(!args.contains("generation")||!args["generation"].is_number_integer()||args["generation"]!=json(generation))return fail("STALE_UI_GENERATION");
 if(!args.contains("id")||!args["id"].is_string())return fail("PLUGIN_UI_NOT_FOUND");auto id=args["id"].get<std::string>();auto found=plugins.find(id);if(found==plugins.end())return fail("PLUGIN_UI_NOT_FOUND");
 if(method=="ui.action"){
  if(args.size()!=3||!args.contains("action")||!name(args["action"]))return fail("INVALID_ARGUMENT");bool available=false;for(auto &row:found->second.descriptor["sidebar"]["rows"])if(row["type"]=="button"&&row["id"]==args["action"])available=true;
  if(!available||!found->second.event)return fail("UI_ACTION_UNAVAILABLE");notify(id,{{"kind","action"},{"action",args["action"]}});return success();
 }
 if(method=="ui.set_visible"){
  if(args.size()!=3||!args.contains("visible")||!args["visible"].is_boolean())return fail("INVALID_ARGUMENT");auto next=visibility;next[id]=args["visible"];if(!save_state(preferences,next))return fail("UI_PREFERENCES_WRITE_FAILED");commit_value(id,"visible","visible",args["visible"],false);if(categories_changed)categories_changed();refresh_plugin(id,true);return success();
 }
 auto values=found->second.values;
 if(method=="ui.set_setting"){
  if(args.size()!=4||!args.contains("key")||!name(args["key"])||!args.contains("value"))return fail("INVALID_ARGUMENT");bool valid=false;for(auto &field:found->second.descriptor["settings"])if(field["key"]==args["key"]&&field_value(field,args["value"]))valid=true;if(!valid)return fail("INVALID_SETTING_VALUE");values[args["key"].get<std::string>()]=args["value"];
 }else if(method=="ui.set_settings"){
  if(args.size()!=3||!args.contains("settings")||!args["settings"].is_object())return fail("INVALID_ARGUMENT");if(args["settings"].size()!=found->second.descriptor["settings"].size())return fail("INVALID_SETTING_VALUE");for(auto &field:found->second.descriptor["settings"]){auto key=field["key"].get<std::string>();if(!args["settings"].contains(key)||!field_value(field,args["settings"][key]))return fail("INVALID_SETTING_VALUE");}values=args["settings"];
 }else return fail("UNSUPPORTED_UI_METHOD");
 if(!save_values(id,values))return fail("UI_PREFERENCES_WRITE_FAILED");found->second.values=values;for(auto &field:found->second.descriptor["settings"]){auto key=field["key"].get<std::string>();commit_value(id,key,field["type"],values[key],false);}notify(id,{{"kind","settings"},{"settings",values}});return success();
}
}
#endif


