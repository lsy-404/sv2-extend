#include "../loader/windows/network_scope.h"
#include <cassert>
#include <iostream>
#include <string>
using namespace network_scope;
static SOCKET bound(int family,int type){
 SOCKET value=socket(family,type,0);if(value==INVALID_SOCKET)return value;
 if(family==AF_INET){sockaddr_in a{};a.sin_family=AF_INET;a.sin_addr.s_addr=htonl(INADDR_LOOPBACK);if(bind(value,reinterpret_cast<sockaddr*>(&a),sizeof a)==SOCKET_ERROR){closesocket(value);return INVALID_SOCKET;}}
 else{sockaddr_in6 a{};a.sin6_family=AF_INET6;a.sin6_addr=in6addr_loopback;if(bind(value,reinterpret_cast<sockaddr*>(&a),sizeof a)==SOCKET_ERROR){closesocket(value);return INVALID_SOCKET;}}
 return value;
}
static bool connected(int family,SOCKET &listener,SOCKET &client,SOCKET &server){
 listener=bound(family,SOCK_STREAM);if(listener==INVALID_SOCKET)return false;if(listen(listener,1)==SOCKET_ERROR)return false;
 sockaddr_storage address{};int length=sizeof address;if(getsockname(listener,reinterpret_cast<sockaddr*>(&address),&length)==SOCKET_ERROR)return false;
 client=socket(family,SOCK_STREAM,0);if(client==INVALID_SOCKET||connect(client,reinterpret_cast<sockaddr*>(&address),length)==SOCKET_ERROR)return false;
 server=accept(listener,nullptr,nullptr);return server!=INVALID_SOCKET;
}
int wmain(int argc,wchar_t **argv){
 if(argc==2&&std::wstring(argv[1])==L"--child"){Sleep(300);return 0;}
 assert(!tcp_active(MIB_TCP_STATE_LISTEN)&&!tcp_active(MIB_TCP_STATE_TIME_WAIT)&&tcp_active(MIB_TCP_STATE_ESTAB));
 assert(loopback4(htonl(0x7f010203))&&!loopback4(htonl(0x0a000001)));unsigned char mapped[16]={0,0,0,0,0,0,0,0,0,0,0xff,0xff,127,0,0,1};assert(loopback6(mapped));mapped[12]=10;assert(!loopback6(mapped));
 std::vector<unsigned char> malformed(4);*reinterpret_cast<DWORD*>(malformed.data())=1;assert(!valid_table<MIB_TCPTABLE_OWNER_PID>(malformed));
 WSADATA wsa{};assert(WSAStartup(MAKEWORD(2,2),&wsa)==0);
 HANDLE jobHandle=CreateJobObjectW(nullptr,nullptr);assert(jobHandle&&AssignProcessToJobObject(jobHandle,GetCurrentProcess()));
 auto handleText=std::to_wstring(reinterpret_cast<uintptr_t>(jobHandle));assert(SetEnvironmentVariableW(L"OPEN_SV2_HOST_JOB_HANDLE",handleText.c_str()));
 auto first=job();assert(first.error==0&&first.child_scope&&first.self_present&&first.child_count==0);
 wchar_t exe[32768]{};assert(GetModuleFileNameW(nullptr,exe,32768));std::wstring cmd=L"\""+std::wstring(exe)+L"\" --child";STARTUPINFOW startup{};startup.cb=sizeof startup;PROCESS_INFORMATION child{};
 assert(CreateProcessW(exe,cmd.data(),nullptr,nullptr,FALSE,CREATE_SUSPENDED|CREATE_NO_WINDOW,nullptr,nullptr,&startup,&child));
 auto during=job();assert(during.error==0&&!during.child_scope&&during.child_count==1);ResumeThread(child.hThread);assert(WaitForSingleObject(child.hProcess,5000)==WAIT_OBJECT_0);CloseHandle(child.hThread);CloseHandle(child.hProcess);
 auto after=job();for(int i=0;i<100&&(!after.child_scope||after.error);++i){Sleep(20);after=job();}assert(after.error==0&&after.child_scope&&after.child_count==0);
 auto baseline=connections(GetCurrentProcessId());assert(baseline.complete);
 SOCKET listener=INVALID_SOCKET,client=INVALID_SOCKET,server=INVALID_SOCKET;assert(connected(AF_INET,listener,client,server));SOCKET udp=bound(AF_INET,SOCK_DGRAM);assert(udp!=INVALID_SOCKET);
 auto ipv4=connections(GetCurrentProcessId());assert(ipv4.complete&&ipv4.tcp4_active>=baseline.tcp4_active+2&&ipv4.tcp4_loopback>=baseline.tcp4_loopback+2&&ipv4.udp4_active>=baseline.udp4_active+1&&ipv4.udp4_loopback>=baseline.udp4_loopback+1);
 SOCKET listener6=INVALID_SOCKET,client6=INVALID_SOCKET,server6=INVALID_SOCKET;bool has6=connected(AF_INET6,listener6,client6,server6);SOCKET udp6=bound(AF_INET6,SOCK_DGRAM);bool hasUdp6=udp6!=INVALID_SOCKET;
 auto ipv6=connections(GetCurrentProcessId());assert(ipv6.complete);if(has6)assert(ipv6.tcp6_active>=baseline.tcp6_active+2&&ipv6.tcp6_loopback>=baseline.tcp6_loopback+2);if(hasUdp6)assert(ipv6.udp6_active>=baseline.udp6_active+1&&ipv6.udp6_loopback>=baseline.udp6_loopback+1);
 for(SOCKET s:{server,client,listener,server6,client6,listener6,udp,udp6})if(s!=INVALID_SOCKET)closesocket(s);
 auto wrong=connections(GetCurrentProcessId()+1);assert(!wrong.complete&&wrong.tcp4_error==ERROR_INVALID_PARAMETER);
 CloseHandle(jobHandle);auto invalid=job();assert(!invalid.child_scope&&invalid.error!=0);WSACleanup();
 std::cout<<"job child lifecycle and IPv4 loopback passed; IPv6 TCP "<<(has6?"passed":"skipped")<<", UDP "<<(hasUdp6?"passed":"skipped")<<"\n";return 0;
}
