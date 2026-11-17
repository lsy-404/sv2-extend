#include "../loader/windows/export_native_service.h"
#include <cassert>
int main(){using namespace export_service;
 Job unseen;assert(observe(unseen,false,false,true,false)==Progress::waiting&&!unseen.native_done);
 Job normal;assert(observe(normal,false,false,false,false)==Progress::waiting&&!normal.native_done);assert(observe(normal,true,true,false,false)==Progress::waiting);assert(observe(normal,false,true,true,false)==Progress::finished&&normal.native_done&&normal.error.empty());
 Job cancel;cancel.cancel_requested=true;assert(observe(cancel,false,false,false,false)==Progress::waiting);assert(observe(cancel,true,true,false,false)==Progress::request_cancel);cancel.cancel_dispatched=true;assert(observe(cancel,true,true,false,true)==Progress::waiting);assert(observe(cancel,false,true,true,true)==Progress::finished&&cancel.cancel_seen&&cancel.error.empty());
 Job spontaneous;assert(observe(spontaneous,true,true,false,false)==Progress::waiting);assert(observe(spontaneous,false,true,true,true)==Progress::finished&&spontaneous.cancel_seen);
 Job uncertain;uncertain.cancel_requested=true;assert(observe(uncertain,true,true,false,false)==Progress::request_cancel);uncertain.cancel_dispatched=true;assert(observe(uncertain,false,true,true,false)==Progress::finished&&uncertain.error=="CANCEL_UNCONFIRMED");
}
