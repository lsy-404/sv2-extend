#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <fstream>
#include <filesystem>
#include <string>
#include "vendor/json.hpp"
#include "../../sdk/include/loader_api.h"
using json=nlohmann::json;
static json response;
static void complete(const char *body,size_t length,void*){response=json::parse(body,body+length);}
int wmain(int argc,wchar_t **argv){
 if(argc!=2)return 64;json checks=json::object();
 auto module=GetModuleHandleW(L"ExtendCore.dll");checks["corePresentBeforeHostMain"]=module!=nullptr;if(!module)return 1;
 auto getter=(const loader_api_v1*(*)())GetProcAddress(module,"ExtendGetAPI");auto api=getter?getter():nullptr;checks["apiReadyBeforeHostMain"]=api!=nullptr;if(!api)return 2;
 checks["sameHostPID"]=api->host_pid==int(GetCurrentProcessId());checks["abiV1AppendOnly"]=api->abi_version==1&&api->struct_size>=sizeof(loader_api_v1);
 checks["noFalseEditorNetworkDataCapabilities"]=(api->capabilities&~(LOADER_CAP_NETWORK_STATE|LOADER_CAP_PLUGIN_UI))==0&&(api->capabilities&LOADER_CAP_PLUGIN_UI)&&api->host_data_directory()==nullptr;
 api->request_capability_json("hello.greet","{}",complete,nullptr);checks["sameCrossPlatformPluginLoaded"]=response.value("ok",false)&&response["result"]["message"]=="Hello from the resident plugin";
 response=nullptr;api->request_capability_json("runtime.info","{}",complete,nullptr);checks["runtimePIDMatches"]=response["result"]["hostPid"]==GetCurrentProcessId();
 loader_network_state_v1 state{};state.abi_version=1;state.struct_size=sizeof state;checks["networkStateTruthful"]=api->get_host_network_state(&state)==0&&!state.whole_host_verified&&state.intercepted_paths==0&&(state.unverified_paths&LOADER_NET_WINSOCK);
 checks["unverifiedReceiptRefused"]=api->verify_host_network_quiescence(state.generation,&state)==LOADER_ERR_NETWORK_UNVERIFIED;
 checks["blockingUnsupportedExplicitly"]=api->set_host_network_blocked(1)==LOADER_ERR_NOT_SUPPORTED;
 checks["privateMethodNotPublic"]=api->register_capability_provider("bad.account",[](const char*,const char*,loader_json_completion,void*,void*){},nullptr,"{\"methods\":[\"account.switch\"]}")==LOADER_ERR_INVALID;
 bool passed=true;for(auto &value:checks)passed=passed&&value.get<bool>();
 std::ofstream report{std::filesystem::path(argv[1])};report<<json{{"passed",passed},{"checks",checks},{"hostPid",GetCurrentProcessId()},{"scope","Real Windows bootstrap and resident shared SDK fixture; not a Windows SV2 acceptance test."}}.dump(2);report.close();
 ULONGLONG deadline=GetTickCount64()+2000;while(GetTickCount64()<deadline){MSG message;while(PeekMessageW(&message,nullptr,0,0,PM_REMOVE)){TranslateMessage(&message);DispatchMessageW(&message);}Sleep(10);}return passed?0:3;
}
