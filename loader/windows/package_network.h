#pragma once
// Network services Loader performs for packages: outbound requests through
// WinHTTP, and a loopback HTTP listener whose requests a package handles in
// its own code. The listener is created outside the host socket gate, like
// Loader's other local control channels, and stays up while the host is offline.
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <winhttp.h>
#include <bcrypt.h>
#include <atomic>
#include <chrono>
#include <functional>
#include <future>
#include <memory>
#include <string>
#include <thread>
#include <vector>
#include "vendor/json.hpp"
#include "windows_socket_gate.h"
extern "C" {
#include "../packages/vendor/picohttpparser/picohttpparser.h"
}

namespace package_network {
using json=nlohmann::json;
using Done=std::function<void(const json&)>;
constexpr size_t request_body_limit=1024*1024,response_body_limit=4*1024*1024,header_limit=16384;
inline json failure(const char *code,json extra=json::object()){extra["code"]=code;return {{"ok",false},{"error",extra}};}
inline std::wstring wide(const std::string &text){if(text.empty())return L"";int size=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,text.data(),static_cast<int>(text.size()),nullptr,0);std::wstring out(size>0?size:0,L'\0');if(size>0)MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,text.data(),static_cast<int>(text.size()),&out[0],size);return out;}
inline std::string narrow(const std::wstring &text){if(text.empty())return "";int size=WideCharToMultiByte(CP_UTF8,0,text.data(),static_cast<int>(text.size()),nullptr,0,nullptr,nullptr);std::string out(size>0?size:0,'\0');if(size>0)WideCharToMultiByte(CP_UTF8,0,text.data(),static_cast<int>(text.size()),&out[0],size,nullptr,nullptr);return out;}
inline std::string lower(std::string text){for(auto &character:text)if(character>='A'&&character<='Z')character=static_cast<char>(character+32);return text;}
inline std::string base64(const std::string &data){
 static const char table[]="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";std::string out;out.reserve((data.size()+2)/3*4);
 for(size_t index=0;index<data.size();index+=3){unsigned value=static_cast<unsigned char>(data[index])<<16;if(index+1<data.size())value|=static_cast<unsigned char>(data[index+1])<<8;if(index+2<data.size())value|=static_cast<unsigned char>(data[index+2]);
  out+=table[value>>18&63];out+=table[value>>12&63];out+=index+1<data.size()?table[value>>6&63]:'=';out+=index+2<data.size()?table[value&63]:'=';}
 return out;
}
// Text bodies travel as "body"; anything that is not UTF-8 as "bodyBase64".
inline void put_body(json &target,const std::string &body){if(body.empty()||!wide(body).empty())target["body"]=body;else target["bodyBase64"]=base64(body);}
inline bool header_token(const std::string &name){if(name.empty()||name.size()>128)return false;for(unsigned char c:name)if(!isalnum(c)&&!strchr("!#$%&'*+-.^_`|~",c))return false;return true;}
inline bool header_value(const std::string &value){if(value.size()>8192)return false;for(unsigned char c:value)if(c=='\r'||c=='\n'||c==0)return false;return true;}
inline bool loopback(const std::string &host){return host=="127.0.0.1"||host=="localhost"||host=="::1";}
// A pattern is an exact host name, or "*.example.com" for any subdomain.
inline bool host_allowed(const std::vector<std::string> &patterns,const std::string &host){
 for(const auto &pattern:patterns){if(pattern==host)return true;if(pattern.rfind("*.",0)==0&&host.size()>pattern.size()-1&&host.compare(host.size()-(pattern.size()-1),pattern.size()-1,pattern,1,pattern.size()-1)==0)return true;}
 return false;
}

