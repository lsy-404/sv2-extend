#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shlobj.h>
#include <filesystem>
#include <fstream>
#include <cstring>
#include "../sdk/include/loader_api.h"
static std::filesystem::path panel_file;
static loader_plugin_ui_event panel_event=nullptr;
static int requests=0;
int register_panel(const char* descriptor,loader_plugin_ui_event event,void*){panel_event=event;std::ofstream out{panel_file};out<<descriptor;return out?0:-1;}
int update_panel(const char*,const char* state,void*){std::ofstream out{std::filesystem::path(panel_file.wstring()+L".update")};out<<state;return out?0:-1;}
void request(const char* method,const char*,loader_json_completion cb,void* ctx){
 const char* body=std::strcmp(method,"editor.capabilities")==0?"{\"ok\":true,\"result\":{\"editorReady\":true,\"providers\":[{\"capabilities\":{\"methods\":[\"editor.read\"],\"tools\":[{\"name\":\"read\",\"method\":\"editor.read\",\"description\":\"Read editor\",\"inputSchema\":{\"type\":\"object\"}}]}}]}}":"{\"ok\":true,\"result\":{\"value\":42}}";cb(body,strlen(body),ctx);
 if(std::strcmp(method,"editor.capabilities")==0){std::ofstream out{std::filesystem::path(panel_file.wstring()+L".count")};out<<++requests;}
}
int wmain(int argc,wchar_t**argv){
 if(argc!=3)return 64;panel_file=std::filesystem::path(std::wstring(argv[1])+L".panel");PWSTR folder=nullptr;SHGetKnownFolderPath(FOLDERID_LocalAppData,0,nullptr,&folder);if(folder)CoTaskMemFree(folder);wchar_t path[32768],core[32768];if(GetEnvironmentVariableW(L"OPEN_SV2_TEST_CORE",core,32768))LoadLibraryExW(core,nullptr,DONT_RESOLVE_DLL_REFERENCES);GetModuleFileNameW(nullptr,path,32768);auto dll=std::filesystem::path(path).parent_path()/L"McpPlugin.dll";HMODULE module=LoadLibraryW(dll.c_str());auto init=module?(loader_plugin_init_v1_fn)GetProcAddress(module,"loader_plugin_init_v1"):nullptr;static loader_api_v1 api{};api.host_pid=GetCurrentProcessId();api.abi_version=1;api.struct_size=sizeof api;api.capabilities=LOADER_CAP_PLUGIN_UI;api.register_plugin_ui=register_panel;api.update_plugin_ui=update_panel;api.request_capability_json=request;int result=init?init(&api):-999;
 {std::ofstream out{std::filesystem::path(argv[1])};out<<result;}if(result)return 65;auto deadline=GetTickCount64()+30000;bool refreshed=false;while(GetTickCount64()<deadline){MSG msg;while(PeekMessageW(&msg,nullptr,0,0,PM_REMOVE)){TranslateMessage(&msg);DispatchMessageW(&msg);}if(!refreshed&&panel_event&&std::filesystem::exists(std::filesystem::path(panel_file.wstring()+L".refresh"))){refreshed=true;panel_event("{\"kind\":\"action\",\"action\":\"refresh\"}",nullptr);}Sleep(10);}return 0;
}
