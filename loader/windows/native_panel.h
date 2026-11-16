#ifndef EXTEND_NATIVE_PANEL_H
#define EXTEND_NATIVE_PANEL_H
#include <windows.h>
#include <map>
#include <string>
#include <vector>
#include <set>
#include <cmath>
#include <filesystem>
#include <fstream>
#include "vendor/json.hpp"
#include "../../sdk/include/loader_api.h"

namespace native_panel {
using json=nlohmann::json;
struct Plugin { json descriptor, values; loader_plugin_ui_event event; void *owner; };
struct Control { std::string plugin, key, type; };
static std::map<std::string,Plugin> plugins;
static std::map<int,Control> controls;
static HWND host=nullptr, panel=nullptr, toggle=nullptr, dispatcher=nullptr;
static WNDPROC original=nullptr;
static DWORD thread_id=0;
static bool expanded=false, rebuilding=false;
static int content_height=0, scroll_position=0;
static HBRUSH background=nullptr;
static std::filesystem::path preference_file, diagnostic_file;
static std::string last_diagnostic;
static ULONGLONG installed_at=0;
static json preferences=json::object();
static bool preferences_writable=false;
static std::string selected;
static const wchar_t *property=L"SV2Extend.NativePanel.Original";
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
static bool save_values(const std::string &id,const json &values) {
 if(!preferences_writable)return false;json next=preferences;next[id]=values;std::string data=json{{"schema",1},{"plugins",next}}.dump();if(data.size()>262144)return false;
 auto temp=preference_file;temp+=L".tmp-"+std::to_wstring(GetCurrentProcessId())+L"-"+std::to_wstring(GetTickCount64());HANDLE file=CreateFileW(temp.c_str(),GENERIC_WRITE,0,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr);if(file==INVALID_HANDLE_VALUE)return false;
 DWORD written=0;bool ok=WriteFile(file,data.data(),(DWORD)data.size(),&written,nullptr)&&written==data.size()&&FlushFileBuffers(file);if(!CloseHandle(file))ok=false;if(ok)ok=MoveFileExW(temp.c_str(),preference_file.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)!=0;if(!ok){DeleteFileW(temp.c_str());return false;}preferences=next;return true;
}
static void notify(const std::string &id,const json &event) {auto it=plugins.find(id);if(it==plugins.end()||!it->second.event)return; auto callback=it->second.event;auto owner=it->second.owner;std::string body=event.dump();callback(body.c_str(),owner);}
static std::string utf8(const std::wstring &s);
static BOOL CALLBACK diagnostic_window(HWND w,LPARAM context){auto &list=*(json*)context;if(list.size()>=32)return FALSE;wchar_t cls[256]{};if(!GetClassNameW(w,cls,256))return TRUE;RECT r{};GetClientRect(w,&r);list.push_back({{"class",utf8(cls)},{"style",(uint64_t)(DWORD)GetWindowLongPtrW(w,GWL_STYLE)},{"exStyle",(uint64_t)(DWORD)GetWindowLongPtrW(w,GWL_EXSTYLE)},{"visible",IsWindowVisible(w)!=FALSE},{"owned",GetWindow(w,GW_OWNER)!=nullptr},{"clientWidth",r.right-r.left},{"clientHeight",r.bottom-r.top}});return TRUE;}
static void diagnostic(){
 if(diagnostic_file.empty()||(!host&&GetTickCount64()-installed_at<2000))return;json ids=json::array();for(auto &p:plugins)ids.push_back(p.first);json candidates=json::array();if(!host)EnumThreadWindows(thread_id,diagnostic_window,(LPARAM)&candidates);
 DWORD host_pid=0,child_pid=0;DWORD host_thread=host?GetWindowThreadProcessId(host,&host_pid):0,child_thread=panel?GetWindowThreadProcessId(panel,&child_pid):0;wchar_t cls[256]{};if(host)GetClassNameW(host,cls,256);
 json snapshot={{"processId",GetCurrentProcessId()},{"mainThreadId",thread_id},{"attached",host&&panel&&IsWindow(host)&&IsWindow(panel)&&GetParent(panel)==host},{"hostClass",utf8(cls)},{"hostVisible",host&&IsWindowVisible(host)},{"hostProcessId",host_pid},{"hostThreadId",host_thread},{"childProcessId",child_pid},{"childThreadId",child_thread},{"expanded",expanded},{"pluginIds",ids},{"candidates",candidates}};std::string body=snapshot.dump(2);if(body==last_diagnostic)return;
 std::error_code ec;std::filesystem::create_directories(diagnostic_file.parent_path(),ec);if(ec)return;DWORD attr=GetFileAttributesW(diagnostic_file.parent_path().c_str());if(attr==INVALID_FILE_ATTRIBUTES||(attr&FILE_ATTRIBUTE_REPARSE_POINT)||!(attr&FILE_ATTRIBUTE_DIRECTORY))return;
 auto temporary=diagnostic_file;temporary+=L".tmp-"+std::to_wstring(GetCurrentProcessId())+L"-"+std::to_wstring(GetTickCount64());HANDLE file=CreateFileW(temporary.c_str(),GENERIC_WRITE,0,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr);if(file==INVALID_HANDLE_VALUE)return;DWORD written=0;bool ok=WriteFile(file,body.data(),(DWORD)body.size(),&written,nullptr)&&written==body.size();if(!CloseHandle(file))ok=false;if(ok)ok=MoveFileExW(temporary.c_str(),diagnostic_file.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)!=0;if(ok)last_diagnostic=body;else DeleteFileW(temporary.c_str());
}
static void layout() {if(!host||!IsWindow(host))return;RECT r{};GetClientRect(host,&r);int width=r.right-r.left,height=r.bottom-r.top;int pw=expanded?std::min(300,std::max(0,width-44)):0; if(toggle)MoveWindow(toggle,std::max(0,width-44),4,40,28,TRUE);if(panel){MoveWindow(panel,std::max(0,width-pw),36,pw,std::max(0,height-36),TRUE);ShowWindow(panel,expanded?SW_SHOW:SW_HIDE);} diagnostic(); }
static HWND add(const wchar_t *cls,const std::wstring &value,DWORD style,int x,int y,int w,int h,int id) {HWND c=CreateWindowExW(0,cls,value.c_str(),WS_CHILD|WS_VISIBLE|style,x,y,w,h,panel,(HMENU)(INT_PTR)id,GetModuleHandleW(nullptr),nullptr);if(c)SendMessageW(c,WM_SETFONT,(WPARAM)GetStockObject(DEFAULT_GUI_FONT),TRUE);return c;}
static void update_scroll() {
 if(!panel)return;RECT r{};GetClientRect(panel,&r);SCROLLINFO info{sizeof(info),SIF_RANGE|SIF_PAGE|SIF_POS,0,std::max(0,content_height-1),(UINT)std::max(0L,r.bottom),scroll_position,0};SetScrollInfo(panel,SB_VERT,&info,TRUE);
}
static void refresh() {
 if(!panel)return;rebuilding=true; scroll_position=0; while(HWND child=GetWindow(panel,GW_CHILD))DestroyWindow(child);controls.clear(); int y=8,id=100; add(L"STATIC",L"Extend",0,8,y,270,22,id++);y+=26;
 for(auto &entry:plugins){int current=id++;controls[current]={entry.first,"","select"};add(L"BUTTON",wide(entry.second.descriptor["name"].get<std::string>()),BS_PUSHBUTTON,8,y,270,26,current);y+=30;}
 auto it=plugins.find(selected);if(it!=plugins.end()) {auto &p=it->second;add(L"STATIC",wide(p.descriptor["sidebar"]["title"].get<std::string>()),0,8,y,270,22,id++);y+=24;add(L"STATIC",wide(p.descriptor["status"].get<std::string>()),0,8,y,270,22,id++);y+=24;
 for(auto &row:p.descriptor["sidebar"]["rows"]) {std::string type=row["type"];int current=id++;if(type=="button")controls[current]={it->first,row["id"].get<std::string>(),"action"};add(type=="button"?L"BUTTON":L"STATIC",wide(row["text"].get<std::string>()),type=="button"?BS_PUSHBUTTON:0,8,y,270,26,current);y+=30;}
 for(auto &f:p.descriptor["settings"]) {std::string key=f["key"],type=f["type"];int current=id++;controls[current]={it->first,key,type};std::wstring title=wide(f["title"].get<std::string>());if(type=="boolean"){HWND c=add(L"BUTTON",title,BS_AUTOCHECKBOX,8,y,270,24,current);SendMessageW(c,BM_SETCHECK,p.values[key].get<bool>()?BST_CHECKED:BST_UNCHECKED,0);y+=28;}else{add(L"STATIC",title,0,8,y,270,20,id++);y+=20;if(type=="choice"){HWND c=add(L"COMBOBOX",L"",CBS_DROPDOWNLIST|WS_VSCROLL,8,y,270,200,current);int index=0,chosen=0;for(auto &choice:f["choices"]){auto w=wide(choice.get<std::string>());SendMessageW(c,CB_ADDSTRING,0,(LPARAM)w.c_str());if(choice==p.values[key])chosen=index;index++;}SendMessageW(c,CB_SETCURSEL,chosen,0);}else{std::string v=type=="text"?p.values[key].get<std::string>():p.values[key].dump();add(L"EDIT",wide(v),WS_BORDER|ES_AUTOHSCROLL,8,y,270,24,current);}y+=28;}}
 }content_height=y+8;update_scroll();rebuilding=false;
}
static std::string utf8(const std::wstring &s){int n=WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,s.data(),(int)s.size(),nullptr,0,nullptr,nullptr);if(n<=0)return {};std::string out((size_t)n,'\0');WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,s.data(),(int)s.size(),out.data(),n,nullptr,nullptr);return out;}
static LRESULT CALLBACK panel_proc(HWND w,UINT msg,WPARAM wp,LPARAM lp){
 if(msg==WM_SIZE){update_scroll();return 0;}
 if(msg==WM_CTLCOLORSTATIC || msg==WM_CTLCOLORBTN){SetTextColor((HDC)wp,RGB(235,230,223));SetBkColor((HDC)wp,RGB(42,40,39));return (LRESULT)background;}
 if(msg==WM_ERASEBKGND){RECT r{};GetClientRect(w,&r);FillRect((HDC)wp,&r,background);return 1;}
 if(msg==WM_VSCROLL || msg==WM_MOUSEWHEEL){SCROLLINFO info{sizeof(info),SIF_ALL,0,0,0,0,0};GetScrollInfo(w,SB_VERT,&info);int next=scroll_position;if(msg==WM_MOUSEWHEEL)next-=GET_WHEEL_DELTA_WPARAM(wp)/WHEEL_DELTA*48;else switch(LOWORD(wp)){case SB_LINEUP:next-=24;break;case SB_LINEDOWN:next+=24;break;case SB_PAGEUP:next-=(int)info.nPage;break;case SB_PAGEDOWN:next+=(int)info.nPage;break;case SB_THUMBTRACK:next=info.nTrackPos;break;default:return 0;}next=std::max(0,std::min(next,std::max(0,info.nMax-(int)info.nPage+1)));int delta=scroll_position-next;scroll_position=next;ScrollWindowEx(w,0,delta,nullptr,nullptr,nullptr,nullptr,SW_SCROLLCHILDREN|SW_INVALIDATE);update_scroll();return 0;}
 if(msg==WM_COMMAND&&!rebuilding){int id=LOWORD(wp);auto c=controls.find(id);if(c==controls.end())return 0;Control control=c->second;auto p=plugins.find(control.plugin);if(p==plugins.end())return 0;
 if(control.type=="select"&&HIWORD(wp)==BN_CLICKED){selected=control.plugin;refresh();return 0;}if(control.type=="action"&&HIWORD(wp)==BN_CLICKED){notify(control.plugin,json{{"kind","action"},{"action",control.key}});return 0;}
 HWND input=(HWND)lp;json value;bool changed=false;
 if(control.type=="boolean"&&HIWORD(wp)==BN_CLICKED){value=SendMessageW(input,BM_GETCHECK,0,0)==BST_CHECKED;changed=true;}
 if(control.type=="choice"&&HIWORD(wp)==CBN_SELCHANGE){int index=(int)SendMessageW(input,CB_GETCURSEL,0,0);for(auto &f:p->second.descriptor["settings"])if(f["key"]==control.key&&index>=0&&(size_t)index<f["choices"].size()){value=f["choices"][index];changed=true;break;}}
 if((control.type=="text"||control.type=="integer")&&HIWORD(wp)==EN_KILLFOCUS){int n=GetWindowTextLengthW(input);std::wstring s((size_t)n+1,L'\0');GetWindowTextW(input,s.data(),n+1);s.resize((size_t)n);std::string v=utf8(s);if(control.type=="text"){value=v;changed=true;}else{try{size_t used=0;long long x=std::stoll(v,&used);if(used==v.size()){value=x;changed=true;}}catch(...){}}}
 if(changed){for(auto &f:p->second.descriptor["settings"])if(f["key"]==control.key&&field_value(f,value)){json values=p->second.values;values[control.key]=value;if(save_values(control.plugin,values)){p->second.values=values;notify(control.plugin,json{{"kind","settings"},{"settings",values}});}else refresh();break;}}return 0;
 }return DefWindowProcW(w,msg,wp,lp);
}
static LRESULT CALLBACK host_proc(HWND w,UINT msg,WPARAM wp,LPARAM lp){WNDPROC previous=(WNDPROC)GetPropW(w,property);if(!previous)return DefWindowProcW(w,msg,wp,lp);if(msg==WM_COMMAND&&(HWND)lp==toggle&&HIWORD(wp)==BN_CLICKED){expanded=!expanded;layout();return 0;}if(msg==WM_SIZE){LRESULT result=CallWindowProcW(previous,w,msg,wp,lp);layout();return result;}if(msg==WM_NCDESTROY){LRESULT result=CallWindowProcW(previous,w,msg,wp,lp);RemovePropW(w,property);if(host==w){host=panel=toggle=nullptr;original=nullptr;diagnostic();}return result;}return CallWindowProcW(previous,w,msg,wp,lp);}
static BOOL CALLBACK find_host(HWND w,LPARAM result){if(!IsWindowVisible(w)||GetWindow(w,GW_OWNER)||(GetWindowLongPtrW(w,GWL_EXSTYLE)&WS_EX_TOOLWINDOW))return TRUE;wchar_t class_name[256]{};if(!GetClassNameW(w,class_name,256)||wcsncmp(class_name,L"JUCE_",5)!=0)return TRUE;LONG_PTR style=GetWindowLongPtrW(w,GWL_STYLE);if(!(style&WS_THICKFRAME)||!(style&WS_SYSMENU))return TRUE;RECT r{};GetClientRect(w,&r);if(r.right<400||r.bottom<300)return TRUE;*(HWND*)result=w;return FALSE;}
static void attach(){if(host&&IsWindow(host))return;HWND candidate=nullptr;EnumThreadWindows(thread_id,find_host,(LPARAM)&candidate);if(!candidate)return;WNDCLASSW cls{};cls.lpfnWndProc=panel_proc;cls.hInstance=GetModuleHandleW(nullptr);cls.lpszClassName=L"SV2ExtendNativePanel";if(!background)background=CreateSolidBrush(RGB(42,40,39));cls.hbrBackground=background;RegisterClassW(&cls);HWND p=CreateWindowExW(0,cls.lpszClassName,L"",WS_CHILD|WS_CLIPCHILDREN|WS_VSCROLL,0,0,0,0,candidate,nullptr,cls.hInstance,nullptr);HWND t=CreateWindowExW(0,L"BUTTON",L"Ext",WS_CHILD|WS_VISIBLE|BS_PUSHBUTTON,0,0,40,28,candidate,nullptr,cls.hInstance,nullptr);if(!p||!t){if(p)DestroyWindow(p);if(t)DestroyWindow(t);return;}WNDPROC prior=(WNDPROC)GetWindowLongPtrW(candidate,GWLP_WNDPROC);if(!prior||!SetPropW(candidate,property,(HANDLE)prior)){DestroyWindow(p);DestroyWindow(t);return;}SetLastError(0);LONG_PTR old=SetWindowLongPtrW(candidate,GWLP_WNDPROC,(LONG_PTR)host_proc);if(!old&&GetLastError()){RemovePropW(candidate,property);DestroyWindow(p);DestroyWindow(t);return;}host=candidate;panel=p;toggle=t;original=prior;SendMessageW(t,WM_SETFONT,(WPARAM)GetStockObject(DEFAULT_GUI_FONT),TRUE);refresh();layout();}
static void CALLBACK timer(HWND,UINT,UINT_PTR,DWORD){attach();diagnostic();}
static int install(HWND window,const wchar_t *profile_root){if(!window||!profile_root||!*profile_root)return LOADER_ERR_INVALID;dispatcher=window;thread_id=GetCurrentThreadId();installed_at=GetTickCount64();diagnostic_file=std::filesystem::path(profile_root)/L"outputs"/L"windows-panel.json";std::filesystem::path dir=std::filesystem::path(profile_root)/L"preferences";std::error_code ec;std::filesystem::create_directories(dir,ec);preference_file=dir/L"windows-plugin-ui.json";
 if(!ec){auto size=std::filesystem::file_size(preference_file,ec);if(ec==std::errc::no_such_file_or_directory){ec.clear();preferences_writable=true;}else if(!ec&&size<=262144){std::ifstream input(preference_file,std::ios::binary);json saved=json::parse(input,nullptr,false);if(saved.is_object()&&saved.contains("schema")&&saved["schema"].is_number_integer()&&saved["schema"]==1&&saved.contains("plugins")&&saved["plugins"].is_object()&&saved["plugins"].size()<=128){preferences=saved["plugins"];preferences_writable=true;}}}
 return SetTimer(dispatcher,0x53563255,500,timer)?LOADER_OK:LOADER_ERR_INVALID;}
