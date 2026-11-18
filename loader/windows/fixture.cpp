#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <fstream>
#include <filesystem>
#include <string>
#include <cstring>
#include "vendor/json.hpp"
#include "package_crypto.h"
#include "../../sdk/include/loader_api.h"
using json=nlohmann::json;
static std::string base64(const std::string &input){
 static const char *table="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";std::string out;size_t index=0;
 while(index+3<=input.size()){unsigned long v=(unsigned char)input[index]<<16|(unsigned char)input[index+1]<<8|(unsigned char)input[index+2];out+=table[v>>18&63];out+=table[v>>12&63];out+=table[v>>6&63];out+=table[v&63];index+=3;}
 if(size_t left=input.size()-index){unsigned long v=(unsigned char)input[index]<<16|(left==2?(unsigned char)input[index+1]<<8:0);out+=table[v>>18&63];out+=table[v>>12&63];out+=left==2?table[v>>6&63]:'=';out+='=';}
 return out;
}
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
 checks["cleanProfileCapability"]=false;
 auto capabilities=call("runtime.capabilities","{}");
 for(const auto &capability:capabilities["result"]["loader"]["capabilities"])
  if(capability["id"]=="profile.clean")checks["cleanProfileCapability"]=capability["supported"]==true&&capability["grantable"]==false&&capability["methods"].empty();
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
 checks["plainHttpRefusedOffLoopback"]=wait("example.echo.fetch",{{"url","http://127.0.0.1.example.com/"}})["error"]["code"]=="INSECURE_URL";
 checks["closedPackageServerStops"]=wait("example.echo.stop",json::object()).value("ok",false)&&wait("example.echo.fetch",{{"url",url},{"headers",bearer}})["error"]["code"]=="HTTP_FAILED";
 auto plugins=std::filesystem::path(argv[1]).parent_path()/"smoke-profile"/"plugins";std::error_code moved;std::filesystem::rename(plugins/"example.locked",plugins.parent_path()/"example.locked.parked",moved);
 checks["removedPackageUnloads"]=!moved&&call("runtime.packages.reload","{}").value("ok",false)&&package("example.locked")["state"]=="removed"&&call("example.locked.svext_locked_greet","{}")["error"]["code"]=="METHOD_UNAVAILABLE"&&package("example.greeter")["state"]=="active";
 std::filesystem::rename(plugins.parent_path()/"example.locked.parked",plugins/"example.locked",moved);
 checks["restoredPackageLoads"]=!moved&&call("runtime.packages.reload","{}").value("ok",false)&&package("example.locked")["state"]=="active"&&call("example.locked.svext_locked_greet","{}")["error"]["code"]=="PERMISSION_DENIED";
 std::string key(32,'\0'),iv(12,'\0');BCryptGenRandom(nullptr,(PUCHAR)&key[0],32,BCRYPT_USE_SYSTEM_PREFERRED_RNG);BCryptGenRandom(nullptr,(PUCHAR)&iv[0],12,BCRYPT_USE_SYSTEM_PREFERRED_RNG);
 std::string plain="local extend=...\nreturn function(input) return { value=(input or 0)*7, proof=\"sealed\" } end\n",cipher,tag,sealError;
 bool sealed=package_crypto::encrypt("aes-256-gcm",key,iv,plain,"",cipher,tag,sealError);
 auto folder=plugins/"example.sealed";std::filesystem::create_directories(folder);
 std::ofstream(folder/"plugin.json")<<R"({"schema":1,"id":"example.sealed","name":"Sealed","version":"1.0.0","requires":{"capabilities":{"runtime.lua":"*","runtime.protected":"*"}},"main":"main.lua","contributes":{"methods":["open"]}})";
 std::ofstream(folder/"main.lua")<<"local extend=...\nlocal handlers={}\nfunction handlers.open(request,reply)\n local ok,result=pcall(function()\n  local blob=extend.resource(\"secret.bin\")\n  local source=extend.decrypt{algorithm=\"aes-256-gcm\",key=extend.b64decode(request.key),iv=extend.b64decode(request.iv),tag=extend.b64decode(request.tag),data=blob}\n  return extend.load(source,\"secret\")(request.input)\n end)\n if ok then reply(result) else reply(nil,{code=\"SEALED_FAILED\",message=tostring(result)}) end\nend\nreturn handlers\n";
 std::ofstream(folder/"secret.bin",std::ios::binary).write(cipher.data(),(std::streamsize)cipher.size());
 bool reloaded=sealed&&call("runtime.packages.reload","{}").value("ok",false)&&package("example.sealed")["state"]=="active";
 json good{{"key",base64(key)},{"iv",base64(iv)},{"tag",base64(tag)},{"input",6}};
 auto opened=call("example.sealed.open",good.dump().c_str());
 checks["sealedLogicDecryptsAndRuns"]=reloaded&&opened.value("ok",false)&&opened["result"]["value"]==42&&opened["result"]["proof"]=="sealed";
 std::string tampered=tag;tampered[0]^=0x40;json bad{{"key",base64(key)},{"iv",base64(iv)},{"tag",base64(tampered)},{"input",6}};
 checks["sealedLogicRejectsTampering"]=reloaded&&call("example.sealed.open",bad.dump().c_str())["error"]["code"]=="SEALED_FAILED";
 bool passed=true;for(auto &value:checks)passed=passed&&value.get<bool>();
 std::ofstream report{std::filesystem::path(argv[1])};report<<json{{"passed",passed},{"checks",checks},{"hostPid",GetCurrentProcessId()},{"scope","Real Windows bootstrap and resident shared SDK fixture; not a Windows SV2 acceptance test."}}.dump(2);report.close();
 ULONGLONG deadline=GetTickCount64()+2000;while(GetTickCount64()<deadline){MSG message;while(PeekMessageW(&message,nullptr,0,0,PM_REMOVE)){TranslateMessage(&message);DispatchMessageW(&message);}Sleep(10);}return passed?0:3;
}
