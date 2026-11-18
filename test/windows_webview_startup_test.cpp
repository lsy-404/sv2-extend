#include "../loader/windows/windows_webview_gate.h"
#include "../loader/windows/windows_webview_startup.h"
#include "../loader/windows/network_bootstrap.h"
#include <cassert>
using namespace windows_webview_startup;
int wmain(int argc,wchar_t **argv){
 if(argc!=2)return 2;assert(MH_Initialize()==MH_OK);assert(windows_socket_gate::install());assert(windows_child_gate::install());assert(windows_webview_gate::install(argv[1]));
 windows_network_bootstrap::Handoff handoff;assert(handoff.create());uint32_t pid=0;uint64_t creation=0;std::wstring image;assert(windows_network_bootstrap::process_identity(GetCurrentProcess(),pid,creation,image));
 assert(handoff.publish(GetCurrentProcess(),image,9,true,true,true));assert(handoff.seal());windows_network_bootstrap::Record record{};assert(windows_network_bootstrap::read(handoff.read_handle(),record));
 Proof proof;auto gate=windows_webview_gate::state();assert(gate.installed&&gate.blocked&&!gate.factoryBound);assert(!windows_webview_transport::state().installed);assert(!windows_webview_gate::covered(gate,false));
 {
  Proof::Scope scope(proof);assert(!proof.valid(9,true,true,true,false,false,false));proof.attest(9,false,true,true);assert(!proof.valid(9,true,true,true,false,false,false));proof.attest(record.generation,record.requested,record.applied,record.verified);
  auto valid=[&](){return proof.valid(9,true,true,true,false,false,false);};assert(valid()&&windows_webview_gate::covered(gate,valid()));
  assert(!proof.valid(10,true,true,true,false,false,false));assert(!proof.valid(9,true,true,true,true,false,false));assert(!proof.valid(9,true,true,true,false,true,false));assert(!proof.valid(9,true,true,true,false,false,true));
  auto changed=gate;changed.onlineFactorySeen=true;assert(!windows_webview_gate::covered(changed,valid()));changed=gate;changed.denied=1;assert(!windows_webview_gate::covered(changed,valid()));changed=gate;changed.pendingEnvironment=1;assert(!windows_webview_gate::covered(changed,valid()));changed=gate;changed.controllers=1;assert(!windows_webview_gate::covered(changed,valid()));changed=gate;changed.coverage=false;assert(!windows_webview_gate::covered(changed,valid()));
 }
 assert(!proof.valid(9,true,true,true,false,false,false));{Proof::Scope second(proof);proof.attest(9,true,true,true);assert(!proof.valid(9,true,true,true,false,false,false));}
 gate.factoryBound=true;assert(!windows_webview_gate::covered(gate,true));assert(windows_webview_gate::start_transport());assert(windows_webview_transport::state().installed);assert(windows_webview_gate::covered(gate,false));gate.controllers=1;assert(windows_webview_gate::covered(gate,false));
 Proof interrupted;try{Proof::Scope scope(interrupted);interrupted.attest(9,true,true,true);throw 1;}catch(int){}assert(!interrupted.valid(9,true,true,true,false,false,false));{Proof::Scope retry(interrupted);interrupted.attest(9,true,true,true);assert(!interrupted.valid(9,true,true,true,false,false,false));}return 0;
}
