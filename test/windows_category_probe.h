#pragma once
// Included only by the generated integration-test Core.
#ifndef SV2_TEST_CATEGORY_CHANGE
#error Define SV2_TEST_CATEGORY_CHANGE to the verified native category action.
#endif
#ifndef SV2_TEST_CATEGORY_OPEN_STATE
#error Define SV2_TEST_CATEGORY_OPEN_STATE to the verified native category state reader.
#endif
namespace category_probe {
using json=nlohmann::json;
static int owner, settings_events, action_events;static decltype(loader_api_v1::update_plugin_ui) update_ui=nullptr;static RECT original_bounds{};static WINDOWPLACEMENT original_placement{};static bool resized=false;
struct Chain {bool known=false,self_visible=false,ancestors_visible=false,peer_flag=false,root_peer_flag=false;void *root=nullptr;int depth=0,root_width=0,root_height=0;};
static Chain inspect_chain(void *component){Chain result{};auto node=static_cast<unsigned char*>(component);bool all=true;while(node&&result.depth<128){if(!native_category_registry::readable(node,0xe9))return result;bool visible=(node[0xe8]&2)!=0;if(!result.depth){result.self_visible=visible;result.peer_flag=(node[0xe8]&1)!=0;}else all=all&&visible;result.root=node;++result.depth;unsigned char *parent=nullptr;std::memcpy(&parent,node+0x30,sizeof parent);node=parent;}if(node||!result.root)return result;auto root=static_cast<unsigned char*>(result.root);std::memcpy(&result.root_width,root+0x40,4);std::memcpy(&result.root_height,root+0x44,4);result.root_peer_flag=(root[0xe8]&1)!=0;result.ancestors_visible=all;result.known=true;return result;}
static json peer_geometry(const Chain &chain,HWND host){
 using namespace native_category_registry;json out={{"known",false},{"matches",0},{"scale",0.0},{"rootAffineKnown",false},{"rootAffinePresent",false}};if(chain.known&&readable(chain.root,0x58)){void *affine=nullptr;std::memcpy(&affine,static_cast<unsigned char*>(chain.root)+0x50,sizeof affine);out["rootAffineKnown"]=true;out["rootAffinePresent"]=affine!=nullptr;}if(!chain.known||!image||!host||!readable(image+0x17f0f38,sizeof(void*)))return out;
 void *desktop=nullptr;std::memcpy(&desktop,image+0x17f0f38,sizeof desktop);if(!readable(desktop,0xe0))return out;auto d=static_cast<unsigned char*>(desktop);void **peers=nullptr;int count=0;std::memcpy(&peers,d+0xd0,sizeof peers);std::memcpy(&count,d+0xdc,4);if(count<0||count>256||(count&&!readable(peers,size_t(count)*sizeof(void*))))return out;
 int matches=0;double scale=0;for(int i=0;i<count;i++){auto p=static_cast<unsigned char*>(peers[i]);if(!readable(p,0x178))return out;void *table=nullptr,*component=nullptr;HWND window=nullptr;std::memcpy(&table,p,sizeof table);std::memcpy(&component,p+8,sizeof component);std::memcpy(&window,p+0xf8,sizeof window);if(table==image+0x14f8510&&component==chain.root&&window==host){++matches;std::memcpy(&scale,p+0x170,sizeof scale);}}
 out["matches"]=matches;if(matches==1&&scale>0.0&&scale<100.0){out["known"]=true;out["scale"]=scale;}return out;
}
static json guards(){
 using namespace native_category_registry;json result={{"prepared",prepared},{"mainThread",GetCurrentThreadId()==thread_id},{"bars",json::array()}};if(!prepared||!image)return result;
 for(void *bar:bars){auto bytes=static_cast<unsigned char*>(bar);bool valid=readable(bytes,0x100),matching=false,visible=false,container_ok=false;if(valid){void *table=nullptr,*container=nullptr;std::memcpy(&table,bytes,sizeof table);std::memcpy(&container,bytes+0xf8,sizeof container);matching=table==image+0x14ea908;visible=(bytes[0xe8]&2)!=0;container_ok=readable(container,0xa8);}auto chain=inspect_chain(bar);result["bars"].push_back({{"chainKnown",chain.known},{"rootWidth",chain.root_width},{"rootHeight",chain.root_height},{"peerGeometry",peer_geometry(chain,native_panel::host)},{"readable",valid},{"typeMatches",matching},{"visible",visible},{"containerReadable",container_ok}});}
 return result;
}
static void fixture_event(const char *raw,void *){auto value=raw?json::parse(raw,nullptr,false):json();if(!value.is_object())return;if(value.value("kind","")=="settings")++settings_events;if(value.value("kind","")=="action")++action_events;}
static json ok(json value=json::object()){return {{"ok",true},{"result",std::move(value)}};}
static json fail(const char *code){return {{"ok",false},{"error",{{"code",code}}}};}
static void reply(loader_json_completion done,void *context,const json &value){auto body=value.dump();done(body.data(),body.size(),context);}
struct MonitorSearch {UINT dpi;RECT work{};bool found=false;HRESULT (WINAPI *get_dpi)(HMONITOR,int,UINT*,UINT*)=nullptr;};
static BOOL CALLBACK find_monitor(HMONITOR monitor,HDC,LPRECT,LPARAM context){auto &search=*reinterpret_cast<MonitorSearch*>(context);UINT x=0,y=0;MONITORINFO info{};info.cbSize=sizeof info;if(SUCCEEDED(search.get_dpi(monitor,0,&x,&y))&&x==search.dpi&&y==search.dpi&&GetMonitorInfoW(monitor,&info)){search.work=info.rcWork;search.found=true;return FALSE;}return TRUE;}
static WINDOWPLACEMENT monitor_original{};static RECT monitor_original_bounds{};static UINT monitor_original_dpi=0;static bool monitor_moved=false;
static void dispatch(const char *method,const char *raw,loader_json_completion done,void *context,void *){
 auto args=raw?json::parse(raw,nullptr,false):json();if(!args.is_object()){reply(done,context,fail("INVALID_ARGUMENT"));return;}
 std::string name=method?method:"";
 if(name=="test.category.monitor") {
  if(args.size()!=2||!args.contains("hostPid")||!args["hostPid"].is_number_unsigned()||args["hostPid"].get<uint64_t>()!=GetCurrentProcessId()||!args.contains("target")||!args["target"].is_string()){reply(done,context,fail("INVALID_ARGUMENT"));return;}
  auto target=args["target"].get<std::string>();if(target!="96"&&target!="144"&&target!="restore"){reply(done,context,fail("INVALID_ARGUMENT"));return;}
  HWND host=native_panel::host;DWORD pid=0;if(!host||!IsWindow(host)||GetAncestor(host,GA_ROOT)!=host||!GetWindowThreadProcessId(host,&pid)||pid!=GetCurrentProcessId()||IsIconic(host)||resized){reply(done,context,fail("HOST_UNAVAILABLE"));return;}
  using Dpi=UINT(WINAPI*)(HWND);auto get_window_dpi=reinterpret_cast<Dpi>(GetProcAddress(GetModuleHandleW(L"user32.dll"),"GetDpiForWindow"));if(!get_window_dpi){reply(done,context,fail("DPI_UNAVAILABLE"));return;}
  if(target=="restore"){
   if(!monitor_moved||!SetWindowPlacement(host,&monitor_original)){reply(done,context,fail("RESTORE_UNAVAILABLE"));return;}
   bool was_max=monitor_original.showCmd==SW_SHOWMAXIMIZED;if(was_max){ShowWindow(host,SW_RESTORE);if(!SetWindowPos(host,nullptr,monitor_original_bounds.left,monitor_original_bounds.top,monitor_original_bounds.right-monitor_original_bounds.left,monitor_original_bounds.bottom-monitor_original_bounds.top,SWP_NOZORDER|SWP_NOACTIVATE)){reply(done,context,fail("RESTORE_UNAVAILABLE"));return;}ShowWindow(host,SW_MAXIMIZE);}
   RECT actual{};if(!GetWindowRect(host,&actual)||!EqualRect(&actual,&monitor_original_bounds)||!!IsZoomed(host)!=was_max||get_window_dpi(host)!=monitor_original_dpi){reply(done,context,fail("RESTORE_UNAVAILABLE"));return;}monitor_moved=false;reply(done,context,ok({{"dpi",get_window_dpi(host)}}));return;
  }
  HMODULE shcore=LoadLibraryW(L"shcore.dll");if(!shcore){reply(done,context,fail("DPI_UNAVAILABLE"));return;}MonitorSearch search{};search.dpi=target=="96"?96:144;search.get_dpi=reinterpret_cast<decltype(search.get_dpi)>(GetProcAddress(shcore,"GetDpiForMonitor"));if(search.get_dpi)EnumDisplayMonitors(nullptr,nullptr,find_monitor,reinterpret_cast<LPARAM>(&search));FreeLibrary(shcore);if(!search.found){reply(done,context,fail("MONITOR_UNAVAILABLE"));return;}
  if(!monitor_moved){monitor_original={};monitor_original.length=sizeof monitor_original;if(!GetWindowPlacement(host,&monitor_original)||!GetWindowRect(host,&monitor_original_bounds)){reply(done,context,fail("HOST_UNAVAILABLE"));return;}monitor_original_dpi=get_window_dpi(host);monitor_moved=true;}
  ShowWindow(host,SW_RESTORE);int width=search.work.right-search.work.left,height=search.work.bottom-search.work.top;if(width<=0||height<=0||!SetWindowPos(host,nullptr,search.work.left,search.work.top,width,height,SWP_NOZORDER|SWP_NOACTIVATE)){reply(done,context,fail("MOVE_FAILED"));return;}ShowWindow(host,SW_MAXIMIZE);reply(done,context,ok({{"dpi",get_window_dpi(host)}}));return;
 }
 if(name=="test.category.close_host") {
  if(args.size()!=1||!args.contains("hostPid")||!args["hostPid"].is_number_unsigned()||args["hostPid"].get<uint64_t>()!=GetCurrentProcessId()){reply(done,context,fail("INVALID_ARGUMENT"));return;}
  HWND host=native_panel::host;DWORD owner_pid=0;if(!host||!IsWindow(host)||GetAncestor(host,GA_ROOT)!=host||!GetWindowThreadProcessId(host,&owner_pid)||owner_pid!=GetCurrentProcessId()){reply(done,context,fail("HOST_UNAVAILABLE"));return;}
  reply(done,context,ok());PostMessageW(host,WM_CLOSE,0,0);return;
 }
 if(name=="test.category.pause") {if(!args.empty()){reply(done,context,fail("INVALID_ARGUMENT"));return;}Sleep(2300);reply(done,context,ok());return;}
 if(name=="test.category.resize"){
  if(args.size()!=1||!args.contains("restore")||!args["restore"].is_boolean()||!native_panel::host||!IsWindow(native_panel::host)||IsIconic(native_panel::host)){reply(done,context,fail("RESIZE_UNAVAILABLE"));return;}
  bool restore=args["restore"].get<bool>();if(restore&&!resized){reply(done,context,fail("RESIZE_UNAVAILABLE"));return;}
  if(!restore){original_placement={};original_placement.length=sizeof original_placement;if(resized||!GetWindowPlacement(native_panel::host,&original_placement)||!GetWindowRect(native_panel::host,&original_bounds)){reply(done,context,fail("RESIZE_UNAVAILABLE"));return;}resized=true;if(IsZoomed(native_panel::host))ShowWindow(native_panel::host,SW_RESTORE);}
  bool done_resize=false;
  if(restore){done_resize=SetWindowPlacement(native_panel::host,&original_placement)!=FALSE;if(done_resize)done_resize=IsZoomed(native_panel::host)==(original_placement.showCmd==SW_SHOWMAXIMIZED);if(done_resize)resized=false;}
  else{int change=original_placement.showCmd==SW_SHOWMAXIMIZED?-1:1;int width=original_bounds.right-original_bounds.left+change*80,height=original_bounds.bottom-original_bounds.top+change*60;done_resize=width>=400&&height>=300&&SetWindowPos(native_panel::host,nullptr,original_bounds.left,original_bounds.top,width,height,SWP_NOZORDER|SWP_NOACTIVATE);}
  if(!restore&&!done_resize&&resized&&SetWindowPlacement(native_panel::host,&original_placement))resized=false;
  reply(done,context,done_resize?ok():fail("RESIZE_UNAVAILABLE"));return;
 }
 if(name=="test.category.change") {
  if(args.size()!=2||!args.contains("id")||!args["id"].is_string()||!args.contains("open")||!args["open"].is_boolean()){reply(done,context,fail("INVALID_ARGUMENT"));return;}
  auto id=args["id"].get<std::string>();if(id!=""&&id!="boxy.sv2"&&id!="sv2.mcp"&&id!="test.category.fixture"){reply(done,context,fail("INVALID_ARGUMENT"));return;}
  bool accepted=SV2_TEST_CATEGORY_CHANGE(id.c_str(),args["open"].get<bool>());reply(done,context,accepted?ok():fail("NATIVE_CATEGORY_UNAVAILABLE"));return;
 }
 if(name=="test.category.state") {
  if(!args.empty()){reply(done,context,fail("INVALID_ARGUMENT"));return;}
  json categories=json::array(),panes=json::array();bool root_open=false;bool root_known=SV2_TEST_CATEGORY_OPEN_STATE("",&root_open);categories.push_back({{"id",""},{"visible",native_category_registry::categories[0].visible},{"openKnown",root_known},{"open",root_known&&root_open}});for(const auto &item:native_panel::categories()){bool is_open=false;bool known=SV2_TEST_CATEGORY_OPEN_STATE(item.id.c_str(),&is_open);categories.push_back({{"id",item.id},{"visible",item.visible},{"openKnown",known},{"open",known&&is_open}});}
  for(const auto &entry:native_panel::panes){const auto &p=entry.second;RECT rect{};GetWindowRect(p.window,&rect);RECT local=rect;MapWindowPoints(nullptr,native_panel::host,reinterpret_cast<POINT*>(&local),2);LOGFONTW font{};bool font_known=p.font&&GetObjectW(p.font,sizeof font,&font)==sizeof font;auto bytes=static_cast<unsigned char*>(entry.first);bool known=native_category_registry::readable(bytes,0x48);int x=0,y=0,w=0,h=0;if(known){std::memcpy(&x,bytes+0x38,4);std::memcpy(&y,bytes+0x3c,4);std::memcpy(&w,bytes+0x40,4);std::memcpy(&h,bytes+0x44,4);}auto chain=inspect_chain(entry.first);panes.push_back({{"id",p.id},{"visible",IsWindowVisible(p.window)!=FALSE},{"clientX",local.left},{"clientY",local.top},{"width",rect.right-rect.left},{"height",rect.bottom-rect.top},{"contentDpi",p.dpi},{"fontKnown",font_known},{"fontHeight",font.lfHeight},{"nativeBoundsKnown",known},{"nativeX",x},{"nativeY",y},{"nativeWidth",w},{"nativeHeight",h},{"rootKnown",chain.known},{"peerGeometry",peer_geometry(chain,native_panel::host)},{"rootWidth",chain.root_width},{"rootHeight",chain.root_height}});}
  RECT client{};bool client_known=native_panel::host&&GetClientRect(native_panel::host,&client);using Dpi=UINT(WINAPI*)(HWND);auto dpi_fn=reinterpret_cast<Dpi>(GetProcAddress(GetModuleHandleW(L"user32.dll"),"GetDpiForWindow"));UINT dpi=client_known&&dpi_fn?dpi_fn(native_panel::host):0;
  reply(done,context,ok({{"categories",categories},{"panes",panes},{"guards",guards()},{"hostClientKnown",client_known},{"hostClientWidth",client.right-client.left},{"hostClientHeight",client.bottom-client.top},{"hostDpi",dpi},{"nativePrepared",native_category_registry::prepared},{"editorReady",editor_online()},{"settingsEvents",settings_events},{"actionEvents",action_events}}));return;
 }
 if(name=="test.category.control"){
  if(args.size()!=1||!args.contains("kind")||!args["kind"].is_string()){reply(done,context,fail("INVALID_ARGUMENT"));return;}auto kind=args["kind"].get<std::string>();if(kind!="action"&&kind!="boolean"&&kind!="unchanged"){reply(done,context,fail("INVALID_ARGUMENT"));return;}
  if(kind=="unchanged"){
   if(!update_ui){reply(done,context,fail("CONTROL_UNAVAILABLE"));return;}for(auto &entry:native_panel::panes){auto &pane=entry.second;if(pane.id!="test.category.fixture"||!IsWindowVisible(pane.window)||pane.controls.empty())continue;auto key=entry.first;HWND window=pane.window,focus=GetFocus();int scroll=pane.scroll;std::map<int,HWND> controls;for(const auto &control:pane.controls){HWND child=GetDlgItem(window,control.first);if(!child){reply(done,context,fail("CONTROL_UNAVAILABLE"));return;}controls.emplace(control.first,child);}
    for(int i=0;i<5;i++){if(update_ui("test.category.fixture","{\"status\":\"ready\"}",&owner)!=LOADER_OK){reply(done,context,fail("UPDATE_FAILED"));return;}auto current=native_panel::panes.find(key);if(current==native_panel::panes.end()||current->second.window!=window||current->second.scroll!=scroll||GetFocus()!=focus||current->second.controls.size()!=controls.size()){reply(done,context,fail("UI_CHANGED"));return;}for(const auto &control:controls)if(!IsWindow(control.second)||GetDlgItem(window,control.first)!=control.second){reply(done,context,fail("UI_CHANGED"));return;}}
    reply(done,context,ok());return;}
   reply(done,context,fail("CONTROL_UNAVAILABLE"));return;
  }
  for(auto &entry:native_panel::panes){auto &pane=entry.second;if(pane.id!="test.category.fixture"||!IsWindowVisible(pane.window))continue;for(const auto &control:pane.controls)if(control.second.type==kind){auto button=GetDlgItem(pane.window,control.first);if(!button)break;SendMessageW(pane.window,WM_COMMAND,MAKEWPARAM(control.first,BN_CLICKED),reinterpret_cast<LPARAM>(button));reply(done,context,ok());return;}}
  reply(done,context,fail("CONTROL_UNAVAILABLE"));return;
 }
 reply(done,context,fail("UNSUPPORTED_HOST_CAPABILITY"));
}
static bool install(const loader_api_v1 &api){
 update_ui=api.update_plugin_ui;
 json methods=json::array({"test.category.state","test.category.change","test.category.pause","test.category.control","test.category.resize","test.category.close_host","test.category.monitor"});
 json empty={{"type","object"},{"properties",json::object()},{"additionalProperties",false}};
 json tools=json::array({{{"name","svext_test_category_monitor"},{"method","test.category.monitor"},{"description","Move the test host to a display or restore its placement."},{"inputSchema",{{"type","object"},{"properties",{{"hostPid",{{"type","integer"},{"minimum",1}}},{"target",{{"type","string"},{"enum",json::array({"96","144","restore"})}}}}},{"required",json::array({"hostPid","target"})},{"additionalProperties",false}}}},
  {{"name","svext_test_close_host"},{"method","test.category.close_host"},{"description","Request the test host to close normally."},{"inputSchema",{{"type","object"},{"properties",{{"hostPid",{{"type","integer"},{"minimum",1}}}}},{"required",json::array({"hostPid"})},{"additionalProperties",false}}}},
  {{"name","svext_test_category_state"},{"method","test.category.state"},{"description","Read test category state."},{"inputSchema",empty}},
  {{"name","svext_test_category_change"},{"method","test.category.change"},{"description","Open or close a test category."},{"inputSchema",{{"type","object"},{"properties",{{"id",{{"type","string"}}},{"open",{{"type","boolean"}}}}},{"required",json::array({"id","open"})},{"additionalProperties",false}}}},
  {{"name","svext_test_category_pause"},{"method","test.category.pause"},{"description","Pause the test host main thread."},{"inputSchema",empty}},
  {{"name","svext_test_category_control"},{"method","test.category.control"},{"description","Exercise a test panel control."},{"inputSchema",{{"type","object"},{"properties",{{"kind",{{"type","string"}}}}},{"required",json::array({"kind"})},{"additionalProperties",false}}}},
  {{"name","svext_test_category_resize"},{"method","test.category.resize"},{"description","Resize or restore the test host window."},{"inputSchema",{{"type","object"},{"properties",{{"restore",{{"type","boolean"}}}}},{"required",json::array({"restore"})},{"additionalProperties",false}}}}});
 auto descriptor=json{{"methods",methods},{"tools",tools}}.dump();if(api.register_capability_provider("test.category",dispatch,&owner,descriptor.c_str())!=LOADER_OK)return false;
 json fixture={{"id","test.category.fixture"},{"name","Test"},{"version","1"},{"status","ready"},{"sidebar",{{"title","Test"},{"rows",json::array({{{"type","button"},{"id","action"},{"text","Action"}}})}}},{"settings",json::array({{{"key","enabled"},{"title","Enabled"},{"type","boolean"},{"default",false}}})}};
 auto body=fixture.dump();return api.register_plugin_ui(body.c_str(),fixture_event,&owner)==LOADER_OK;
}
}
