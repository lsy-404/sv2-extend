#define SV2_NETWORK_PROBE_TEST
#include "windows_network_live_probe.h"
#include <cassert>
int main(){
 WSADATA w{};assert(WSAStartup(MAKEWORD(2,2),&w)==0);assert(MH_Initialize()==MH_OK);assert(windows_socket_gate::install());
 SOCKET listener=socket(AF_INET,SOCK_STREAM,IPPROTO_TCP);assert(listener!=INVALID_SOCKET);sockaddr_in addr{};addr.sin_family=AF_INET;addr.sin_addr.s_addr=htonl(INADDR_LOOPBACK);assert(bind(listener,reinterpret_cast<sockaddr*>(&addr),sizeof addr)==0);assert(listen(listener,1)==0);int n=sizeof addr;assert(getsockname(listener,reinterpret_cast<sockaddr*>(&addr),&n)==0);
 windows_network_probe::Probe probe;assert(probe.start_test(addr));for(int i=0;i<400&&probe.status().pending;i++)Sleep(10);auto open=probe.status();assert(!open.pending&&open.connected&&open.owned);
 SOCKET accepted=accept(listener,nullptr,nullptr);assert(accepted!=INVALID_SOCKET);assert(windows_socket_gate::setblocked(true));auto blocked=probe.check_closed();assert(blocked.socketChecked&&blocked.socketClosed&&blocked.ownershipReleased&&blocked.newSocketDenied);probe.close();
 assert(windows_socket_gate::setblocked(false));SOCKET second=socket(AF_INET,SOCK_STREAM,IPPROTO_TCP);assert(second!=INVALID_SOCKET);assert(closesocket(second)==0);WSACleanup();return 0;
}
