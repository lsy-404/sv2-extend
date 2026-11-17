#pragma once
#include <windows.h>
#include <cstdint>
#include <limits>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include "vendor/json.hpp"
#include "../../sdk/include/loader_api.h"

namespace script_runtime {
using json=nlohmann::json;
struct Ticket { ULONGLONG deadline; std::mutex mutex; json response; bool done=false; };
class Service {
 const loader_api_v1 *api; DWORD thread; uint64_t next=1;
 std::map<uint64_t,std::shared_ptr<Ticket>> tickets;
 using Pending=std::map<uintptr_t,std::weak_ptr<Ticket>>;
 static Pending &pending(){static Pending value;return value;}
 static std::mutex &lock(){static std::mutex value;return value;}
 static uintptr_t &sequence(){static uintptr_t value=0;return value;}
 static void complete(const char *body,size_t length,void *context){
  std::shared_ptr<Ticket> ticket;
  {std::lock_guard<std::mutex> guard(lock());auto it=pending().find(reinterpret_cast<uintptr_t>(context));if(it==pending().end())return;ticket=it->second.lock();pending().erase(it);}
  if(!ticket)return;
  auto value=body&&length<=1048576?json::parse(body,body+length,nullptr,false):json();
  std::lock_guard<std::mutex> guard(ticket->mutex);
  ticket->response=value.is_object()&&value.contains("ok")&&value["ok"].is_boolean()?value:failure("INVALID_NATIVE_RESPONSE");ticket->done=true;
 }
 void expire(){auto now=GetTickCount64();for(auto it=tickets.begin();it!=tickets.end();)if(now>=it->second->deadline)it=tickets.erase(it);else ++it;
  std::lock_guard<std::mutex> guard(lock());for(auto it=pending().begin();it!=pending().end();)if(it->second.expired())it=pending().erase(it);else ++it;}
public:
 explicit Service(const loader_api_v1 *value):api(value),thread(GetCurrentThreadId()){}
 static json failure(const char *code){return {{"ok",false},{"error",{{"code",code}}}};}
 static json success(json value=json::object()){return {{"ok",true},{"result",value}};}
 json call(const std::string &method,const json &args){
  if(GetCurrentThreadId()!=thread)return failure("MAIN_THREAD_REQUIRED");
  if(!args.is_object())return failure("INVALID_ARGUMENT");
  expire();
  if(method=="call.submit"){
   if(!args.contains("method")||!args["method"].is_string()||args["method"].get<std::string>().empty()||args["method"].get<std::string>().size()>96)return failure("INVALID_ARGUMENT");
   auto payload=args.value("arguments",json::object());if(!payload.is_object())return failure("INVALID_ARGUMENT");auto encoded=payload.dump();if(encoded.size()>262144)return failure("REQUEST_TOO_LARGE");
   if(!api||!api->request_capability_json)return failure("UNSUPPORTED_HOST_CAPABILITY");
   if(tickets.size()>=64||next==std::numeric_limits<uint64_t>::max())return failure("CAPABILITY_BUSY");
   auto ticket=std::make_shared<Ticket>();ticket->deadline=GetTickCount64()+15000;auto id=next++;tickets.emplace(id,ticket);uintptr_t token;
   {std::lock_guard<std::mutex> guard(lock());if(sequence()==std::numeric_limits<uintptr_t>::max()){tickets.erase(id);return failure("CAPABILITY_BUSY");}token=++sequence();pending().emplace(token,ticket);}
   auto name=args["method"].get<std::string>();api->request_capability_json(name.c_str(),encoded.c_str(),complete,reinterpret_cast<void*>(token));
   return success({{"ticket",id}});
  }
  if(method=="call.result"){
   if(!args.contains("ticket")||!args["ticket"].is_number_unsigned())return failure("INVALID_ARGUMENT");
   auto it=tickets.find(args["ticket"].get<uint64_t>());if(it==tickets.end())return failure("CALL_TICKET_EXPIRED");
   auto ticket=it->second;json response;
   {std::lock_guard<std::mutex> guard(ticket->mutex);if(!ticket->done)return success({{"pending",true}});response=ticket->response;}
   tickets.erase(it);return success({{"pending",false},{"response",response}});
  }
  return failure("UNSUPPORTED_HOST_CAPABILITY");
 }
};
}
