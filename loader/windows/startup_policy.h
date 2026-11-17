#pragma once
#include "plugin_storage.h"
#include "vendor/json.hpp"

namespace startup_policy {
using json=nlohmann::json;
class Service {
 DWORD thread_;
 bool applied_=false;
 static bool name(const char *value,size_t limit){return value&&((value[0]>='A'&&value[0]<='Z')||(value[0]>='a'&&value[0]<='z')||(value[0]>='0'&&value[0]<='9'))&&plugin_storage::component(value,limit,false);}
 static int read(json &value){
  size_t length=0;int result=plugin_storage::read("opensv2.loader","startup-network-policy.json",nullptr,&length);
  if(result==LOADER_ERR_NOT_FOUND){value={{"schema",1},{"generation",uint64_t(0)},{"guards",json::object()}};return LOADER_OK;}
  if(result!=LOADER_OK||length>65536)return LOADER_ERR_POLICY_INVALID;
  std::vector<uint8_t> body(length);result=plugin_storage::read("opensv2.loader","startup-network-policy.json",body.data(),&length);
  if(result!=LOADER_OK)return LOADER_ERR_POLICY_INVALID;value=json::parse(body.begin(),body.end(),nullptr,false);
  if(!value.is_object()||value.size()!=3||!value.contains("schema")||!value["schema"].is_number_unsigned()||value["schema"]!=1||!value.contains("generation")||!value["generation"].is_number_unsigned()||value["generation"].get<uint64_t>()>=9007199254740990ull||!value.contains("guards")||!value["guards"].is_object()||value["guards"].size()>128)return LOADER_ERR_POLICY_INVALID;
  for(auto it=value["guards"].begin();it!=value["guards"].end();++it){auto key=it.key();auto pos=key.find(':');if(pos==std::string::npos||!name(key.substr(0,pos).c_str(),96)||!name(key.substr(pos+1).c_str(),128)||it.value()!=true)return LOADER_ERR_POLICY_INVALID;}
  return LOADER_OK;
 }
public:
 explicit Service(DWORD thread):thread_(thread){}
 void mark_applied(){applied_=true;}
 int state(loader_startup_network_state_v1 *out)const{
  if(GetCurrentThreadId()!=thread_||!out||out->abi_version!=LOADER_ABI_V1||out->struct_size<sizeof *out)return LOADER_ERR_INVALID;
  json value;int result=read(value);*out={LOADER_ABI_V1,sizeof *out,0,1,0,0,0};
  if(result!=LOADER_OK)return result;
  *out={LOADER_ABI_V1,sizeof *out,value["generation"].get<uint64_t>(),value["guards"].empty()?0u:1u,uint32_t(value["guards"].size()),1,applied_?1u:0u};return LOADER_OK;
 }
 int set(const char *owner,const char *guard,int enabled){
  if(GetCurrentThreadId()!=thread_||!name(owner,96)||!name(guard,128)||(enabled!=0&&enabled!=1))return LOADER_ERR_INVALID;
  json value;int result=read(value);if(result!=LOADER_OK)return result;
  std::string key=std::string(owner)+":"+guard;bool exists=value["guards"].contains(key);
  if(exists==(enabled!=0))return LOADER_OK;
  if(enabled){if(value["guards"].size()>=128)return LOADER_ERR_INVALID;value["guards"][key]=true;}else value["guards"].erase(key);
  value["generation"]=value["generation"].get<uint64_t>()+1;auto body=value.dump();
  return plugin_storage::write("opensv2.loader","startup-network-policy.json",reinterpret_cast<const uint8_t*>(body.data()),body.size());
 }
 json list(const char *owner)const{
  if(GetCurrentThreadId()!=thread_||!name(owner,96))return {{"ok",false},{"error",{{"code","INVALID_ARGUMENT"}}}};
  json value;int result=read(value);if(result!=LOADER_OK)return {{"ok",false},{"error",{{"code","STARTUP_POLICY_UNAVAILABLE"}}}};
  json guards=json::array();std::string prefix=std::string(owner)+":";for(auto it=value["guards"].begin();it!=value["guards"].end();++it)if(it.key().rfind(prefix,0)==0)guards.push_back(it.key().substr(prefix.size()));
  return {{"ok",true},{"result",{{"guards",guards}}}};
 }
};
}
