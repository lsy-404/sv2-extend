#pragma once
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <map>
#include <mutex>
#include "vendor/minhook/include/MinHook.h"

namespace windows_socket_gate {
struct State { bool installed=false,blocked=false,pending=false,discovery_error=false;size_t active=0,unknown=0,close_failed=0; };
inline std::mutex mutex;
struct Entry {int family;bool failed=false;};
inline std::map<SOCKET,Entry> sockets;
inline State current;
using Socket=SOCKET(WSAAPI*)(int,int,int);
using SocketA=SOCKET(WSAAPI*)(int,int,int,LPWSAPROTOCOL_INFOA,GROUP,DWORD);
using SocketW=SOCKET(WSAAPI*)(int,int,int,LPWSAPROTOCOL_INFOW,GROUP,DWORD);
using Accept=SOCKET(WSAAPI*)(SOCKET,sockaddr*,int*);
using WAccept=SOCKET(WSAAPI*)(SOCKET,sockaddr*,LPINT,LPCONDITIONPROC,DWORD_PTR);
using Close=int(WSAAPI*)(SOCKET);
inline Socket original_socket=nullptr;inline SocketA original_socket_a=nullptr;inline SocketW original_socket_w=nullptr;inline Accept original_accept=nullptr;inline WAccept original_waccept=nullptr;inline Close original_close=nullptr;
inline bool internet(int family){return family==AF_INET||family==AF_INET6;}
inline int family_of(SOCKET value,int hint){
 if(internet(hint)||hint==AF_UNIX)return hint;
 WSAPROTOCOL_INFOW info{};int length=sizeof info;
 return getsockopt(value,SOL_SOCKET,SO_PROTOCOL_INFOW,reinterpret_cast<char*>(&info),&length)==0?info.iAddressFamily:AF_UNSPEC;
}
inline SOCKET record(SOCKET value,int hint){
 if(value==INVALID_SOCKET)return value;
 int family=family_of(value,hint);if(family!=AF_UNSPEC&&!internet(family))return value;
 std::lock_guard<std::mutex> lock(mutex);
 if(current.blocked){if(original_close(value)==SOCKET_ERROR){try{sockets[value]={internet(family)?family:AF_UNSPEC,true};++current.close_failed;}catch(...){current.discovery_error=true;}}WSASetLastError(WSAEACCES);return INVALID_SOCKET;}
 try{sockets[value]={internet(family)?family:AF_UNSPEC,false};}catch(...){current.discovery_error=true;original_close(value);WSASetLastError(WSAENOBUFS);return INVALID_SOCKET;}
 return value;
}
inline SOCKET WSAAPI hook_socket(int family,int type,int protocol){
 {std::lock_guard<std::mutex> lock(mutex);if(current.blocked&&internet(family)){WSASetLastError(WSAEACCES);return INVALID_SOCKET;}}
 return record(original_socket(family,type,protocol),family);
}
inline SOCKET WSAAPI hook_socket_a(int family,int type,int protocol,LPWSAPROTOCOL_INFOA info,GROUP group,DWORD flags){
 int hint=family;
 {std::lock_guard<std::mutex> lock(mutex);if(current.blocked&&internet(hint)){WSASetLastError(WSAEACCES);return INVALID_SOCKET;}}
 return record(original_socket_a(family,type,protocol,info,group,flags),hint);
}
inline SOCKET WSAAPI hook_socket_w(int family,int type,int protocol,LPWSAPROTOCOL_INFOW info,GROUP group,DWORD flags){
 int hint=family;
 {std::lock_guard<std::mutex> lock(mutex);if(current.blocked&&internet(hint)){WSASetLastError(WSAEACCES);return INVALID_SOCKET;}}
 return record(original_socket_w(family,type,protocol,info,group,flags),hint);
}
inline SOCKET WSAAPI hook_accept(SOCKET value,sockaddr *address,int *length){return record(original_accept(value,address,length),AF_UNSPEC);}
inline SOCKET WSAAPI hook_waccept(SOCKET value,sockaddr *address,LPINT length,LPCONDITIONPROC condition,DWORD_PTR data){return record(original_waccept(value,address,length,condition,data),AF_UNSPEC);}
inline int WSAAPI hook_close(SOCKET value){std::lock_guard<std::mutex> lock(mutex);int result=original_close(value);if(result==0){auto it=sockets.find(value);if(it!=sockets.end()){if(it->second.failed&&current.close_failed)--current.close_failed;sockets.erase(it);}}return result;}
inline State state(){std::lock_guard<std::mutex> lock(mutex);State out=current;out.active=sockets.size();out.pending=out.blocked&&out.active!=0;out.unknown=0;for(const auto &entry:sockets)if(entry.second.family==AF_UNSPEC)++out.unknown;return out;}
inline bool setblocked(bool value){
 std::lock_guard<std::mutex> lock(mutex);if(!current.installed)return false;current.blocked=value;current.close_failed=0;
 if(value)for(auto it=sockets.begin();it!=sockets.end();){if(original_close(it->first)==0)it=sockets.erase(it);else{it->second.failed=true;++current.close_failed;++it;}}
 return !value||(sockets.empty()&&!current.discovery_error);
}
inline bool install(){
 std::lock_guard<std::mutex> lock(mutex);if(current.installed)return true;
 HMODULE ws=GetModuleHandleW(L"ws2_32.dll");if(!ws)ws=LoadLibraryExW(L"ws2_32.dll",nullptr,LOAD_LIBRARY_SEARCH_SYSTEM32);if(!ws)return false;
 const char *names[]={"socket","WSASocketA","WSASocketW","accept","WSAAccept","closesocket"};
 void *hooks[]={reinterpret_cast<void*>(hook_socket),reinterpret_cast<void*>(hook_socket_a),reinterpret_cast<void*>(hook_socket_w),reinterpret_cast<void*>(hook_accept),reinterpret_cast<void*>(hook_waccept),reinterpret_cast<void*>(hook_close)};
 void **originals[]={reinterpret_cast<void**>(&original_socket),reinterpret_cast<void**>(&original_socket_a),reinterpret_cast<void**>(&original_socket_w),reinterpret_cast<void**>(&original_accept),reinterpret_cast<void**>(&original_waccept),reinterpret_cast<void**>(&original_close)};
 void *targets[6]{};size_t created=0,enabled=0;
 for(size_t i=0;i<6;i++){targets[i]=reinterpret_cast<void*>(GetProcAddress(ws,names[i]));if(!targets[i]||MH_CreateHook(targets[i],hooks[i],originals[i])!=MH_OK)goto fail;++created;}
 for(size_t i=0;i<6;i++){if(MH_EnableHook(targets[i])!=MH_OK)goto fail;++enabled;}
 current.installed=true;return true;
fail:
 for(size_t i=0;i<enabled;i++)MH_DisableHook(targets[i]);
 for(size_t i=0;i<created;i++)MH_RemoveHook(targets[i]);
 current.discovery_error=true;return false;
}
}
