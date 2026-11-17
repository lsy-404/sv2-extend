#include "../loader/windows/windows_web_retirement.h"
#include <cassert>
using namespace windows_web_retirement;
static void complete(const char *body,size_t size,void *context){*static_cast<json*>(context)=json::parse(body,body+size);}
struct Reentry {Service *service;int status=0;};
static void retry(const char*,size_t,void *context){auto p=static_cast<Reentry*>(context);Observation o;o.requested=true;o.exclusive=true;o.networkGeneration=7;p->status=p->service->start(7,o,retry,context);}
static Observation ready(){Observation o;o.networkGeneration=7;o.leaseGeneration=3;o.now=1000;o.requested=true;o.exclusive=true;o.policyApplied=true;o.gateBlocked=true;o.coverage=true;o.jobKnown=true;o.jobEmpty=true;o.childrenStable=true;return o;}
int main(){
 Service service;json result;auto o=ready();assert(service.start(7,o,nullptr,&result)==LOADER_ERR_INVALID);assert(service.start(8,o,complete,&result)==LOADER_ERR_STALE_GENERATION);
 auto shared=o;shared.exclusive=false;assert(service.start(7,shared,complete,&result)==LOADER_ERR_PROFILE_BUSY);assert(service.start(7,o,complete,&result)==LOADER_OK);assert(service.start(7,o,complete,&result)==LOADER_ERR_PROFILE_BUSY);
 assert(service.may_retire(o));auto changed=o;changed.leaseGeneration++;assert(!service.may_retire(changed));changed=o;changed.networkGeneration++;assert(!service.may_retire(changed));o.pending=true;service.tick(o);assert(result.is_null());o.pending=false;o.controllers=true;service.tick(o);assert(result.is_null());o.controllers=false;o.jobEmpty=false;service.tick(o);assert(result.is_null());o.jobEmpty=true;service.tick(o);o.now+=99;service.tick(o);assert(result.is_null());o.now++;service.tick(o);assert(result["ok"]==true&&!service.active());
 Reentry reentry{&service};o=ready();assert(service.start(7,o,retry,&reentry)==LOADER_OK);service.tick(o);o.now+=100;service.tick(o);assert(reentry.status==LOADER_ERR_PROFILE_BUSY&&!service.active());
 result=nullptr;o=ready();assert(service.start(7,o,complete,&result)==LOADER_OK);o.networkGeneration++;service.tick(o);assert(result["error"]["code"]=="STALE_NETWORK_GENERATION");
 result=nullptr;o=ready();assert(service.start(7,o,complete,&result)==LOADER_OK);o.leaseGeneration++;service.tick(o);assert(result["error"]["code"]=="PROFILE_LEASE_CHANGED");
 result=nullptr;o=ready();assert(service.start(7,o,complete,&result)==LOADER_OK);o.coverage=false;service.tick(o);assert(result["error"]["code"]=="WEB_NETWORK_CONTEXT_OBSERVATION_UNAVAILABLE");
 result=nullptr;o=ready();assert(service.start(7,o,complete,&result)==LOADER_OK);o.jobEmpty=false;o.now+=5000;service.tick(o);assert(result["error"]["code"]=="WEB_RENDERER_RETIREMENT_TIMEOUT");
}
