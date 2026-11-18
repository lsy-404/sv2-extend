#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <fstream>
#include <filesystem>
#include <string>
#include <cstring>
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
 const uint64_t fixtureCaps=LOADER_CAP_NETWORK_STATE|LOADER_CAP_PLUGIN_UI|LOADER_CAP_SCOPED_STORAGE;
 checks["fixtureCapabilities"]=(api->capabilities&fixtureCaps)==fixtureCaps&&(api->capabilities&~fixtureCaps)==0&&api->host_data_directory()==nullptr;
 const uint8_t stored[]={1,2,3};uint8_t restored[sizeof stored]{};size_t length=sizeof restored;
 checks["scopedStorageRoundTrip"]=api->plugin_storage_write_scoped("fixture.plugin","value",stored,sizeof stored)==LOADER_OK&&api->plugin_storage_read_scoped("fixture.plugin","value",restored,&length)==LOADER_OK&&length==sizeof stored&&!std::memcmp(stored,restored,length);
 checks["scopedStorageRejectsInvalidOwner"]=api->plugin_storage_write_scoped("../invalid","value",stored,sizeof stored)==LOADER_ERR_INVALID;
 api->request_capability_json("hello.greet","{}",complete,nullptr);checks["sameCrossPlatformPluginLoaded"]=response.value("ok",false)&&response["result"]["message"]=="Hello from the resident plugin";
 response=nullptr;api->request_capability_json("runtime.info","{}",complete,nullptr);checks["runtimePIDMatches"]=response["result"]["hostPid"]==GetCurrentProcessId();
 loader_network_state_v1 state{};state.abi_version=1;state.struct_size=sizeof state;checks["fixtureNetworkStateUnsupported"]=api->get_host_network_state(&state)==LOADER_ERR_NOT_SUPPORTED;
 checks["unverifiedReceiptRefused"]=api->verify_host_network_quiescence(state.generation,&state)==LOADER_ERR_NOT_SUPPORTED;
 checks["blockingWithoutControllerRefused"]=api->set_host_network_blocked(1)==LOADER_ERR_PROFILE_BUSY;checks["invalidNetworkPolicyRejected"]=api->set_host_network_blocked(2)==LOADER_ERR_INVALID;
 checks["privateMethodNotPublic"]=api->register_capability_provider("bad.account",[](const char*,const char*,loader_json_completion,void*,void*){},nullptr,"{\"methods\":[\"account.switch\"]}")==LOADER_ERR_INVALID;
 auto call=[&](const char *method,const char *arguments){response=nullptr;api->request_capability_json(method,arguments,complete,nullptr);return response;};
 auto package=[&](const char *id){auto snapshot=call("runtime.capabilities","{}");for(auto &entry:snapshot["result"]["packages"])if(entry["id"]==id)return entry;return json::object();};
 checks["loaderDeclaresCapabilities"]=call("runtime.capabilities","{}")["result"]["loader"]["version"]=="0.10.0";
 checks["grantedPackageToolCalls"]=call("example.greeter.svext_greeter_greet","{}")["result"]["message"]=="Hello from the resident plugin";
 checks["ungrantedPackageToolDenied"]=package("example.locked")["state"]=="active"&&call("example.locked.svext_locked_greet","{}")["error"]["code"]=="PERMISSION_DENIED";
 checks["packageStorageRoundTrip"]=call("example.greeter.svext_greeter_remember","{\"value\":\"kept\"}").value("ok",false)&&call("example.greeter.svext_greeter_recall","{}")["result"]["value"]=="kept";
 checks["missingCapabilityNotLoaded"]=package("example.editor-reader")["reason"]=="missing-capability:editor.read";
 auto first=call("example.counter.increment","{\"by\":2}"),second=call("example.counter.svext_counter_increment","{\"by\":2}");
 checks["packageCodeHandlesCalls"]=first.value("ok",false)&&second["result"]["count"]==first["result"]["count"].get<long long>()+2;
 checks["dependencyCodeRunsForDependent"]=call("example.greeter.svext_greeter_count","{}")["result"]["count"]==second["result"]["count"].get<long long>()+1&&call("example.locked.svext_locked_count","{}")["error"]["code"]=="PERMISSION_DENIED";
 checks["runawayCodeStopped"]=call("example.runaway.spin","{}")["error"]["code"]=="HANDLER_FAILED"&&call("example.runaway.hoard","{}")["error"]["code"]=="HANDLER_FAILED";
 auto reach=call("example.runaway.reach","{}")["result"];checks["packageCodeIsolated"]=reach["os"]=="nil"&&reach["io"]=="nil"&&reach["load"]=="nil"&&reach["require"]=="nil"&&reach["debug"]=="nil";
 auto wait=[&](const char *method,const json &arguments){response=nullptr;api->request_capability_json(method,arguments.dump().c_str(),complete,nullptr);ULONGLONG limit=GetTickCount64()+8000;while(response.is_null()&&GetTickCount64()<limit){MSG message;while(PeekMessageW(&message,nullptr,0,0,PM_REMOVE))DispatchMessageW(&message);Sleep(5);}return response;};
 auto server=wait("example.echo.start",json::object())["result"];auto url=server.value("url",std::string())+"/notes?x=1";json bearer{{"Authorization","Bearer "+server.value("token",std::string())}};
 auto echoed=wait("example.echo.fetch",{{"url",url},{"method","POST"},{"headers",bearer},{"body","kept"}})["result"];
 checks["packageServesAndRequestsHttp"]=echoed["status"]==200&&echoed["body"]=="POST /notes kept"&&echoed["headers"]["content-type"]=="text/plain; charset=utf-8";
 checks["packageServerRequiresToken"]=wait("example.echo.fetch",{{"url",url}})["result"]["status"]==401;
 checks["undeclaredHostRefused"]=wait("example.echo.fetch",{{"url","https://example.com/"}})["error"]["code"]=="HOST_NOT_DECLARED"&&wait("example.echo.fetch",{{"url","http://127.0.0.1.example.com/"}})["error"]["code"]=="INSECURE_URL";
 checks["closedPackageServerStops"]=wait("example.echo.stop",json::object()).value("ok",false)&&wait("example.echo.fetch",{{"url",url},{"headers",bearer}})["error"]["code"]=="HTTP_FAILED";
 auto plugins=std::filesystem::path(argv[1]).parent_path()/"smoke-profile"/"plugins";std::error_code moved;std::filesystem::rename(plugins/"example.locked",plugins.parent_path()/"example.locked.parked",moved);
 checks["removedPackageUnloads"]=!moved&&call("runtime.packages.reload","{}").value("ok",false)&&package("example.locked")["state"]=="removed"&&call("example.locked.svext_locked_greet","{}")["error"]["code"]=="METHOD_UNAVAILABLE"&&package("example.greeter")["state"]=="active";
 std::filesystem::rename(plugins.parent_path()/"example.locked.parked",plugins/"example.locked",moved);
 checks["restoredPackageLoads"]=!moved&&call("runtime.packages.reload","{}").value("ok",false)&&package("example.locked")["state"]=="active"&&call("example.locked.svext_locked_greet","{}")["error"]["code"]=="PERMISSION_DENIED";
 bool passed=true;for(auto &value:checks)passed=passed&&value.get<bool>();
 std::ofstream report{std::filesystem::path(argv[1])};report<<json{{"passed",passed},{"checks",checks},{"hostPid",GetCurrentProcessId()},{"scope","Real Windows bootstrap and resident shared SDK fixture; not a Windows SV2 acceptance test."}}.dump(2);report.close();
 ULONGLONG deadline=GetTickCount64()+2000;while(GetTickCount64()<deadline){MSG message;while(PeekMessageW(&message,nullptr,0,0,PM_REMOVE)){TranslateMessage(&message);DispatchMessageW(&message);}Sleep(10);}return passed?0:3;
}
