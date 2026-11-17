#pragma once
#include <windows.h>
#include <sddl.h>
#include <string>
#include <vector>
namespace executable_integrity {
inline bool medium(const std::wstring &path){
 PSECURITY_DESCRIPTOR descriptor=nullptr;if(!ConvertStringSecurityDescriptorToSecurityDescriptorW(L"S:(ML;;NW;;;ME)",SDDL_REVISION_1,&descriptor,nullptr))return false;
 bool ok=SetFileSecurityW(path.c_str(),LABEL_SECURITY_INFORMATION,descriptor)!=FALSE;LocalFree(descriptor);if(!ok)return false;
 DWORD needed=0;GetFileSecurityW(path.c_str(),LABEL_SECURITY_INFORMATION,nullptr,0,&needed);if(!needed)return false;std::vector<unsigned char> buffer(needed);
 if(!GetFileSecurityW(path.c_str(),LABEL_SECURITY_INFORMATION,buffer.data(),needed,&needed))return false;
 LPWSTR text=nullptr;if(!ConvertSecurityDescriptorToStringSecurityDescriptorW(buffer.data(),SDDL_REVISION_1,LABEL_SECURITY_INFORMATION,&text,nullptr))return false;
 ok=std::wstring(text)==L"S:(ML;;NW;;;ME)";LocalFree(text);return ok;
}
}
