#include "../loader/windows/network_control.h"
int main(){windows_network::Controller c(L"C:\\not-installed\\synthv-studio.exe");auto s=c.state();return s.pending||s.policy_applied||s.child_scope||s.host_connections_drained;}
