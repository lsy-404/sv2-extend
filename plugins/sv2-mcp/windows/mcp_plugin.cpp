#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <sddl.h>
#include <atomic>
#include <string>
#include <vector>
#include <filesystem>
#include <fstream>
#include "protocol.h"
namespace {
const loader_api_v1* loader=nullptr;
HANDLE server=nullptr;
std::atomic<bool> server_running{false};
HWND panel_dispatcher=nullptr;
HWND startup_dispatcher=nullptr;
bool panel_pending=false;
bool show_command=false;
std::string editor_status="Editor: checking";
std::vector<std::string> command_lines;
std::string utf8(const std::wstring& value){int size=WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,value.data(),int(value.size()),nullptr,0,nullptr,nullptr);if(size<=0)return {};std::string result(size,'\0');if(!WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,value.data(),int(value.size()),result.data(),size,nullptr,nullptr))return {};return result;}
windows_mcp::json panel_sidebar(){
 auto rows=windows_mcp::json::array({{{"type","label"},{"text",editor_status}}});
 if(show_command){rows.push_back({{"type","label"},{"text","MCP stdio command:"}});for(const auto& line:command_lines)rows.push_back({{"type","label"},{"text",line}});}
 rows.push_back({{"type","button"},{"id","refresh"},{"text","Refresh"}});
 return {{"title","MCP"},{"rows",rows}};
}
void panel_update(){auto body=windows_mcp::json{{"status",!windows_mcp::enabled("enabled")?(server_running?"Disabled (listener active)":"Disabled"):server_running?"Listening":"Unavailable"},{"sidebar",panel_sidebar()}}.dump();loader->update_plugin_ui("sv2.mcp",body.c_str(),nullptr);}
void panel_complete(const char* body,size_t length,void*){auto response=new std::string;if(body&&length<=1048576)response->assign(body,length);if(!PostMessageW(panel_dispatcher,WM_APP+1,0,reinterpret_cast<LPARAM>(response)))delete response;}
void panel_refresh(){if(panel_pending)return;panel_pending=true;editor_status="Editor: checking";panel_update();loader->request_capability_json("editor.capabilities","{}",panel_complete,nullptr);}
void panel_event(const char* body,void*){auto value=body?windows_mcp::json::parse(body,nullptr,false):windows_mcp::json();if(!value.is_object())return;if(value.value("kind","")=="settings"&&value.contains("settings")&&value["settings"].is_object()){windows_mcp::set_settings(value["settings"]);show_command=value["settings"].value("showEndpoint",false);panel_update();}else if(value.value("kind","")=="action"&&value.value("action","")=="refresh")panel_refresh();}
LRESULT CALLBACK panel_proc(HWND window,UINT message,WPARAM w,LPARAM l){if(message==WM_APP+1){std::unique_ptr<std::string> response(reinterpret_cast<std::string*>(l));panel_pending=false;auto value=windows_mcp::json::parse(*response,nullptr,false);editor_status="Editor: unavailable";if(value.is_object()&&value.value("ok",false)&&value.contains("result")&&value["result"].is_object()){auto& result=value["result"];if(result.contains("editorReady")&&result["editorReady"].is_boolean())editor_status=result["editorReady"].get<bool>()?"Editor: ready":"Editor: not ready";}panel_update();return 0;}return DefWindowProcW(window,message,w,l);}
void install_panel(){
 if(!(loader->capabilities&LOADER_CAP_PLUGIN_UI)||loader->struct_size<offsetof(loader_api_v1,update_plugin_ui)+sizeof(loader->update_plugin_ui)||!loader->register_plugin_ui||!loader->update_plugin_ui)return;
 wchar_t root[32768],core[32768];DWORD n=GetEnvironmentVariableW(L"OPEN_SV2_LOADER_DATA_DIR",root,32768);HMODULE module=GetModuleHandleW(L"ExtendCore.dll");DWORD m=module?GetModuleFileNameW(module,core,32768):0;
 if(n&&n<32768&&m&&m<32768){auto executable=std::filesystem::path(core).parent_path()/L"Sv2Mcp.exe";auto endpoint=std::filesystem::path(root)/L"outputs"/L"mcp-endpoint.json";std::wstring command=L"\""+executable.wstring()+L"\" --endpoint \""+endpoint.wstring()+L"\"";for(size_t i=0;i<command.size()&&command_lines.size()<28;i+=38)command_lines.push_back(utf8(command.substr(i,38)));}else command_lines.push_back("Command unavailable");
 WNDCLASSW cls{};cls.lpfnWndProc=panel_proc;cls.hInstance=GetModuleHandleW(nullptr);cls.lpszClassName=L"SV2ExtendMcpPanelDispatch";if(!RegisterClassW(&cls)&&GetLastError()!=ERROR_CLASS_ALREADY_EXISTS)return;panel_dispatcher=CreateWindowExW(0,cls.lpszClassName,L"",0,0,0,0,0,HWND_MESSAGE,nullptr,cls.hInstance,nullptr);if(!panel_dispatcher)return;
 auto fields=windows_mcp::json::array({{{"key","enabled"},{"title","启用 MCP"},{"type","boolean"},{"default",true}},{{"key","showEndpoint"},{"title","侧栏显示连接命令"},{"type","boolean"},{"default",false}}});
 for(const auto *method:{"selection.read","groups.create","groups.remove","notes.set_pitch","notes.add","notes.remove","pitch.read_range","pitch.replace_range"})fields.push_back({{"key",std::string("method.")+method},{"title",method},{"type","boolean"},{"default",true}});
 auto descriptor=windows_mcp::json{{"id","sv2.mcp"},{"name","MCP"},{"version","0.3.0"},{"status",!windows_mcp::enabled("enabled")?(server_running?"Disabled (listener active)":"Disabled"):server_running?"Listening":"Unavailable"},{"sidebar",panel_sidebar()},{"settings",fields}}.dump();if(loader->register_plugin_ui(descriptor.c_str(),panel_event,nullptr)!=LOADER_OK){DestroyWindow(panel_dispatcher);panel_dispatcher=nullptr;return;}panel_refresh();
}
struct Pending {
 std::atomic<int> refs{2};HANDLE event=CreateEventW(nullptr,TRUE,FALSE,nullptr);std::string response;
 ~Pending(){if(event)CloseHandle(event);}
 void release(){if(--refs==0)delete this;}
};
void complete(const char* body,size_t length,void* context){auto p=static_cast<Pending*>(context);if(body)p->response.assign(body,length);SetEvent(p->event);p->release();}
enum class ClientCheck { denied, accepted, fatal };
ClientCheck user_matches(HANDLE pipe){
 HANDLE self=nullptr,client=nullptr;if(!OpenProcessToken(GetCurrentProcess(),TOKEN_QUERY,&self))return ClientCheck::denied;
 bool same=false;if(ImpersonateNamedPipeClient(pipe)){
  if(OpenThreadToken(GetCurrentThread(),TOKEN_QUERY,TRUE,&client)){SECURITY_IMPERSONATION_LEVEL level=SecurityAnonymous;DWORD levelSize=0;bool limited=GetTokenInformation(client,TokenImpersonationLevel,&level,sizeof level,&levelSize)&&level==SecurityIdentification;DWORD a=0,b=0;GetTokenInformation(client,TokenUser,nullptr,0,&a);GetTokenInformation(self,TokenUser,nullptr,0,&b);if(a&&b){std::vector<unsigned char> x(a),y(b);if(GetTokenInformation(client,TokenUser,x.data(),a,&a)&&GetTokenInformation(self,TokenUser,y.data(),b,&b))same=limited&&EqualSid(reinterpret_cast<TOKEN_USER*>(x.data())->User.Sid,reinterpret_cast<TOKEN_USER*>(y.data())->User.Sid);}CloseHandle(client);}
  if(!RevertToSelf()){CloseHandle(self);return ClientCheck::fatal;}
 }
 CloseHandle(self);return same?ClientCheck::accepted:ClientCheck::denied;
}
bool write_all(HANDLE pipe,const std::string& value){size_t offset=0;while(offset<value.size()){DWORD written=0;if(!WriteFile(pipe,value.data()+offset,DWORD(value.size()-offset),&written,nullptr)||!written)return false;offset+=written;}return true;}
bool publish(const loader_api_v1* api){
 wchar_t root[32768],name[512];DWORD rn=GetEnvironmentVariableW(L"OPEN_SV2_LOADER_DATA_DIR",root,32768),nn=GetEnvironmentVariableW(L"OPEN_SV2_MCP_PIPE_NAME",name,512);if(!rn||rn>=32768||!nn||nn>=512)return false;
 std::wstring wide(name);if(wide.rfind(L"\\\\.\\pipe\\SV2Extend.MCP.",0)!=0)return false;int bytes=WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,name,-1,nullptr,0,nullptr,nullptr);if(bytes<2)return false;std::string encoded(bytes,'\0');if(!WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,name,-1,encoded.data(),bytes,nullptr,nullptr))return false;encoded.pop_back();
 auto directory=std::filesystem::path(root)/L"outputs";std::error_code ec;std::filesystem::create_directories(directory,ec);if(ec)return false;auto target=directory/L"mcp-endpoint.json";auto temp=directory/(L".mcp-endpoint-"+std::to_wstring(GetCurrentProcessId())+L".tmp");
 windows_mcp::json value={{"schema",1},{"transport","named-pipe"},{"pipeName",encoded},{"hostPid",api->host_pid},{"protocolVersions",windows_mcp::json::array({"2025-11-25","2025-06-18","2025-03-26"})}};
 HANDLE token=nullptr;if(!OpenProcessToken(GetCurrentProcess(),TOKEN_QUERY,&token))return false;DWORD a=0,b=0;GetTokenInformation(token,TokenUser,nullptr,0,&a);GetTokenInformation(token,TokenAppContainerSid,nullptr,0,&b);std::vector<unsigned char> u(a),c(b);bool ok=a&&b&&GetTokenInformation(token,TokenUser,u.data(),a,&a)&&GetTokenInformation(token,TokenAppContainerSid,c.data(),b,&b);CloseHandle(token);if(!ok)return false;LPWSTR user=nullptr,app=nullptr;ok=ConvertSidToStringSidW(reinterpret_cast<TOKEN_USER*>(u.data())->User.Sid,&user)&&ConvertSidToStringSidW(reinterpret_cast<TOKEN_APPCONTAINER_INFORMATION*>(c.data())->TokenAppContainer,&app);if(!ok){if(user)LocalFree(user);if(app)LocalFree(app);return false;}std::wstring acl=L"D:P(A;;GA;;;"+std::wstring(user)+L")(A;;GA;;;"+std::wstring(app)+L")(A;;GA;;;SY)";LocalFree(user);LocalFree(app);PSECURITY_DESCRIPTOR sd=nullptr;if(!ConvertStringSecurityDescriptorToSecurityDescriptorW(acl.c_str(),SDDL_REVISION_1,&sd,nullptr))return false;SECURITY_ATTRIBUTES sa{sizeof sa,sd,FALSE};HANDLE file=CreateFileW(temp.c_str(),GENERIC_WRITE,0,&sa,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr);LocalFree(sd);if(file==INVALID_HANDLE_VALUE)return false;auto content=value.dump();DWORD written=0;ok=WriteFile(file,content.data(),DWORD(content.size()),&written,nullptr)&&written==content.size()&&FlushFileBuffers(file);CloseHandle(file);if(!ok){DeleteFileW(temp.c_str());return false;}
 return MoveFileExW(temp.c_str(),target.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)!=FALSE;
}
bool (*publish_endpoint)(const loader_api_v1*)=publish;
DWORD WINAPI run(void* context){
 HANDLE ready=static_cast<HANDLE>(context);SetEvent(ready);CloseHandle(ready);
 while(server_running){BOOL connected=ConnectNamedPipe(server,nullptr);if(!connected&&GetLastError()!=ERROR_PIPE_CONNECTED){if(!server_running)break;Sleep(100);continue;}
  std::string line;char bytes[4096];DWORD count=0;bool active=true,authenticated=false;
  while(active&&ReadFile(server,bytes,sizeof bytes,&count,nullptr)&&count){if(!authenticated){auto check=user_matches(server);if(check==ClientCheck::fatal){server_running=false;return 1;}authenticated=check==ClientCheck::accepted;if(!authenticated)break;}for(DWORD i=0;i<count&&active;i++){
   if(bytes[i]!='\n'){line+=bytes[i];if(line.size()>1048576)active=false;continue;}
   auto pending=new Pending;if(!pending->event){pending->release();pending->release();active=false;break;}
   windows_mcp::handle(loader,line.data(),line.size(),complete,pending);line.clear();
   if(WaitForSingleObject(pending->event,10000)!=WAIT_OBJECT_0){pending->release();active=false;break;}
   auto response=std::move(pending->response);pending->release();if(!response.empty()&&!write_all(server,response+"\n"))active=false;
  }}
  DisconnectNamedPipe(server);
 }
 return 0;
}
void remove_endpoint(){wchar_t root[32768];DWORD n=GetEnvironmentVariableW(L"OPEN_SV2_LOADER_DATA_DIR",root,32768);if(n&&n<32768){auto path=std::filesystem::path(root)/L"outputs"/L"mcp-endpoint.json";DeleteFileW(path.c_str());}}
void startup_error(const char *message){if(loader&&loader->struct_size>=offsetof(loader_api_v1,log)+sizeof(loader->log)&&loader->log)loader->log(message);}
LRESULT CALLBACK startup_proc(HWND window,UINT message,WPARAM w,LPARAM l){
 if(message==WM_APP+2){
  HANDLE ready=CreateEventW(nullptr,TRUE,FALSE,nullptr),worker_ready=nullptr;
  if(ready&&!DuplicateHandle(GetCurrentProcess(),ready,GetCurrentProcess(),&worker_ready,0,FALSE,DUPLICATE_SAME_ACCESS)){CloseHandle(ready);ready=nullptr;}
  server_running=ready!=nullptr;HANDLE thread=ready?CreateThread(nullptr,0,run,worker_ready,0,nullptr):nullptr;
  if(!thread){server_running=false;if(worker_ready)CloseHandle(worker_ready);startup_error("MCP worker start failed");}
  else {bool started=WaitForSingleObject(ready,1000)==WAIT_OBJECT_0&&WaitForSingleObject(thread,0)==WAIT_TIMEOUT;if(!started||!publish_endpoint(loader)){startup_error(started?"MCP endpoint publication failed":"MCP worker readiness failed");remove_endpoint();server_running=false;CancelSynchronousIo(thread);WaitForSingleObject(thread,1000);}CloseHandle(thread);}
  if(ready)CloseHandle(ready);
  if(panel_dispatcher)panel_update();startup_dispatcher=nullptr;DestroyWindow(window);return 0;
 }
 return DefWindowProcW(window,message,w,l);
}
bool defer_start(){
 WNDCLASSW cls{};cls.lpfnWndProc=startup_proc;cls.hInstance=GetModuleHandleW(nullptr);cls.lpszClassName=L"SV2ExtendMcpStartupDispatch";
 if(!RegisterClassW(&cls)&&GetLastError()!=ERROR_CLASS_ALREADY_EXISTS)return false;
 startup_dispatcher=CreateWindowExW(0,cls.lpszClassName,L"",0,0,0,0,0,HWND_MESSAGE,nullptr,cls.hInstance,nullptr);
 if(!startup_dispatcher)return false;
 if(!PostMessageW(startup_dispatcher,WM_APP+2,0,0)){DestroyWindow(startup_dispatcher);startup_dispatcher=nullptr;return false;}
 return true;
}
}
extern "C" __declspec(dllexport) int loader_plugin_init_v1(const loader_api_v1* api){
 if(!api||api->abi_version!=LOADER_ABI_V1||!api->request_capability_json)return LOADER_ERR_INVALID;
 wchar_t value[64];DWORD n=GetEnvironmentVariableW(L"OPEN_SV2_MCP_PIPE_HANDLE",value,64);if(!n||n>=64)return LOADER_ERR_INVALID;wchar_t* end=nullptr;auto raw=_wcstoui64(value,&end,10);if(!raw||!end||*end)return LOADER_ERR_INVALID;
 HANDLE candidate=reinterpret_cast<HANDLE>(static_cast<uintptr_t>(raw));DWORD flags=0;if(GetFileType(candidate)!=FILE_TYPE_PIPE||!GetNamedPipeInfo(candidate,&flags,nullptr,nullptr,nullptr)||!(flags&PIPE_SERVER_END))return LOADER_ERR_INVALID;
 loader=api;server=candidate;remove_endpoint();if(!defer_start()){loader=nullptr;server=nullptr;return LOADER_ERR_INVALID;}install_panel();return LOADER_OK;
}
