#pragma once
#include "windows_socket_gate.h"
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <set>
#include <string>
#include <thread>
#include <vector>
#include <filesystem>
#include <fstream>
#include "vendor/json.hpp"

namespace windows_webview_transport {
struct State { bool installed=false, blocked=true, pending=false; unsigned active=0; bool coverage=false; };
struct Internal { bool installed=false, blocked=true, error=false; unsigned active=0; };
inline std::mutex mutex;
inline std::condition_variable changed;
inline Internal current;
inline std::set<std::wstring> prepared;
inline SOCKET listener=INVALID_SOCKET;
inline std::set<SOCKET> clients, remotes;
inline uint16_t port=0;
inline bool read_exact(SOCKET s,void *data,size_t size) {
 char *p=static_cast<char*>(data);while(size){int n=recv(s,p,static_cast<int>(size),0);if(n<=0)return false;p+=n;size-=n;}return true;
}
inline bool write_exact(SOCKET s,const void *data,size_t size) {
 const char *p=static_cast<const char*>(data);while(size){int n=send(s,p,static_cast<int>(size),0);if(n<=0)return false;p+=n;size-=n;}return true;
}
inline void reply(SOCKET s,unsigned char result){unsigned char r[]={5,result,0,1,0,0,0,0,0,0};write_exact(s,r,sizeof r);}
inline bool blocked(){std::lock_guard<std::mutex> lock(mutex);return current.blocked;}
inline void relay(SOCKET client) {
 SOCKET remote=INVALID_SOCKET;
 struct Finish {SOCKET client;SOCKET &remote;~Finish(){
  {std::lock_guard<std::mutex> lock(mutex);clients.erase(client);if(remote!=INVALID_SOCKET)remotes.erase(remote);}
  if(remote!=INVALID_SOCKET)closesocket(remote);windows_socket_gate::original_close(client);
  {std::lock_guard<std::mutex> lock(mutex);--current.active;}changed.notify_all();
 }} finish{client,remote};
 DWORD timeout=5000;setsockopt(client,SOL_SOCKET,SO_RCVTIMEO,reinterpret_cast<char*>(&timeout),sizeof timeout);
 unsigned char h[4],methods[255],raw[16],length=0,pb[2];char hostname[256]{};
 if(!read_exact(client,h,2)||h[0]!=5||!h[1]||!read_exact(client,methods,h[1]))return;
 bool noauth=false;for(unsigned i=0;i<h[1];++i)if(methods[i]==0)noauth=true;
 unsigned char method[]={5,static_cast<unsigned char>(noauth?0:255)};if(!write_exact(client,method,2)||!noauth)return;
 if(!read_exact(client,h,4)||h[0]!=5||h[1]!=1||h[2]!=0){reply(client,7);return;}
 if(h[3]==1||h[3]==4){int family=h[3]==1?AF_INET:AF_INET6;size_t n=h[3]==1?4:16;if(!read_exact(client,raw,n)||!inet_ntop(family,raw,hostname,sizeof hostname))return;}
 else if(h[3]==3){if(!read_exact(client,&length,1)||!length||!read_exact(client,hostname,length))return;}
 else {reply(client,8);return;}
 if(!read_exact(client,pb,2))return;if(blocked()){reply(client,2);return;}
 char service[8];snprintf(service,sizeof service,"%u",(unsigned(pb[0])<<8)|pb[1]);
 addrinfo hints{},*list=nullptr;hints.ai_family=AF_UNSPEC;hints.ai_socktype=SOCK_STREAM;
 if(getaddrinfo(hostname,service,&hints,&list)!=0){reply(client,4);return;}
 for(auto a=list;a&&!blocked();a=a->ai_next){
  SOCKET s=socket(a->ai_family,a->ai_socktype,a->ai_protocol);if(s==INVALID_SOCKET)continue;
  {std::lock_guard<std::mutex> lock(mutex);if(current.blocked){closesocket(s);break;}try{remotes.insert(s);remote=s;}catch(...){current.error=true;closesocket(s);break;}}
  u_long nonblocking=1;ioctlsocket(s,FIONBIO,&nonblocking);int rc=connect(s,a->ai_addr,static_cast<int>(a->ai_addrlen));
  if(rc==SOCKET_ERROR&&WSAGetLastError()==WSAEWOULDBLOCK){fd_set out,err;FD_ZERO(&out);FD_ZERO(&err);FD_SET(s,&out);FD_SET(s,&err);timeval tv{5,0};rc=select(0,nullptr,&out,&err,&tv);int e=0;int n=sizeof e;if(rc>0&&FD_ISSET(s,&out)&&getsockopt(s,SOL_SOCKET,SO_ERROR,reinterpret_cast<char*>(&e),&n)==0&&e==0)rc=0;else rc=SOCKET_ERROR;}
  if(rc==0&&!blocked()){nonblocking=0;ioctlsocket(s,FIONBIO,&nonblocking);break;}
  {std::lock_guard<std::mutex> lock(mutex);remotes.erase(s);remote=INVALID_SOCKET;}closesocket(s);
 }
 freeaddrinfo(list);if(remote==INVALID_SOCKET||blocked()){reply(client,5);return;}
 if(!write_exact(client,"\x05\x00\x00\x01\x00\x00\x00\x00\x00\x00",10))return;
 for(;;){fd_set in;FD_ZERO(&in);FD_SET(client,&in);FD_SET(remote,&in);timeval tv{1,0};int n=select(0,&in,nullptr,nullptr,&tv);if(n<0||blocked())return;
  SOCKET endpoints[]={client,remote};for(int i=0;i<2;i++)if(FD_ISSET(endpoints[i],&in)){char buffer[16384];int got=recv(endpoints[i],buffer,sizeof buffer,0);if(got<=0||blocked()||!write_exact(endpoints[1-i],buffer,got))return;}
 }
}
inline bool install(){
 std::lock_guard<std::mutex> lock(mutex);if(current.installed)return true;
 if(!windows_socket_gate::current.installed||!windows_socket_gate::original_socket||!windows_socket_gate::original_accept||!windows_socket_gate::original_close)return false;
 WSADATA data{};if(WSAStartup(MAKEWORD(2,2),&data)!=0)return false;
 SOCKET s=windows_socket_gate::original_socket(AF_INET,SOCK_STREAM,IPPROTO_TCP);if(s==INVALID_SOCKET)return false;
 sockaddr_in address{};address.sin_family=AF_INET;address.sin_addr.s_addr=htonl(INADDR_LOOPBACK);
 if(bind(s,reinterpret_cast<sockaddr*>(&address),sizeof address)||listen(s,32)){windows_socket_gate::original_close(s);return false;}
 int length=sizeof address;if(getsockname(s,reinterpret_cast<sockaddr*>(&address),&length)){windows_socket_gate::original_close(s);return false;}
 listener=s;port=ntohs(address.sin_port);current.installed=true;
 try {std::thread([]{for(;;){SOCKET client=windows_socket_gate::original_accept(listener,nullptr,nullptr);if(client==INVALID_SOCKET){std::lock_guard<std::mutex> lock(mutex);current.error=true;return;}
  {std::lock_guard<std::mutex> lock(mutex);if(current.blocked){windows_socket_gate::original_close(client);continue;}try{clients.insert(client);++current.active;}catch(...){current.error=true;windows_socket_gate::original_close(client);continue;}}
  try{std::thread(relay,client).detach();}catch(...){std::lock_guard<std::mutex> lock(mutex);clients.erase(client);--current.active;current.error=true;windows_socket_gate::original_close(client);changed.notify_all();}
 }}).detach();}catch(...){windows_socket_gate::original_close(s);listener=INVALID_SOCKET;current.installed=false;current.error=true;return false;}return true;
}
inline std::wstring arguments(){std::lock_guard<std::mutex> lock(mutex);if(!current.installed)return {};return L"--proxy-server=socks5://127.0.0.1:"+std::to_wstring(port)+L" --proxy-bypass-list=<-loopback> --disable-quic";}
inline bool setblocked(bool value){std::unique_lock<std::mutex> lock(mutex);if(!current.installed)return false;if(!value&&current.error)return false;current.blocked=value;if(!value)return true;for(auto s:clients)shutdown(s,SD_BOTH);for(auto s:remotes)shutdown(s,SD_BOTH);return changed.wait_for(lock,std::chrono::seconds(10),[]{return current.active==0;})&&!current.error;}
inline State state(){std::lock_guard<std::mutex> lock(mutex);return {current.installed,current.blocked,current.blocked&&current.active!=0,static_cast<unsigned>(remotes.size()),current.installed&&!current.error};}
inline bool prepare_profile(PCWSTR udf){
 if(!udf||!*udf)return false;
 std::lock_guard<std::mutex> lock(mutex);
 try{
  auto root=std::filesystem::absolute(std::filesystem::path(udf)).lexically_normal();auto key=root.wstring();if(prepared.count(key))return true;
  auto dir=root/L"EBWebView"/L"Default";std::filesystem::create_directories(dir);auto path=dir/L"Preferences";
  nlohmann::json prefs=nlohmann::json::object();if(std::filesystem::exists(path)){std::ifstream input(path,std::ios::binary);if(!input)return false;input>>prefs;if(!prefs.is_object())return false;}
  if(prefs.contains("webrtc")&&!prefs["webrtc"].is_object())return false;
  prefs["webrtc"]["ip_handling_policy"]="disable_non_proxied_udp";
  auto temp=dir/(L"Preferences."+std::to_wstring(GetCurrentProcessId())+L".tmp");
  {std::ofstream output(temp,std::ios::binary|std::ios::trunc);if(!output)return false;output<<prefs.dump();output.flush();if(!output)return false;}
  if(!MoveFileExW(temp.c_str(),path.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)){DeleteFileW(temp.c_str());return false;}
  prepared.insert(key);return true;
 }catch(...){return false;}
}
}
