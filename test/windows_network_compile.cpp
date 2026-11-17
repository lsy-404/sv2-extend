#include "../loader/windows/network_control.h"
int main(){windows_network::Controller c(L"C:\\not-installed\\synthv-studio.exe");if(c.adopt(true))return 1;auto s=c.state();return s.pending||s.policy_applied||s.child_scope||s.host_connections_drained||s.error==0;}
