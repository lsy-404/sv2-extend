#include "../loader/windows/startup_policy.h"
#include <cassert>
#include <thread>
int main(){
 auto root=std::filesystem::current_path()/L"test"/L".outputs"/(L"startup-policy-"+std::to_wstring(GetCurrentProcessId())+L"-"+std::to_wstring(GetTickCount64()));
 assert(std::filesystem::create_directories(root));assert(plugin_storage::install(root));startup_policy::Service service(GetCurrentThreadId());
 loader_startup_network_state_v1 state{LOADER_ABI_V1,sizeof state,0,0,0,0,0};
 assert(service.state(&state)==LOADER_OK&&state.policy_valid&&!state.requested_blocked&&!state.active_guards);
 assert(service.set("plugin.one","work",1)==LOADER_OK);assert(service.set("plugin.one","work",1)==LOADER_OK);
 assert(service.set("plugin.two","work",1)==LOADER_OK);assert(service.state(&state)==LOADER_OK&&state.generation==2&&state.active_guards==2&&state.requested_blocked&&!state.bootstrap_applied);
 assert(service.list("plugin.one")["result"]["guards"]==startup_policy::json::array({"work"}));
 assert(service.set("plugin.one","work",0)==LOADER_OK);assert(service.state(&state)==LOADER_OK&&state.active_guards==1&&state.requested_blocked);
 startup_policy::Service restored(GetCurrentThreadId());restored.mark_applied();assert(restored.state(&state)==LOADER_OK&&state.bootstrap_applied&&state.generation==3);
 int result=0;std::thread worker([&]{result=service.set("plugin.two","work",0);});worker.join();assert(result==LOADER_ERR_INVALID);
 assert(service.set("plugin.two","work",0)==LOADER_OK);assert(service.state(&state)==LOADER_OK&&!state.requested_blocked&&state.generation==4);
 for(const auto &body:{std::string("{}"),std::string("{\"schema\":\"1\",\"generation\":0,\"guards\":{}}"),std::string("{\"schema\":1,\"generation\":0,\"guards\":{\"plugin:guard\":false}}")}){
  assert(plugin_storage::write("opensv2.loader","startup-network-policy.json",reinterpret_cast<const uint8_t*>(body.data()),body.size())==LOADER_OK);
  assert(service.state(&state)==LOADER_ERR_POLICY_INVALID&&!state.policy_valid&&state.requested_blocked);assert(service.set("plugin","guard",0)==LOADER_ERR_POLICY_INVALID);
 }
}
