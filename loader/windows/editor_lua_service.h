#pragma once
#include <windows.h>
#include <map>
#include <string>
#include "vendor/json.hpp"
#include "../../sdk/include/loader_api.h"

namespace editor_lua {
using json=nlohmann::json;
struct Job {json request;loader_json_completion completion;void *context;ULONGLONG deadline;bool taken;};
struct Service {
 const loader_api_v1 *api;DWORD thread;std::string epoch,descriptor;bool registered=false;ULONGLONG heartbeat=0;unsigned long long sequence=0;std::map<std::string,Job> jobs;
 explicit Service(const loader_api_v1 *value):api(value),thread(GetCurrentThreadId()){}
 static json fail(const char *code){return {{"ok",false},{"error",{{"code",code}}}};}
 static json success(json value=json::object()){return {{"ok",true},{"result",value}};}
 static void reply(Job &job,const json &value){auto body=value.dump();job.completion(body.data(),body.size(),job.context);}
 static json definitions(){json tools=json::array({
  {{"name","svext_read_editor"},{"method","selection.read"},{"description","Read a group and its edit guard."},{"inputSchema",{{"type","object"},{"properties",{{"groupUUID",{{"type","string"}}}}},{"additionalProperties",false}}}},
  {{"name","svext_create_group"},{"method","groups.create"},{"description","Create a note group on the current track using the current edit guard."},{"inputSchema",{{"type","object"},{"properties",{{"name",{{"type","string"},{"maxLength",256}}},{"guard",{{"type","string"}}}}},{"required",json::array({"guard"})},{"additionalProperties",false}}}},
  {{"name","svext_remove_group"},{"method","groups.remove"},{"description","Remove an exclusively referenced library group using its fresh edit guard."},{"inputSchema",{{"type","object"},{"properties",{{"groupUUID",{{"type","string"}}},{"guard",{{"type","string"}}}}},{"required",json::array({"groupUUID","guard"})},{"additionalProperties",false}}}},
  {{"name","svext_set_note_pitch"},{"method","notes.set_pitch"},{"description","Set a note pitch using a fresh edit guard."},{"inputSchema",{{"type","object"},{"properties",{{"noteIndex",{{"type","integer"},{"minimum",1}}},{"midiPitch",{{"type","integer"},{"minimum",0},{"maximum",127}}},{"guard",{{"type","string"}}}}},{"required",json::array({"noteIndex","midiPitch","guard"})},{"additionalProperties",false}}}},
  {{"name","svext_add_note"},{"method","notes.add"},{"description","Add one note to an exclusive group using a fresh edit guard."},{"inputSchema",{{"type","object"},{"properties",{{"onsetBlick",{{"type","integer"},{"minimum",0}}},{"durationBlick",{{"type","integer"},{"minimum",1}}},{"midiPitch",{{"type","integer"},{"minimum",0},{"maximum",127}}},{"lyrics",{{"type","string"},{"maxLength",1024}}},{"guard",{{"type","string"}}}}},{"required",json::array({"onsetBlick","durationBlick","midiPitch","guard"})},{"additionalProperties",false}}}},
  {{"name","svext_remove_note"},{"method","notes.remove"},{"description","Remove one note from an exclusive group using a fresh edit guard."},{"inputSchema",{{"type","object"},{"properties",{{"noteIndex",{{"type","integer"},{"minimum",1}}},{"guard",{{"type","string"}}}}},{"required",json::array({"noteIndex","guard"})},{"additionalProperties",false}}}},
  {{"name","svext_read_pitch_range"},{"method","pitch.read_range"},{"description","Read pitch points and a range guard."},{"inputSchema",{{"type","object"},{"properties",{{"startBlick",{{"type","integer"},{"minimum",0}}},{"endBlick",{{"type","integer"},{"minimum",0}}}}},{"required",json::array({"startBlick","endBlick"})},{"additionalProperties",false}}}},
  {{"name","svext_set_pitch_curve"},{"method","pitch.replace_range"},{"description","Replace a bounded pitch range using fresh guards."},{"inputSchema",{{"type","object"},{"properties",{{"startBlick",{{"type","integer"},{"minimum",0}}},{"endBlick",{{"type","integer"},{"minimum",0}}},{"points",{{"type","array"},{"maxItems",4096}}},{"guard",{{"type","string"}}},{"curveGuard",{{"type","string"}}}}},{"required",json::array({"startBlick","endBlick","points","guard","curveGuard"})},{"additionalProperties",false}}}}
 });tools.push_back({{"name","svext_read_voicebank"},{"method","voicebanks.current"},{"description","Read the native voicebank identity and a fresh selection guard."},{"inputSchema",{{"type","object"},{"properties",{{"groupUUID",{{"type","string"}}}}},{"additionalProperties",false}}}});tools.push_back({{"name","svext_list_voicebanks"},{"method","voicebanks.list"},{"description","List available native voicebank identities."},{"inputSchema",{{"type","object"},{"properties",json::object()},{"additionalProperties",false}}}});tools.push_back({{"name","svext_select_voicebank"},{"method","voicebanks.select"},{"description","Select a native voicebank using a fresh reference guard and verify the result."},{"inputSchema",{{"type","object"},{"properties",{{"voicebankId",{{"type","string"}}},{"guard",{{"type","string"}}},{"groupUUID",{{"type","string"}}}}},{"required",json::array({"voicebankId","guard"})},{"additionalProperties",false}}}});for(auto &tool:tools){auto method=tool["method"].get<std::string>();if(method=="notes.set_pitch"||method=="notes.add"||method=="notes.remove"||method=="pitch.read_range"||method=="pitch.replace_range")tool["inputSchema"]["properties"]["groupUUID"]={{"type","string"}};}return tools;}
 static void dispatch(const char *method,const char *arguments,loader_json_completion done,void *context,void *owner){
  auto &self=*static_cast<Service*>(owner);auto args=json::parse(arguments,nullptr,false);ULONGLONG now=GetTickCount64();
  if(GetCurrentThreadId()!=self.thread || !self.registered || self.epoch.empty() || now-self.heartbeat>2000){Job j{{},done,context,0,false};reply(j,fail("EDITOR_PROVIDER_OFFLINE"));return;}
  if(!args.is_object() || self.jobs.size()>=32){Job j{{},done,context,0,false};reply(j,fail(args.is_object()?"EDITOR_BUSY":"INVALID_ARGUMENT"));return;}
  auto id=std::to_string(++self.sequence);auto expires=static_cast<long long>(time(nullptr))+4;
  self.jobs.emplace(id,Job{{{"id",id},{"epoch",self.epoch},{"method",method},{"arguments",args},{"expires",expires}},done,context,now+4000,false});
 }
 void suspend(){if(registered){registered=false;api->unregister_capability_provider("editor.lua",this);}auto old=std::move(jobs);jobs.clear();for(auto &item:old)reply(item.second,fail("EDITOR_PROVIDER_OFFLINE"));}
 void reset(){suspend();epoch.clear();descriptor.clear();heartbeat=0;}
 void expire_jobs(ULONGLONG now){for(auto it=jobs.begin();it!=jobs.end();){if(now>=it->second.deadline){auto job=std::move(it->second);it=jobs.erase(it);reply(job,fail("EDITOR_TIMEOUT"));}else ++it;}}
 void tick(){
  if(GetCurrentThreadId()!=thread)return;auto now=GetTickCount64();
  if(!epoch.empty() && registered && now-heartbeat>2000)suspend();
  expire_jobs(now);
 }
 json call(const std::string &method,const json &args){
  if(GetCurrentThreadId()!=thread)return fail("MAIN_THREAD_REQUIRED");if(!args.is_object())return fail("INVALID_ARGUMENT");
  if(method=="voice.native_status")return success({{"identityReadable",false},{"catalogReadable",false},{"selectable",false}});
  if(method=="notes.native_status")return success({{"addable",true},{"removable",true}});
  if(method=="groups.native_status")return success({{"available",true}});
  if(method=="export.native_status")return success({{"available",false}});
  if(method=="editor.attach"){
   auto now=GetTickCount64();if(!epoch.empty()&&now-heartbeat<=2000)return fail("EDITOR_PROVIDER_ALREADY_ATTACHED");
   const json supported=json::array({"selection.read","groups.create","groups.remove","notes.set_pitch","notes.add","notes.remove","pitch.read_range","pitch.replace_range","transport.status","transport.play","transport.stop","voicebanks.current","voicebanks.list","voicebanks.select"});
   if(!args.contains("methods")||!args["methods"].is_array()||args["methods"].empty()||args["methods"].size()>supported.size())return fail("INVALID_ARGUMENT");
   json accepted=json::array();
   for(const auto &item:args["methods"]){if(!item.is_string()||std::find(supported.begin(),supported.end(),item)==supported.end()||std::find(accepted.begin(),accepted.end(),item)!=accepted.end())return fail("UNSUPPORTED_HOST_CAPABILITY");accepted.push_back(item);}
   json tools=json::array();for(auto &item:definitions())if(std::find(accepted.begin(),accepted.end(),item["method"])!=accepted.end())tools.push_back(item);
   auto nextDescriptor=json{{"methods",accepted},{"tools",tools},{"provider","official-lua"}}.dump();
   if(!epoch.empty())suspend();
   if(api->register_capability_provider("editor.lua",dispatch,this,nextDescriptor.c_str())!=LOADER_OK)return fail("EDITOR_REGISTRATION_REJECTED");
   registered=true;descriptor=std::move(nextDescriptor);epoch=std::to_string(GetCurrentProcessId())+"-"+std::to_string(GetTickCount64())+"-"+std::to_string(++sequence);heartbeat=GetTickCount64();return success({{"epoch",epoch},{"methods",accepted}});
  }
  if(epoch.empty()||!args.contains("epoch")||args["epoch"]!=epoch)return fail("EDITOR_EPOCH_MISMATCH");
  if(method=="editor.next"){if(!registered){if(descriptor.empty()||api->register_capability_provider("editor.lua",dispatch,this,descriptor.c_str())!=LOADER_OK)return fail("EDITOR_REGISTRATION_REJECTED");registered=true;}heartbeat=GetTickCount64();expire_jobs(heartbeat);for(auto &item:jobs)if(!item.second.taken){item.second.taken=true;return success({{"request",item.second.request}});}return success();}
  if(method=="editor.reply"){
   if(!registered)return fail("EDITOR_PROVIDER_OFFLINE");heartbeat=GetTickCount64();expire_jobs(heartbeat);
   if(!args.contains("id")||!args["id"].is_string()||!args.contains("response")||!args["response"].is_object()||!args["response"].contains("ok")||!args["response"]["ok"].is_boolean())return fail("INVALID_ARGUMENT");
   auto it=jobs.find(args["id"].get<std::string>());if(it==jobs.end()||!it->second.taken)return fail("EDITOR_REQUEST_EXPIRED");auto job=std::move(it->second);jobs.erase(it);reply(job,args["response"]);return success();
  }
  return fail("UNSUPPORTED_HOST_CAPABILITY");
 }
};
}
