#include "../loader/windows/network_control.h"
#include <cassert>
int wmain(int argc,wchar_t **argv){if(argc!=2)return 2;windows_network::Controller controller(argv[1]);assert(controller.request(true));for(int i=0;i<100;i++){auto state=controller.state();if(!state.pending)return state.policy_applied&&state.applied_blocked&&!state.error?0:3;Sleep(50);}return 4;}
