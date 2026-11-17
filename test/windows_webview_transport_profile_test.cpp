#include "../loader/windows/windows_webview_transport.h"
#include <cassert>
#include <fstream>
int wmain(int argc,wchar_t **argv){
 assert(argc==3);SetEnvironmentVariableW(L"WEBVIEW2_ADDITIONAL_BROWSER_ARGUMENTS",L"--no-proxy-server");assert(!windows_webview_transport::configuration_supported());SetEnvironmentVariableW(L"WEBVIEW2_ADDITIONAL_BROWSER_ARGUMENTS",nullptr);
 assert(!windows_webview_transport::prepare_profile(argv[2]));assert(windows_webview_transport::prepare_profile(argv[1]));
 auto p=std::filesystem::path(argv[1])/L"EBWebView"/L"Default"/L"Preferences";nlohmann::json j;{std::ifstream in(p);in>>j;}assert(j["fixture"]==1&&j["webrtc"]["other"]==2&&j["webrtc"]["ip_handling_policy"]=="disable_non_proxied_udp");
 j["fixture"]=3;{std::ofstream out(p);out<<j.dump();}assert(windows_webview_transport::prepare_profile(argv[1]));{std::ifstream in(p);in>>j;}assert(j["fixture"]==3);return 0;
}
