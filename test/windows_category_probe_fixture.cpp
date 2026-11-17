#include "../loader/windows/native_panel.h"
#include <cassert>
namespace native_category_registry { struct Category{bool visible=true;};Category categories[1];bool prepared=true;unsigned char *image=nullptr;DWORD thread_id=GetCurrentThreadId();std::set<void*> bars;bool readable(const void*,size_t){return false;}}
static bool editor_online(){return true;}
static bool native_change(const char *id,bool open){return std::string(id)=="sv2.mcp"&&open;}
static bool native_state(const char*,bool *value){*value=true;return true;}
#define SV2_TEST_CATEGORY_CHANGE native_change
#define SV2_TEST_CATEGORY_OPEN_STATE native_state
#include "windows_category_probe.h"
static std::string reply_body;
static void completed(const char *body,size_t length,void*){reply_body.assign(body,length);}
static void close_completed(const char *body,size_t length,void *context){MSG pending{};assert(!PeekMessageW(&pending,native_panel::host,WM_CLOSE,WM_CLOSE,PM_NOREMOVE));completed(body,length,context);}
static int register_provider(const char*,loader_editor_dispatch,void*,const char*){return LOADER_OK;}
static int update_calls=0;static bool disturb=false;
static int update_ui(const char*,const char*,void*){++update_calls;if(disturb&&!native_panel::panes.empty())native_panel::panes.begin()->second.scroll++;return LOADER_OK;}
static int register_ui(const char*,loader_plugin_ui_event event,void *owner){event("{\"kind\":\"settings\"}",owner);return LOADER_OK;}
int main(){
 loader_api_v1 api{};api.register_capability_provider=register_provider;api.register_plugin_ui=register_ui;api.update_plugin_ui=update_ui;assert(category_probe::install(api));assert(category_probe::settings_events==1);
 category_probe::dispatch("test.category.change","{\"id\":\"sv2.mcp\",\"open\":true}",completed,nullptr,nullptr);assert(nlohmann::json::parse(reply_body)["ok"]);
 category_probe::dispatch("test.category.change","{\"id\":\"arbitrary\",\"open\":true}",completed,nullptr,nullptr);assert(!nlohmann::json::parse(reply_body)["ok"]);
 category_probe::dispatch("test.category.state","{}",completed,nullptr,nullptr);auto result=nlohmann::json::parse(reply_body);assert(result["ok"]&&result["result"]["nativePrepared"]&&result["result"]["editorReady"]);
 HWND window=CreateWindowExW(0,L"STATIC",L"",WS_OVERLAPPEDWINDOW,0,0,900,700,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);assert(window);native_panel::host=window;
 ShowWindow(window,SW_SHOW);auto key=reinterpret_cast<void*>(1);auto inserted=native_panel::panes.emplace(std::piecewise_construct,std::forward_as_tuple(key),std::forward_as_tuple(key,"test.category.fixture"));inserted.first->second.window=window;inserted.first->second.controls.emplace(100,native_panel::Control{"test.category.fixture","action","action"});HWND child=CreateWindowExW(0,L"BUTTON",L"",WS_CHILD|WS_VISIBLE,0,0,80,25,window,reinterpret_cast<HMENU>(100),GetModuleHandleW(nullptr),nullptr);assert(child);SetFocus(child);
 category_probe::dispatch("test.category.control","{\"kind\":\"unchanged\"}",completed,nullptr,nullptr);assert(nlohmann::json::parse(reply_body)["ok"]&&update_calls==5);disturb=true;category_probe::dispatch("test.category.control","{\"kind\":\"unchanged\"}",completed,nullptr,nullptr);assert(!nlohmann::json::parse(reply_body)["ok"]);native_panel::panes.clear();DestroyWindow(child);
 category_probe::dispatch("test.category.state","{}",completed,nullptr,nullptr);auto host_state=nlohmann::json::parse(reply_body)["result"];assert(host_state["hostClientKnown"]&&host_state["hostClientWidth"].get<int>()>0&&host_state["hostClientHeight"].get<int>()>0);assert(!category_probe::inspect_chain(nullptr).known);
 category_probe::dispatch("test.category.close_host","{\"hostPid\":0}",completed,nullptr,nullptr);assert(!nlohmann::json::parse(reply_body)["ok"]);MSG pending{};assert(!PeekMessageW(&pending,window,WM_CLOSE,WM_CLOSE,PM_NOREMOVE));
 auto close_args=nlohmann::json{{"hostPid",GetCurrentProcessId()}}.dump();category_probe::dispatch("test.category.close_host",close_args.c_str(),close_completed,nullptr,nullptr);assert(nlohmann::json::parse(reply_body)["ok"]);assert(PeekMessageW(&pending,window,WM_CLOSE,WM_CLOSE,PM_REMOVE));
 category_probe::dispatch("test.category.monitor","{\"hostPid\":0,\"target\":\"96\"}",completed,nullptr,nullptr);assert(!nlohmann::json::parse(reply_body)["ok"]);
 auto monitor_args=nlohmann::json{{"hostPid",GetCurrentProcessId()},{"target","restore"}}.dump();category_probe::dispatch("test.category.monitor",monitor_args.c_str(),completed,nullptr,nullptr);assert(!nlohmann::json::parse(reply_body)["ok"]);
 auto dpi_fn=reinterpret_cast<UINT(WINAPI*)(HWND)>(GetProcAddress(GetModuleHandleW(L"user32.dll"),"GetDpiForWindow"));if(dpi_fn&&(dpi_fn(window)==96||dpi_fn(window)==144)){auto move_args=nlohmann::json{{"hostPid",GetCurrentProcessId()},{"target",std::to_string(dpi_fn(window))}}.dump();WINDOWPLACEMENT initial{};initial.length=sizeof initial;assert(GetWindowPlacement(window,&initial));category_probe::dispatch("test.category.monitor",move_args.c_str(),completed,nullptr,nullptr);assert(nlohmann::json::parse(reply_body)["ok"]&&IsZoomed(window));category_probe::dispatch("test.category.monitor",monitor_args.c_str(),completed,nullptr,nullptr);assert(nlohmann::json::parse(reply_body)["ok"]);WINDOWPLACEMENT restored{};restored.length=sizeof restored;assert(GetWindowPlacement(window,&restored)&&EqualRect(&initial.rcNormalPosition,&restored.rcNormalPosition)&&initial.showCmd==restored.showCmd);}
 ShowWindow(window,SW_MAXIMIZE);assert(IsZoomed(window));WINDOWPLACEMENT before{};before.length=sizeof before;assert(GetWindowPlacement(window,&before));
 if(dpi_fn&&(dpi_fn(window)==96||dpi_fn(window)==144)){RECT max_before{};assert(GetWindowRect(window,&max_before));UINT before_dpi=dpi_fn(window);auto max_move=nlohmann::json{{"hostPid",GetCurrentProcessId()},{"target",std::to_string(before_dpi)}}.dump();category_probe::dispatch("test.category.monitor",max_move.c_str(),completed,nullptr,nullptr);assert(nlohmann::json::parse(reply_body)["ok"]);category_probe::dispatch("test.category.monitor",monitor_args.c_str(),completed,nullptr,nullptr);RECT max_after{};assert(nlohmann::json::parse(reply_body)["ok"]&&IsZoomed(window)&&GetWindowRect(window,&max_after)&&EqualRect(&max_before,&max_after)&&dpi_fn(window)==before_dpi);before.length=sizeof before;assert(GetWindowPlacement(window,&before));}
 category_probe::dispatch("test.category.resize","{\"restore\":false}",completed,nullptr,nullptr);assert(nlohmann::json::parse(reply_body)["ok"]&&!IsZoomed(window));
 category_probe::dispatch("test.category.resize","{\"restore\":true}",completed,nullptr,nullptr);assert(nlohmann::json::parse(reply_body)["ok"]&&IsZoomed(window));
 WINDOWPLACEMENT after{};after.length=sizeof after;assert(GetWindowPlacement(window,&after)&&EqualRect(&before.rcNormalPosition,&after.rcNormalPosition));DestroyWindow(window);
}