struct Handle{HINTERNET value=nullptr;~Handle(){if(value)WinHttpCloseHandle(value);}operator HINTERNET()const{return value;}};
struct Outbound{std::wstring host,path,method,headers;std::string body;INTERNET_PORT port=0;bool secure=false;int timeout=30000;};
inline std::atomic<int> outbound_active{0};
inline json perform(const Outbound &request){
 auto failed=[](const char *stage){return failure("HTTP_FAILED",{{"stage",stage},{"status",static_cast<unsigned>(GetLastError())}});};
 Handle session,connection,call;
 session.value=WinHttpOpen(L"SV2Extend",4/*automatic proxy*/,WINHTTP_NO_PROXY_NAME,WINHTTP_NO_PROXY_BYPASS,0);if(!session)session.value=WinHttpOpen(L"SV2Extend",WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,WINHTTP_NO_PROXY_NAME,WINHTTP_NO_PROXY_BYPASS,0);
 if(!session||!WinHttpSetTimeouts(session,request.timeout,request.timeout,request.timeout,request.timeout))return failed("open");
 connection.value=WinHttpConnect(session,request.host.c_str(),request.port,0);if(!connection)return failed("connect");
 call.value=WinHttpOpenRequest(connection,request.method.c_str(),request.path.c_str(),nullptr,WINHTTP_NO_REFERER,WINHTTP_DEFAULT_ACCEPT_TYPES,request.secure?WINHTTP_FLAG_SECURE:0);if(!call)return failed("request");
 // Redirects would leave the declared hosts; cookies and credentials are never implied.
 DWORD disabled=WINHTTP_DISABLE_REDIRECTS|WINHTTP_DISABLE_COOKIES|WINHTTP_DISABLE_AUTHENTICATION;if(!WinHttpSetOption(call,WINHTTP_OPTION_DISABLE_FEATURE,&disabled,sizeof disabled))return failed("request");
 if(!request.headers.empty()&&!WinHttpAddRequestHeaders(call,request.headers.c_str(),static_cast<DWORD>(-1),WINHTTP_ADDREQ_FLAG_ADD|WINHTTP_ADDREQ_FLAG_REPLACE))return failed("headers");
 if(!WinHttpSendRequest(call,WINHTTP_NO_ADDITIONAL_HEADERS,0,request.body.empty()?WINHTTP_NO_REQUEST_DATA:const_cast<char*>(request.body.data()),static_cast<DWORD>(request.body.size()),static_cast<DWORD>(request.body.size()),0))return failed("send");
 if(!WinHttpReceiveResponse(call,nullptr))return failed("receive");
 DWORD status=0,size=sizeof status;if(!WinHttpQueryHeaders(call,WINHTTP_QUERY_STATUS_CODE|WINHTTP_QUERY_FLAG_NUMBER,WINHTTP_HEADER_NAME_BY_INDEX,&status,&size,WINHTTP_NO_HEADER_INDEX))return failed("status");
 json headers=json::object();size=0;WinHttpQueryHeaders(call,WINHTTP_QUERY_RAW_HEADERS_CRLF,WINHTTP_HEADER_NAME_BY_INDEX,nullptr,&size,WINHTTP_NO_HEADER_INDEX);
 if(size){std::wstring raw(size/sizeof(wchar_t),L'\0');if(WinHttpQueryHeaders(call,WINHTTP_QUERY_RAW_HEADERS_CRLF,WINHTTP_HEADER_NAME_BY_INDEX,&raw[0],&size,WINHTTP_NO_HEADER_INDEX)){auto text=narrow(raw.substr(0,size/sizeof(wchar_t)));size_t line=text.find("\r\n");
   while(line!=std::string::npos){size_t next=text.find("\r\n",line+2);if(next==std::string::npos||next==line+2)break;auto entry=text.substr(line+2,next-line-2);auto colon=entry.find(':');
    if(colon!=std::string::npos){auto name=lower(entry.substr(0,colon));auto value=entry.substr(entry.find_first_not_of(' ',colon+1)==std::string::npos?entry.size():entry.find_first_not_of(' ',colon+1));if(headers.contains(name))headers[name]=headers[name].get<std::string>()+", "+value;else headers[name]=value;}
    line=next;}}}
 std::string body;for(;;){DWORD available=0,read=0;if(!WinHttpQueryDataAvailable(call,&available))return failed("read");if(!available)break;if(body.size()+available>response_body_limit)return failure("RESPONSE_TOO_LARGE");size_t offset=body.size();body.resize(offset+available);if(!WinHttpReadData(call,&body[offset],available,&read))return failed("read");body.resize(offset+read);if(!read)break;}
 json result{{"status",status},{"headers",headers}};put_body(result,body);return {{"ok",true},{"result",result}};
}
// Validates on the caller's thread, then completes on a worker thread.
inline void request(const json &arguments,const std::vector<std::string> &hosts,Done done){
 Outbound out;
 if(!arguments.contains("url")||!arguments["url"].is_string()){done(failure("INVALID_ARGUMENT"));return;}
 auto url=wide(arguments["url"].get<std::string>());URL_COMPONENTS parts{};parts.dwStructSize=sizeof parts;parts.dwSchemeLength=parts.dwHostNameLength=parts.dwUrlPathLength=parts.dwExtraInfoLength=parts.dwUserNameLength=static_cast<DWORD>(-1);
 if(url.empty()||url.size()>8192||!WinHttpCrackUrl(url.c_str(),static_cast<DWORD>(url.size()),0,&parts)||!parts.lpszHostName||!parts.dwHostNameLength||parts.dwUserNameLength){done(failure("INVALID_URL"));return;}
 out.host.assign(parts.lpszHostName,parts.dwHostNameLength);auto host=lower(narrow(out.host));out.secure=parts.nScheme==INTERNET_SCHEME_HTTPS;out.port=parts.nPort;
 if(parts.lpszUrlPath)out.path.assign(parts.lpszUrlPath,parts.dwUrlPathLength);if(parts.lpszExtraInfo){std::wstring extra(parts.lpszExtraInfo,parts.dwExtraInfoLength);out.path+=extra.substr(0,extra.find(L'#'));}if(out.path.empty())out.path=L"/";
 if(!out.secure&&(parts.nScheme!=INTERNET_SCHEME_HTTP||!loopback(host))){done(failure("INSECURE_URL"));return;}
 if(!host_allowed(hosts,host)){done(failure("HOST_NOT_DECLARED",{{"host",host}}));return;}
 auto method=arguments.value("method",json("GET"));if(!method.is_string()){done(failure("INVALID_ARGUMENT"));return;}auto verb=method.get<std::string>();
 bool known=false;for(auto allowed:{"GET","HEAD","POST","PUT","PATCH","DELETE"})known=known||verb==allowed;if(!known){done(failure("INVALID_ARGUMENT"));return;}out.method=wide(verb);
 if(arguments.contains("headers")){
  if(!arguments["headers"].is_object()||arguments["headers"].size()>64){done(failure("INVALID_ARGUMENT"));return;}
  for(auto item=arguments["headers"].begin();item!=arguments["headers"].end();++item){
   auto name=lower(item.key());if(!item.value().is_string()||!header_token(name)||!header_value(item.value().get<std::string>())||name=="host"||name=="content-length"||name=="transfer-encoding"||name=="connection"){done(failure("INVALID_HEADER",{{"header",name}}));return;}
   out.headers+=wide(item.key()+": "+item.value().get<std::string>()+"\r\n");
  }
 }
 if(arguments.contains("body")){if(!arguments["body"].is_string()||arguments["body"].get_ref<const std::string&>().size()>request_body_limit){done(failure("INVALID_ARGUMENT"));return;}out.body=arguments["body"].get<std::string>();}
 if(arguments.contains("timeoutMs")){if(!arguments["timeoutMs"].is_number_integer()||arguments["timeoutMs"]<1000||arguments["timeoutMs"]>120000){done(failure("INVALID_ARGUMENT"));return;}out.timeout=arguments["timeoutMs"].get<int>();}
 if(++outbound_active>8){--outbound_active;done(failure("HTTP_BUSY"));return;}
 try{std::thread([out,done]{json response;try{response=perform(out);}catch(const std::exception&){response=failure("HTTP_FAILED");}--outbound_active;done(response);}).detach();}
 catch(const std::exception&){--outbound_active;done(failure("HTTP_BUSY"));}
}