static int register_ui(const char *raw,loader_plugin_ui_event event,void *owner){if(GetCurrentThreadId()!=thread_id)return LOADER_ERR_INVALID;json d=parse(raw);if(!valid(d)||plugins.size()>=32)return LOADER_ERR_INVALID;std::string id=d["id"];if(plugins.count(id))return LOADER_ERR_INVALID;json values=json::object();for(auto &f:d["settings"]){std::string key=f["key"];json value=f["default"];if(preferences.contains(id)&&preferences[id].is_object()&&preferences[id].contains(key)&&field_value(f,preferences[id][key]))value=preferences[id][key];values[key]=value;}plugins.emplace(id,Plugin{d,values,event,owner});if(selected.empty())selected=id;refresh();diagnostic();notify(id,json{{"kind","settings"},{"settings",values}});return LOADER_OK;}
static int unregister_ui(const char *raw,void *owner){if(GetCurrentThreadId()!=thread_id||!raw)return LOADER_ERR_INVALID;auto it=plugins.find(raw);if(it==plugins.end()||it->second.owner!=owner)return LOADER_ERR_INVALID;plugins.erase(it);if(selected==raw)selected=plugins.empty()?std::string():plugins.begin()->first;refresh();diagnostic();return LOADER_OK;}
static int update_ui(const char *raw,const char *body,void *owner){if(GetCurrentThreadId()!=thread_id||!raw)return LOADER_ERR_INVALID;auto it=plugins.find(raw);json state=parse(body);if(it==plugins.end()||it->second.owner!=owner||!state.is_object())return LOADER_ERR_INVALID;json next=it->second.descriptor;for(auto i=state.begin();i!=state.end();++i){if(i.key()!="status"&&i.key()!="sidebar")return LOADER_ERR_INVALID;next[i.key()]=i.value();}if(!valid(next))return LOADER_ERR_INVALID;it->second.descriptor=next;refresh();return LOADER_OK;}
}
#endif


