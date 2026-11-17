#include "../loader/windows/export_native_service.h"
#include <cassert>
#include <vector>
namespace editor_gate {bool version_matches(){return false;}}
int main(){
 export_service::Service service;std::vector<unsigned char> renderer(0x2e4d3);auto p=renderer.data();void *project=p;
 service.profile_exclusive=true;auto query=[&](){auto result=service.perform("snapshot",export_service::json::object(),"fixture",project,p);assert(result["ok"]==true);return result["result"];};
 auto value=query();assert(value["busy"]==true&&value["profileExclusive"]==true&&value["actualExportBusy"]==false&&value["pending"]==false);
 p[0x2e4d0]=1;value=query();assert(value["busy"]==true&&value["actualExportBusy"]==true);p[0x2e4d0]=0;
 p[0x2e4d2]=1;value=query();assert(value["busy"]==true&&value["actualExportBusy"]==true);p[0x2e4d2]=0;
 p[0x2e4d1]=1;value=query();assert(value["pending"]==true&&value["actualExportBusy"]==false);p[0x2e4d1]=0;
 service.active="job";value=query();assert(value["actualExportBusy"]==true);service.active.clear();
 service.recovery_required=true;value=query();assert(value["recoveryRequired"]==true&&value["actualExportBusy"]==false);
 auto start=service.perform("start",export_service::json::object(),"fixture",project,p);assert(start["error"]["code"]=="PROFILE_BUSY");
 service.profile_exclusive=false;value=query();assert(value["profileExclusive"]==false&&value["busy"]==false&&value["actualExportBusy"]==false);return 0;
}
