#include <windows.h>
#include <shlobj.h>
#include <initguid.h>
#include <netfw.h>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>
#include <algorithm>

namespace fs = std::filesystem;
template<class T> struct Com { T *p=nullptr; ~Com(){if(p)p->Release();} T** out(){return &p;} T* operator->()const{return p;} };
struct Str { BSTR p=nullptr; ~Str(){SysFreeString(p);} BSTR* out(){return &p;} };
static bool equal(BSTR a,const wchar_t *b){return a && _wcsicmp(a,b)==0;}
static bool empty_or_any(BSTR s){return !s || !*s || _wcsicmp(s,L"*")==0;}
static bool expected(INetFwRule *r,const std::wstring &target) {
 Str name,group,app,service,local,remote,interfaces; NET_FW_RULE_DIRECTION direction; NET_FW_ACTION action; long profiles=0,protocol=0; VARIANT ifaces; VariantInit(&ifaces); VARIANT_BOOL edge=VARIANT_FALSE;
 bool ok=SUCCEEDED(r->get_Name(name.out())) && equal(name.p,L"SV2 Extend Managed Host Outbound") &&
 SUCCEEDED(r->get_Grouping(group.out())) && equal(group.p,L"SV2 Extend Managed Host") &&
 SUCCEEDED(r->get_ApplicationName(app.out())) && equal(app.p,target.c_str()) &&
 SUCCEEDED(r->get_ServiceName(service.out())) && empty_or_any(service.p) &&
 SUCCEEDED(r->get_Direction(&direction)) && direction==NET_FW_RULE_DIR_OUT &&
 SUCCEEDED(r->get_Action(&action)) && action==NET_FW_ACTION_BLOCK &&
 SUCCEEDED(r->get_Profiles(&profiles)) && profiles==NET_FW_PROFILE2_ALL &&
 SUCCEEDED(r->get_Protocol(&protocol)) && protocol==256 &&
 SUCCEEDED(r->get_LocalAddresses(local.out())) && empty_or_any(local.p) &&
 SUCCEEDED(r->get_RemoteAddresses(remote.out())) && empty_or_any(remote.p) &&
 SUCCEEDED(r->get_Interfaces(&ifaces)) && (ifaces.vt==VT_EMPTY || ifaces.vt==VT_NULL) &&
 SUCCEEDED(r->get_InterfaceTypes(interfaces.out())) && (equal(interfaces.p,L"All") || empty_or_any(interfaces.p)) &&
 SUCCEEDED(r->get_EdgeTraversal(&edge)) && edge==VARIANT_FALSE;
 VariantClear(&ifaces);
 Com<INetFwRule3> extended;
 if (!ok || FAILED(r->QueryInterface(IID_INetFwRule3,(void**)extended.out()))) return false;
 Str package,owner,users,remoteUsers,remoteMachines; long flags=0;
 return SUCCEEDED(extended->get_LocalAppPackageId(package.out())) && empty_or_any(package.p) &&
 SUCCEEDED(extended->get_LocalUserOwner(owner.out())) && empty_or_any(owner.p) &&
 SUCCEEDED(extended->get_LocalUserAuthorizedList(users.out())) && empty_or_any(users.p) &&
 SUCCEEDED(extended->get_RemoteUserAuthorizedList(remoteUsers.out())) && empty_or_any(remoteUsers.p) &&
 SUCCEEDED(extended->get_RemoteMachineAuthorizedList(remoteMachines.out())) && empty_or_any(remoteMachines.p) &&
 SUCCEEDED(extended->get_SecureFlags(&flags)) && flags==0;
}
static bool valid_target(const std::wstring &result){
 fs::path p(result); if(!p.is_absolute() || p.lexically_normal()!=p || _wcsicmp(p.filename().c_str(),L"synthv-studio.exe"))return false;
 auto parent=p.parent_path(); if(_wcsicmp(parent.filename().c_str(),L"Synthesizer V Studio 2 Pro"))return false;
 return !_wcsicmp(parent.parent_path().filename().c_str(),L"runtime");
}
static bool target_path(std::wstring &result){
 wchar_t module[32768]; DWORD n=GetModuleFileNameW(nullptr,module,32768); if(!n || n>=32768)return false;
 PWSTR pf=nullptr; if(FAILED(SHGetKnownFolderPath(FOLDERID_ProgramFiles,0,nullptr,&pf)))return false;
 fs::path expected=fs::path(pf)/L"SV2ExtendNetwork"; CoTaskMemFree(pf);
 fs::path base=fs::path(module).parent_path(); if(_wcsicmp(base.c_str(),expected.c_str()))return false;
 std::wifstream in(base/L"target.txt",std::ios::binary); if(!in)return false;
 // The protected file is UTF-16LE and contains one full path without a terminator.
 in.close(); std::ifstream bytes(base/L"target.txt",std::ios::binary); std::vector<unsigned char> data((std::istreambuf_iterator<char>(bytes)),{});
 if(data.size()<4 || data.size()>65534 || data.size()%2 || data[0]!=0xff || data[1]!=0xfe)return false;
 for(size_t i=2;i<data.size();i+=2){wchar_t c=(wchar_t)(data[i]|(data[i+1]<<8));if(!c || c==L'\r' || c==L'\n')return false;result+=c;}
 return valid_target(result);
}
static int apply_rule(bool block,const std::wstring &target){
 Com<INetFwPolicy2> policy; HRESULT hr=CoCreateInstance(CLSID_NetFwPolicy2,nullptr,CLSCTX_INPROC_SERVER,IID_INetFwPolicy2,(void**)policy.out());if(FAILED(hr))return 10;
 NET_FW_MODIFY_STATE modify; if(FAILED(policy->get_LocalPolicyModifyState(&modify)) || modify!=NET_FW_MODIFY_STATE_OK)return 11;
 long active=0;if(FAILED(policy->get_CurrentProfileTypes(&active))||!active)return 12;
 for(long profile: {long(NET_FW_PROFILE2_DOMAIN),long(NET_FW_PROFILE2_PRIVATE),long(NET_FW_PROFILE2_PUBLIC)})if(active&profile){VARIANT_BOOL enabled=VARIANT_FALSE;if(FAILED(policy->get_FirewallEnabled((NET_FW_PROFILE_TYPE2)profile,&enabled)) || enabled!=VARIANT_TRUE)return 13;}
 Com<INetFwRules> rules;if(FAILED(policy->get_Rules(rules.out())))return 14;
 Com<IUnknown> enumeration;if(FAILED(rules->get__NewEnum(enumeration.out())))return 15;
 Com<IEnumVARIANT> it;if(FAILED(enumeration->QueryInterface(IID_IEnumVARIANT,(void**)it.out())))return 15;
 Com<INetFwRule> found; unsigned matches=0;VARIANT v;VariantInit(&v);ULONG fetched=0;
 while(it->Next(1,&v,&fetched)==S_OK){Com<INetFwRule> r;if(v.vt==VT_DISPATCH && v.pdispVal && SUCCEEDED(v.pdispVal->QueryInterface(IID_INetFwRule,(void**)r.out()))){Str name;if(SUCCEEDED(r->get_Name(name.out()))&&equal(name.p,L"SV2 Extend Managed Host Outbound")){++matches;if(!expected(r.p,target)){VariantClear(&v);return 16;}found.p=r.p;r.p=nullptr;}}VariantClear(&v);}
 if(matches!=1)return 17;
 if(FAILED(found->put_Enabled(block?VARIANT_TRUE:VARIANT_FALSE)))return 18;
 VARIANT_BOOL enabled=VARIANT_FALSE;if(FAILED(found->get_Enabled(&enabled)) || (enabled==VARIANT_TRUE)!=block || !expected(found.p,target))return 19;
 return 0;
}
int wmain(int argc,wchar_t **argv){
 SetDefaultDllDirectories(LOAD_LIBRARY_SEARCH_SYSTEM32);
 if(argc!=2 || (wcscmp(argv[1],L"--block") && wcscmp(argv[1],L"--allow")))return 2;
 HRESULT init=CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);if(FAILED(init))return 3;
 std::wstring target; int result=target_path(target)?apply_rule(wcscmp(argv[1],L"--block")==0,target):4;
 CoUninitialize();return result;
}
