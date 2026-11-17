#include "../loader/windows/export_native_service.h"
#include <cassert>
int main(){using namespace export_service;
 std::array<int,4> values={1,1,2,1},applied{};assert(export_native::apply_choices(values,[&](size_t i,int id){applied[i]=id;return true;}));assert(applied==values);size_t calls=0;assert(!export_native::apply_choices(values,[&](size_t i,int){++calls;return i!=1;}));assert(calls==2);

 Job unseen;assert(observe(unseen,false,false,true,false,false)==Progress::waiting&&!unseen.native_done);
 Service service;Job changed;changed.phase="done";changed.error="EXPORT_PROJECT_CHANGED";changed.config_restored=false;assert(service.view(changed)["configRestored"]==false);Job unmodified;assert(service.view(unmodified)["configRestored"]==true);
 Job normal;assert(observe(normal,false,false,false,false,false)==Progress::waiting&&!normal.native_done);assert(observe(normal,true,true,false,false,false)==Progress::waiting);assert(observe(normal,false,true,true,false,false)==Progress::finished&&normal.native_done&&normal.error.empty());
 Job cancel;cancel.cancel_requested=true;assert(observe(cancel,false,false,false,false,false)==Progress::waiting);assert(observe(cancel,true,true,false,false,false)==Progress::request_cancel);cancel.cancel_dispatched=true;assert(observe(cancel,true,true,false,true,false)==Progress::waiting);assert(observe(cancel,false,true,true,true,false)==Progress::finished&&cancel.cancel_seen&&cancel.error.empty());
 Job spontaneous;assert(observe(spontaneous,true,true,false,false,false)==Progress::waiting);assert(observe(spontaneous,false,true,true,true,false)==Progress::finished&&spontaneous.cancel_seen);
 Job delayed;delayed.cancel_requested=true;delayed.cancel_dispatched=true;assert(observe(delayed,false,true,true,false,true)==Progress::waiting&&!delayed.native_done);assert(observe(delayed,false,true,true,false,false)==Progress::finished&&delayed.error=="CANCEL_UNCONFIRMED");
 Job uncertain;uncertain.cancel_requested=true;assert(observe(uncertain,true,true,false,false,false)==Progress::request_cancel);uncertain.cancel_dispatched=true;assert(observe(uncertain,false,true,true,false,false)==Progress::finished&&uncertain.error=="CANCEL_UNCONFIRMED");
}
