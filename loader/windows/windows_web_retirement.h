#pragma once
#include <windows.h>
#include <cstdint>
#include "../../sdk/include/loader_api.h"
#include "vendor/json.hpp"
namespace windows_web_retirement {
using json=nlohmann::json;
struct Observation {
 uint64_t networkGeneration=0,leaseGeneration=0,now=0;
 bool requested=false,policyApplied=false,policyPending=false,policyError=false,exclusive=false;
 bool gateBlocked=false,coverage=false,pending=false,controllers=false,jobKnown=false,jobEmpty=false,childrenStable=false;
};
class Service {
 bool active_=false,finishing_=false;uint64_t network_=0,lease_=0,deadline_=0,empty_since_=0;loader_json_completion completion_=nullptr;void *context_=nullptr;
 void finish(const char *error){
  auto done=completion_;auto context=context_;finishing_=true;completion_=nullptr;context_=nullptr;
  json value=error?json{{"ok",false},{"error",{{"code",error}}}}:json{{"ok",true},{"result",{{"hostPid",GetCurrentProcessId()},{"networkGeneration",network_},{"profileLeaseGeneration",lease_},{"renderersExited",true},{"networkProcessesExited",true},{"websiteDataDeleted",false},{"wholeHostNetworkVerified",false}}}};
  auto body=value.dump();try{done(body.data(),body.size(),context);}catch(...){finishing_=false;active_=false;throw;}finishing_=false;active_=false;
 }
public:
 bool active()const{return active_;}
 bool may_retire(const Observation &o)const{return active_&&o.requested&&o.networkGeneration==network_&&o.exclusive&&o.leaseGeneration==lease_&&o.policyApplied&&!o.policyPending&&!o.policyError&&o.gateBlocked&&o.coverage;}
 int start(uint64_t expected,const Observation &o,loader_json_completion completion,void *context){
  if(!completion)return LOADER_ERR_INVALID;if(active_)return LOADER_ERR_PROFILE_BUSY;if(!o.requested)return LOADER_ERR_INVALID;
  if(expected!=o.networkGeneration)return LOADER_ERR_STALE_GENERATION;if(!o.exclusive)return LOADER_ERR_PROFILE_BUSY;
  active_=true;network_=expected;lease_=o.leaseGeneration;deadline_=o.now+5000;empty_since_=0;completion_=completion;context_=context;return LOADER_OK;
 }
 void tick(const Observation &o){
  if(!active_||finishing_)return;if(!o.requested||o.networkGeneration!=network_){finish("STALE_NETWORK_GENERATION");return;}
  if(!o.exclusive||o.leaseGeneration!=lease_){finish("PROFILE_LEASE_CHANGED");return;}
  if(o.now>=deadline_){finish("WEB_RENDERER_RETIREMENT_TIMEOUT");return;}
  if(!o.coverage){finish("WEB_NETWORK_CONTEXT_OBSERVATION_UNAVAILABLE");return;}
  if(!o.jobKnown){finish("WEB_RENDERER_OBSERVATION_UNAVAILABLE");return;}
  if(o.policyError){finish("NETWORK_POLICY_UNAVAILABLE");return;}
  if(!o.policyApplied||o.policyPending||!o.gateBlocked||o.pending||o.controllers||!o.jobEmpty||!o.childrenStable){empty_since_=0;return;}
  if(!empty_since_)empty_since_=o.now;else if(o.now-empty_since_>=100)finish(nullptr);
 }
};
}
