#include <windows.h>
#include <string>
#include "../sdk/include/loader_api.h"
extern "C" __declspec(dllexport) int loader_plugin_init_v1(const loader_api_v1*) {
 wchar_t path[32768];DWORD length=GetEnvironmentVariableW(L"SV2_TEST_MAIN_MARKER",path,32768);if(!length||length>=32768)return LOADER_ERR_INVALID;
 HANDLE marker=CreateFileW(path,GENERIC_WRITE,FILE_SHARE_READ,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr);if(marker==INVALID_HANDLE_VALUE)return LOADER_ERR_INVALID;
 const char armed[]="armed";DWORD written=0;if(!WriteFile(marker,armed,sizeof(armed)-1,&written,nullptr)||written!=sizeof(armed)-1)return LOADER_ERR_INVALID;
 auto encoded=std::to_wstring(reinterpret_cast<uintptr_t>(marker));if(!SetEnvironmentVariableW(L"SV2_TEST_MAIN_HANDLE",encoded.c_str()))return LOADER_ERR_INVALID;
#ifdef FATAL_STARTUP
 return LOADER_ERR_STORAGE_IO;
#else
 return LOADER_OK;
#endif
}
