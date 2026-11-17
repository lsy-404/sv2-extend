#pragma once
#include <shlobj.h>
#include <string>
#include <cstring>
#include <atomic>
#include <filesystem>
#include "long_path_adapter.h"
#include "vendor/minhook/include/MinHook.h"

namespace data_namespace {
static std::wstring base;
static std::atomic<uint64_t> redirected{0},other{0};
static std::atomic<int> last_csidl{-1};
static std::atomic<DWORD> last_known{0};
static decltype(&SHGetKnownFolderPath) known_original=SHGetKnownFolderPath;
static decltype(&SHGetFolderPathW) folder_original=SHGetFolderPathW;
static decltype(&SHGetSpecialFolderPathW) special_original=SHGetSpecialFolderPathW;
static std::wstring folder(int id){switch(id&0xff){case CSIDL_APPDATA:return base+L"\\Roaming";case CSIDL_LOCAL_APPDATA:return base+L"\\Local";case CSIDL_COMMON_APPDATA:return base+L"\\ProgramData";default:return {};}}
static HRESULT WINAPI known(REFKNOWNFOLDERID id,DWORD flags,HANDLE token,PWSTR *out){
 std::wstring value;if(IsEqualGUID(id,FOLDERID_RoamingAppData))value=base+L"\\Roaming";else if(IsEqualGUID(id,FOLDERID_LocalAppData))value=base+L"\\Local";else if(IsEqualGUID(id,FOLDERID_LocalAppDataLow))value=base+L"\\LocalLow";else if(IsEqualGUID(id,FOLDERID_ProgramData))value=base+L"\\ProgramData";else{other++;last_known=id.Data1;return known_original(id,flags,token,out);}redirected++;
 if(!out)return E_INVALIDARG;*out=(PWSTR)CoTaskMemAlloc((value.size()+1)*sizeof(wchar_t));if(!*out)return E_OUTOFMEMORY;memcpy(*out,value.c_str(),(value.size()+1)*sizeof(wchar_t));return S_OK;
}
static HRESULT WINAPI path(HWND window,int id,HANDLE token,DWORD flags,LPWSTR out){auto value=folder(id);if(value.empty()){other++;last_csidl=id;return folder_original(window,id,token,flags,out);}redirected++;if(!out||value.size()>=MAX_PATH)return E_INVALIDARG;wcscpy(out,value.c_str());return S_OK;}
static BOOL WINAPI special(HWND window,LPWSTR out,int id,BOOL create){auto value=folder(id);if(value.empty()){other++;last_csidl=id;return special_original(window,out,id,create);}redirected++;if(!out||value.size()>=MAX_PATH)return FALSE;wcscpy(out,value.c_str());return TRUE;}
static bool install(const std::wstring &directory){
 base=directory;MH_STATUS status=MH_Initialize();if(status!=MH_OK)return false;
 HMODULE shell=GetModuleHandleW(L"shell32.dll");if(!shell){MH_Uninitialize();return false;}
 LPVOID targets[]={(LPVOID)GetProcAddress(shell,"SHGetKnownFolderPath"),(LPVOID)GetProcAddress(shell,"SHGetFolderPathW"),(LPVOID)GetProcAddress(shell,"SHGetSpecialFolderPathW")};
 LPVOID replacements[]={(LPVOID)known,(LPVOID)path,(LPVOID)special};
 LPVOID *originals[]={(LPVOID*)&known_original,(LPVOID*)&folder_original,(LPVOID*)&special_original};
 for(unsigned i=0;i<3;i++)if(MH_CreateHook(targets[i],replacements[i],originals[i])!=MH_OK||MH_QueueEnableHook(targets[i])!=MH_OK){MH_Uninitialize();return false;}
 if(MH_ApplyQueued()!=MH_OK){MH_Uninitialize();return false;}if(!long_path_adapter::install(std::filesystem::path(directory).parent_path().wstring())){MH_Uninitialize();return false;}return true;
}
}
