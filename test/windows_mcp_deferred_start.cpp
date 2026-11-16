#include <windows.h>
#include <cassert>
#include <string>
static int starts=0;
static HANDLE tracked_thread(LPSECURITY_ATTRIBUTES a,SIZE_T b,LPTHREAD_START_ROUTINE c,LPVOID d,DWORD e,LPDWORD f){++starts;return CreateThread(a,b,c,d,e,f);}
#define CreateThread tracked_thread
#include "../plugins/sv2-mcp/windows/mcp_plugin.cpp"
#undef CreateThread
static std::filesystem::path endpoint_file;
static bool fixture_publish(const loader_api_v1*){assert(starts==1&&server_running);std::ofstream out(endpoint_file);out<<"ready";return bool(out);}
static void request(const char*,const char*,loader_json_completion,void*){}
int main(){
 std::wstring name=L"\\\\.\\pipe\\SV2Extend.MCP.deferred."+std::to_wstring(GetCurrentProcessId());
 HANDLE pipe=CreateNamedPipeW(name.c_str(),PIPE_ACCESS_DUPLEX,PIPE_TYPE_BYTE|PIPE_WAIT,1,4096,4096,0,nullptr);assert(pipe!=INVALID_HANDLE_VALUE);
 auto handle=std::to_wstring(reinterpret_cast<uintptr_t>(pipe));SetEnvironmentVariableW(L"OPEN_SV2_MCP_PIPE_HANDLE",handle.c_str());
 SetEnvironmentVariableW(L"OPEN_SV2_LOADER_DATA_DIR",nullptr);SetEnvironmentVariableW(L"OPEN_SV2_MCP_PIPE_NAME",name.c_str());
 wchar_t temp[MAX_PATH];assert(GetTempPathW(MAX_PATH,temp));endpoint_file=std::filesystem::path(temp)/(L"sv2-mcp-deferred-"+std::to_wstring(GetCurrentProcessId())+L".json");DeleteFileW(endpoint_file.c_str());publish_endpoint=fixture_publish;
 loader_api_v1 api{};api.abi_version=LOADER_ABI_V1;api.request_capability_json=request;
 assert(loader_plugin_init_v1(&api)==LOADER_OK);assert(starts==0&&!server_running&&startup_dispatcher&&!std::filesystem::exists(endpoint_file));
 MSG message{};assert(PeekMessageW(&message,nullptr,0,0,PM_REMOVE));DispatchMessageW(&message);
 assert(starts==1&&server_running&&!startup_dispatcher&&std::filesystem::exists(endpoint_file));server_running=false;HANDLE client=CreateFileW(name.c_str(),GENERIC_READ|GENERIC_WRITE,0,nullptr,OPEN_EXISTING,0,nullptr);if(client!=INVALID_HANDLE_VALUE)CloseHandle(client);DeleteFileW(endpoint_file.c_str());CloseHandle(pipe);
}
