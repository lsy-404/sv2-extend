#pragma once
#include <winsock2.h>
#include <ws2tcpip.h>
#include <atomic>
#include <mutex>
#include <thread>
#include "../loader/windows/windows_socket_gate.h"
namespace windows_network_probe {
struct State {bool pending=false,connected=false,owned=false,ownershipReleased=false,socketChecked=false,socketClosed=false,newSocketChecked=false,newSocketDenied=false;int connectError=0,socketError=0,newSocketError=0;};
class Probe {
 std::mutex mutex_;std::thread worker_;SOCKET socket_=INVALID_SOCKET;State state_{};sockaddr_in local_{};bool local_valid_=false;
 void run(sockaddr_in target){
  SOCKET s=socket(AF_INET,SOCK_STREAM,IPPROTO_TCP);int error=0;bool connected=false;sockaddr_in local{};int len=sizeof local;
  if(s==INVALID_SOCKET)error=WSAGetLastError();
  else {u_long enabled=1;if(ioctlsocket(s,FIONBIO,&enabled)==SOCKET_ERROR)error=WSAGetLastError();else{
   int rc=connect(s,reinterpret_cast<sockaddr*>(&target),sizeof target);if(rc==0)connected=true;else{error=WSAGetLastError();if(error==WSAEWOULDBLOCK||error==WSAEINPROGRESS){fd_set write,except;FD_ZERO(&write);FD_ZERO(&except);FD_SET(s,&write);FD_SET(s,&except);timeval wait{3,0};rc=select(0,nullptr,&write,&except,&wait);if(rc>0){int value=0;int size=sizeof value;if(getsockopt(s,SOL_SOCKET,SO_ERROR,reinterpret_cast<char*>(&value),&size)==0){error=value;connected=value==0;}else error=WSAGetLastError();}else error=rc==0?WSAETIMEDOUT:WSAGetLastError();}}
  }}
  bool have_local=s!=INVALID_SOCKET&&getsockname(s,reinterpret_cast<sockaddr*>(&local),&len)==0;
  std::lock_guard<std::mutex> lock(mutex_);socket_=s;local_=local;local_valid_=have_local;state_.pending=false;state_.connected=connected;state_.connectError=error;state_.owned=s!=INVALID_SOCKET;
 }
 bool start_at(sockaddr_in target){
  {std::lock_guard<std::mutex> lock(mutex_);if(state_.pending||state_.owned)return false;}
  if(worker_.joinable())worker_.join();
  {std::lock_guard<std::mutex> lock(mutex_);state_={};state_.pending=true;socket_=INVALID_SOCKET;local_valid_=false;}
  try{worker_=std::thread([this,target]{run(target);});}catch(...){std::lock_guard<std::mutex> lock(mutex_);state_.pending=false;state_.connectError=WSAENOBUFS;return false;}return true;
 }
public:
 ~Probe(){close();}
 bool start(){sockaddr_in target{};target.sin_family=AF_INET;target.sin_port=htons(443);target.sin_addr.s_addr=htonl(0x01010101u);return start_at(target);}
 bool open(){return start();}
#ifdef SV2_NETWORK_PROBE_TEST
 bool start_test(sockaddr_in target){return start_at(target);}
#endif
 State status(){
  std::lock_guard<std::mutex> lock(mutex_);
  if(state_.owned&&!state_.pending&&windows_socket_gate::state().blocked){
   SOCKET old=socket_;state_.owned=false;state_.ownershipReleased=true;socket_=INVALID_SOCKET;
   if(old!=INVALID_SOCKET){int type=0,length=sizeof type;int rc=getsockopt(old,SOL_SOCKET,SO_TYPE,reinterpret_cast<char*>(&type),&length);state_.socketChecked=true;state_.socketError=rc==SOCKET_ERROR?WSAGetLastError():0;state_.socketClosed=rc==SOCKET_ERROR&&state_.socketError==WSAENOTSOCK;}
  }return state_;
 }
 State check_closed(){
  if(worker_.joinable())worker_.join();status();std::lock_guard<std::mutex> lock(mutex_);
  if(!windows_socket_gate::state().blocked)return state_;
  SOCKET fresh=socket(AF_INET,SOCK_STREAM,IPPROTO_TCP);state_.newSocketChecked=true;state_.newSocketError=fresh==INVALID_SOCKET?WSAGetLastError():0;state_.newSocketDenied=fresh==INVALID_SOCKET&&state_.newSocketError==WSAEACCES;
  if(fresh!=INVALID_SOCKET)closesocket(fresh);return state_;
 }
 void close(){
  if(worker_.joinable())worker_.join();std::lock_guard<std::mutex> lock(mutex_);if(!state_.owned||socket_==INVALID_SOCKET)return;
  std::lock_guard<std::mutex> gate(windows_socket_gate::mutex);auto it=windows_socket_gate::sockets.find(socket_);
  if(it!=windows_socket_gate::sockets.end()&&!windows_socket_gate::current.blocked&&local_valid_){sockaddr_in local{};int len=sizeof local;if(getsockname(socket_,reinterpret_cast<sockaddr*>(&local),&len)==0&&local.sin_family==local_.sin_family&&local.sin_addr.s_addr==local_.sin_addr.s_addr&&local.sin_port==local_.sin_port){if(windows_socket_gate::original_close(socket_)==0)windows_socket_gate::sockets.erase(it);}}
  state_.owned=false;state_.ownershipReleased=true;socket_=INVALID_SOCKET;
 }
};
}
