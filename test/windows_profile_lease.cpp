#include <cassert>
#include <thread>
#include "../loader/windows/profile_lease.h"
int main(){
 using profile_lease::Service;auto main=GetCurrentThreadId();loader_profile_lease_state_v1 state{LOADER_ABI_V1,sizeof state,0,0,0};
 Service absent(main,false);assert(absent.state(&state)==LOADER_ERR_NOT_SUPPORTED&&state.mode==LOADER_PROFILE_LEASE_NONE&&state.generation==0);assert(absent.acquire("owner","op",false)==LOADER_ERR_NOT_SUPPORTED);
 Service service(main,true);state={LOADER_ABI_V1,sizeof state,0,0,0};assert(service.state(&state)==LOADER_OK&&state.mode==LOADER_PROFILE_LEASE_SHARED&&state.generation==1);
 assert(service.acquire("owner","op",true)==LOADER_ERR_PROFILE_BUSY);assert(!service.exclusive());assert(service.acquire("owner","op",false)==LOADER_OK&&service.exclusive());
 assert(service.acquire("owner","op",true)==LOADER_OK);assert(service.acquire("other","op",false)==LOADER_ERR_PROFILE_BUSY);assert(service.acquire("owner","other",false)==LOADER_ERR_PROFILE_BUSY);
 assert(service.state(&state)==LOADER_OK&&state.generation==2&&state.mode==LOADER_PROFILE_LEASE_EXCLUSIVE);assert(service.release("other","op")==LOADER_ERR_INVALID&&service.exclusive());assert(service.release("owner","op")==LOADER_OK&&!service.exclusive());assert(service.release("owner","op")==LOADER_OK);assert(service.release("other","op")==LOADER_OK);
 assert(service.state(&state)==LOADER_OK&&state.generation==3&&state.mode==LOADER_PROFILE_LEASE_SHARED);assert(service.acquire(".bad","op",false)==LOADER_ERR_INVALID);assert(service.acquire("owner","",false)==LOADER_ERR_INVALID);assert(service.acquire("owner space","op",false)==LOADER_ERR_INVALID);
 int result=0;std::thread worker([&]{result=service.acquire("owner","op",false);});worker.join();assert(result==LOADER_ERR_INVALID);state.abi_version=99;assert(service.state(&state)==LOADER_ERR_INVALID);
}
