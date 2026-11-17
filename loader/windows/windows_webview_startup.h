#pragma once
#include <cstdint>
namespace windows_webview_startup {
class Proof {
 bool active_=false,used_=false,attested_=false;uint64_t generation_=0;
public:
 class Scope {
  Proof *proof_=nullptr;
 public:
  explicit Scope(Proof &proof){if(!proof.used_){proof.used_=true;proof.active_=true;proof_= &proof;}}
  Scope(const Scope&)=delete;Scope& operator=(const Scope&)=delete;
  ~Scope(){if(proof_)proof_->active_=false;}
 };
 void attest(uint64_t generation,bool requested,bool applied,bool verified){if(active_&&requested&&applied&&verified){attested_=true;generation_=generation;}}
 bool valid(uint64_t generation,bool requested,bool applied,bool policyApplied,bool pending,bool error,bool controllerPresent)const{
  return active_&&attested_&&generation==generation_&&requested&&applied&&policyApplied&&!pending&&!error&&!controllerPresent;
 }
};
}
