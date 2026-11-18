#define WIN32_LEAN_AND_MEAN
#include "../loader/windows/network_control.h"
namespace { windows_network::Controller* resident=nullptr; }
extern "C" __declspec(dllexport) int start_controller(){
 {windows_network::Controller temporary(L"fixture");}
 resident=windows_network::make_resident_controller(reinterpret_cast<const void*>(&start_controller),L"fixture");
 return resident?1:0;
}
extern "C" __declspec(dllexport) int controller_alive(){return resident!=nullptr;}