// The handler runs on a connection thread and completes at most once.
using Handler=std::function<void(json request,Done respond)>;
struct Listener{SOCKET socket=INVALID_SOCKET;std::string token;bool authenticated=true;Handler handler;std::atomic<bool> open{true};std::atomic<int> connections{0};};
inline SOCKET make_socket(){return windows_socket_gate::original_socket_w?windows_socket_gate::original_socket_w(AF_INET,SOCK_STREAM,IPPROTO_TCP,nullptr,0,0):WSASocketW(AF_INET,SOCK_STREAM,IPPROTO_TCP,nullptr,0,0);}
inline SOCKET accept_socket(SOCKET listener){return windows_socket_gate::original_waccept?windows_socket_gate::original_waccept(listener,nullptr,nullptr,nullptr,0):WSAAccept(listener,nullptr,nullptr,nullptr,0);}
inline void close_socket(SOCKET value){if(windows_socket_gate::original_close)windows_socket_gate::original_close(value);else closesocket(value);}
inline bool send_all(SOCKET client,const std::string &data){size_t sent=0;while(sent<data.size()){int count=send(client,data.data()+sent,static_cast<int>((std::min)(data.size()-sent,size_t(65536))),0);if(count<=0)return false;sent+=count;}return true;}
inline void respond(SOCKET client,int status,const json &headers,const std::string &body){
 std::string head="HTTP/1.1 "+std::to_string(status)+" \r\n";
 if(headers.is_object())for(auto item=headers.begin();item!=headers.end();++item){auto name=lower(item.key());if(item.value().is_string()&&header_token(name)&&header_value(item.value().get<std::string>())&&name!="content-length"&&name!="connection"&&name!="transfer-encoding")head+=item.key()+": "+item.value().get<std::string>()+"\r\n";}
 head+="Content-Length: "+std::to_string(body.size())+"\r\nConnection: close\r\n\r\n";if(send_all(client,head))send_all(client,body);
}
inline void serve(std::shared_ptr<Listener> listener,SOCKET client){
 DWORD timeout=10000;setsockopt(client,SOL_SOCKET,SO_RCVTIMEO,reinterpret_cast<const char*>(&timeout),sizeof timeout);setsockopt(client,SOL_SOCKET,SO_SNDTIMEO,reinterpret_cast<const char*>(&timeout),sizeof timeout);
 std::string buffer;const char *method=nullptr,*path=nullptr;size_t method_length=0,path_length=0,count=0,previous=0;int minor=0,parsed=-2;phr_header headers[64];
 while(parsed==-2&&buffer.size()<header_limit){char chunk[4096];int received=recv(client,chunk,sizeof chunk,0);if(received<=0)break;previous=buffer.size();buffer.append(chunk,received);count=64;parsed=phr_parse_request(buffer.data(),buffer.size(),&method,&method_length,&path,&path_length,&minor,headers,&count,previous);}
 if(parsed<=0){if(parsed!=-2||buffer.size()>=header_limit)respond(client,400,json(),"");return;}
 json fields=json::object();size_t length=0;bool chunked=false,sized=false;
 for(size_t index=0;index<count;++index){
  auto name=lower(std::string(headers[index].name?headers[index].name:"",headers[index].name_len));std::string value(headers[index].value,headers[index].value_len);if(name.empty()){respond(client,400,json(),"");return;}
  if(name=="transfer-encoding")chunked=true;
  if(name=="content-length"){char *end=nullptr;auto number=strtoull(value.c_str(),&end,10);if(sized||value.empty()||*end||number>request_body_limit){respond(client,sized||value.empty()||*end?400:413,json(),"");return;}length=static_cast<size_t>(number);sized=true;}
  if(fields.contains(name))fields[name]=fields[name].get<std::string>()+", "+value;else fields[name]=value;
 }
 if(chunked){respond(client,411,json(),"");return;}
 // A page in a browser cannot read the token, and is refused outright when the package opted out of it.
 if(listener->authenticated?fields.value("authorization",std::string())!="Bearer "+listener->token:fields.contains("origin")){respond(client,listener->authenticated?401:403,json(),"");return;}
 std::string body=buffer.substr(parsed);while(body.size()<length){char chunk[16384];int received=recv(client,chunk,static_cast<int>((std::min)(sizeof chunk,length-body.size())),0);if(received<=0){respond(client,400,json(),"");return;}body.append(chunk,received);}
 body.resize(length);std::string target(path,path_length);auto query=target.find('?');
 json request{{"method",std::string(method,method_length)},{"path",target.substr(0,query)},{"query",query==std::string::npos?"":target.substr(query+1)},{"headers",fields}};put_body(request,body);
 auto promise=std::make_shared<std::promise<json>>();auto future=promise->get_future();
 listener->handler(std::move(request),[promise](const json &response){try{promise->set_value(response);}catch(const std::future_error&){}});
 if(future.wait_for(std::chrono::seconds(30))!=std::future_status::ready){respond(client,504,json(),"");return;}
 auto response=future.get();if(!response.is_object()||!response.value("ok",false)||!response.contains("result")||!response["result"].is_object()){respond(client,500,json(),"");return;}
 const auto &result=response["result"];int status=result.value("status",json(200)).is_number_integer()?result.value("status",200):200;if(status<100||status>599)status=500;
 auto text=result.value("body",json(""));std::string payload=text.is_string()?text.get<std::string>():"";if(payload.size()>response_body_limit){respond(client,500,json(),"");return;}
 respond(client,status,result.value("headers",json::object()),payload);
}
class Server{
public:
 // Binds an ephemeral loopback port. Requests need "Authorization: Bearer <token>" unless authenticated is false.
 static std::unique_ptr<Server> listen(Handler handler,bool authenticated){
  static const bool started=[]{WSADATA data;return WSAStartup(MAKEWORD(2,2),&data)==0;}();if(!started)return nullptr;
  auto listener=std::make_shared<Listener>();listener->handler=std::move(handler);listener->authenticated=authenticated;
  unsigned char random[24];if(BCryptGenRandom(nullptr,random,sizeof random,BCRYPT_USE_SYSTEM_PREFERRED_RNG)<0)return nullptr;static const char digits[]="0123456789abcdef";for(auto byte:random){listener->token+=digits[byte>>4];listener->token+=digits[byte&15];}
  listener->socket=make_socket();if(listener->socket==INVALID_SOCKET)return nullptr;
  sockaddr_in address{};address.sin_family=AF_INET;address.sin_addr.s_addr=htonl(INADDR_LOOPBACK);int size=sizeof address;
  if(bind(listener->socket,reinterpret_cast<sockaddr*>(&address),sizeof address)||::listen(listener->socket,16)||getsockname(listener->socket,reinterpret_cast<sockaddr*>(&address),&size)){close_socket(listener->socket);return nullptr;}
  std::unique_ptr<Server> server(new Server);server->listener=listener;server->port=ntohs(address.sin_port);
  try{std::thread([listener]{
   for(;;){
    // Closing a socket another thread is accepting on blocks, so this thread polls and closes it itself.
    fd_set ready;FD_ZERO(&ready);FD_SET(listener->socket,&ready);timeval interval{0,100000};int waiting=select(0,&ready,nullptr,nullptr,&interval);
    if(!listener->open||waiting<0){close_socket(listener->socket);break;}if(!waiting)continue;
    SOCKET client=accept_socket(listener->socket);if(client==INVALID_SOCKET)continue;
    if(!listener->open||listener->connections>=16){close_socket(client);continue;}
    ++listener->connections;
    try{std::thread([listener,client]{try{serve(listener,client);}catch(const std::exception&){}shutdown(client,SD_SEND);close_socket(client);--listener->connections;}).detach();}catch(const std::exception&){close_socket(client);--listener->connections;}
   }
  }).detach();}catch(const std::exception&){close_socket(listener->socket);return nullptr;}
  return server;
 }
 ~Server(){listener->open=false;}
 uint16_t port=0;const std::string &token()const{return listener->token;}
private:
 Server()=default;std::shared_ptr<Listener> listener;
};
}
