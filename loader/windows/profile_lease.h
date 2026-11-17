#pragma once
#include <windows.h>
#include <cstdint>
#include <string>
#include "../../sdk/include/loader_api.h"

namespace profile_lease {
class Service {
 DWORD thread;bool ready;uint32_t mode;uint64_t generation;std::string owner,operation,last_owner,last_operation;
 static bool name(const char *value,size_t limit){
  if(!value||!value[0])return false;
  for(size_t i=0;i<=limit;i++){
   unsigned char c=static_cast<unsigned char>(value[i]);if(!c)return i>0;
   if(i==limit||!((c>='A'&&c<='Z')||(c>='a'&&c<='z')||(c>='0'&&c<='9')||(i&&(c=='.'||c=='_'||c=='-'))))return false;
  }
  return false;
 }
 bool valid(const char *who,const char *what)const{return GetCurrentThreadId()==thread&&name(who,96)&&name(what,128);}
public:
 explicit Service(DWORD main_thread,bool managed_ready):thread(main_thread),ready(managed_ready),mode(managed_ready?LOADER_PROFILE_LEASE_SHARED:LOADER_PROFILE_LEASE_NONE),generation(managed_ready?1:0){}
 int acquire(const char *who,const char *what,bool busy){
  if(!valid(who,what))return LOADER_ERR_INVALID;if(!ready)return LOADER_ERR_NOT_SUPPORTED;
  if(mode==LOADER_PROFILE_LEASE_EXCLUSIVE)return owner==who&&operation==what?LOADER_OK:LOADER_ERR_PROFILE_BUSY;
  if(busy)return LOADER_ERR_PROFILE_BUSY;
  owner=who;operation=what;mode=LOADER_PROFILE_LEASE_EXCLUSIVE;++generation;return LOADER_OK;
 }
 int release(const char *who,const char *what){
  if(!valid(who,what))return LOADER_ERR_INVALID;if(!ready)return LOADER_ERR_NOT_SUPPORTED;
  if(mode==LOADER_PROFILE_LEASE_SHARED)return last_owner==who&&last_operation==what?LOADER_OK:LOADER_ERR_INVALID;
  if(owner!=who||operation!=what)return LOADER_ERR_INVALID;
  last_owner=owner;last_operation=operation;owner.clear();operation.clear();mode=LOADER_PROFILE_LEASE_SHARED;++generation;return LOADER_OK;
 }
 int state(loader_profile_lease_state_v1 *value)const{
  if(GetCurrentThreadId()!=thread||!value||value->abi_version!=LOADER_ABI_V1||value->struct_size<sizeof *value)return LOADER_ERR_INVALID;
  *value={LOADER_ABI_V1,sizeof *value,static_cast<int32_t>(GetCurrentProcessId()),mode,generation};return ready?LOADER_OK:LOADER_ERR_NOT_SUPPORTED;
 }
 bool exclusive()const{return GetCurrentThreadId()==thread&&ready&&mode==LOADER_PROFILE_LEASE_EXCLUSIVE;}
};
}
