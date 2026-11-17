#include "../loader/windows/windows_socket_gate.h"
#include <cassert>
static int WSAAPI fail_close(SOCKET){WSASetLastError(WSAEWOULDBLOCK);return SOCKET_ERROR;}
int main(){
 WSADATA data{};assert(WSAStartup(MAKEWORD(2,2),&data)==0);assert(MH_Initialize()==MH_OK);assert(windows_socket_gate::install());
 SOCKET listener=socket(AF_INET,SOCK_STREAM,IPPROTO_TCP);assert(listener!=INVALID_SOCKET);
 sockaddr_in addr{};addr.sin_family=AF_INET;addr.sin_addr.s_addr=htonl(INADDR_LOOPBACK);addr.sin_port=0;
 assert(bind(listener,reinterpret_cast<sockaddr*>(&addr),sizeof addr)==0);assert(listen(listener,1)==0);int size=sizeof addr;assert(getsockname(listener,reinterpret_cast<sockaddr*>(&addr),&size)==0);
 SOCKET client=socket(AF_INET,SOCK_STREAM,IPPROTO_TCP);assert(client!=INVALID_SOCKET);assert(connect(client,reinterpret_cast<sockaddr*>(&addr),size)==0);
 SOCKET accepted=accept(listener,nullptr,nullptr);assert(accepted!=INVALID_SOCKET);
 SOCKET client2=socket(AF_INET,SOCK_STREAM,IPPROTO_TCP);assert(client2!=INVALID_SOCKET);assert(connect(client2,reinterpret_cast<sockaddr*>(&addr),size)==0);
 SOCKET accepted2=WSAAccept(listener,nullptr,nullptr,nullptr,0);assert(accepted2!=INVALID_SOCKET);assert(closesocket(client2)==0);
 SOCKET udp=WSASocketW(AF_INET,SOCK_DGRAM,IPPROTO_UDP,nullptr,0,0);assert(udp!=INVALID_SOCKET);
 assert(windows_socket_gate::state().active==5);assert(windows_socket_gate::setblocked(true));assert(windows_socket_gate::state().active==0);
 assert(socket(AF_INET,SOCK_STREAM,IPPROTO_TCP)==INVALID_SOCKET&&WSAGetLastError()==WSAEACCES);
 SOCKET local=socket(AF_UNIX,SOCK_STREAM,0);if(local!=INVALID_SOCKET)assert(closesocket(local)==0);
 assert(windows_socket_gate::setblocked(false));SOCKET again=WSASocketA(AF_INET,SOCK_DGRAM,IPPROTO_UDP,nullptr,0,0);assert(again!=INVALID_SOCKET);
 auto real=windows_socket_gate::original_close;windows_socket_gate::original_close=fail_close;
 assert(!windows_socket_gate::setblocked(true));auto failed=windows_socket_gate::state();assert(failed.active==1&&failed.pending&&failed.close_failed==1);
 windows_socket_gate::original_close=real;assert(windows_socket_gate::setblocked(true));assert(windows_socket_gate::state().active==0&&windows_socket_gate::state().close_failed==0);
 WSACleanup();
}
