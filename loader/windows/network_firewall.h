#pragma once
#include <windows.h>
#include <netfw.h>
#include <string>
namespace windows_firewall {
inline constexpr GUID rule3_iid={0xb21563ff,0xd696,0x4222,{0xab,0x46,0x4e,0x89,0xb7,0x3a,0xb3,0x4a}};
inline constexpr GUID rule_iid={0xaf230d27,0xbaba,0x4e42,{0xac,0xed,0xf5,0x24,0xf2,0x2c,0xfc,0xe2}};
template<class T> struct Com { T *p=nullptr; ~Com(){if(p)p->Release();} T** out(){return &p;} T* operator->()const{return p;} };
struct Str { BSTR p=nullptr; ~Str(){SysFreeString(p);} BSTR* out(){return &p;} };
inline bool equal(BSTR a,const wchar_t *b){return a && _wcsicmp(a,b)==0;}
inline bool empty_or_any(BSTR s){return !s || !*s || _wcsicmp(s,L"*")==0;}
inline bool expected(INetFwRule *r,const std::wstring &target) {
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
 if (!ok || FAILED(r->QueryInterface(rule3_iid,(void**)extended.out()))) return false;
 Str package,owner,users,remoteUsers,remoteMachines; long flags=0;
 return SUCCEEDED(extended->get_LocalAppPackageId(package.out())) && empty_or_any(package.p) &&
 SUCCEEDED(extended->get_LocalUserOwner(owner.out())) && empty_or_any(owner.p) &&
 SUCCEEDED(extended->get_LocalUserAuthorizedList(users.out())) && empty_or_any(users.p) &&
 SUCCEEDED(extended->get_RemoteUserAuthorizedList(remoteUsers.out())) && empty_or_any(remoteUsers.p) &&
 SUCCEEDED(extended->get_RemoteMachineAuthorizedList(remoteMachines.out())) && empty_or_any(remoteMachines.p) &&
 SUCCEEDED(extended->get_SecureFlags(&flags)) && flags==0;
}
inline HRESULT find(INetFwPolicy2 *policy,const std::wstring &target,INetFwRule **output){
 *output=nullptr;Com<INetFwRules> rules;HRESULT hr=policy->get_Rules(rules.out());if(FAILED(hr))return hr;
 Com<IUnknown> enumeration;hr=rules->get__NewEnum(enumeration.out());if(FAILED(hr))return hr;
 Com<IEnumVARIANT> it;hr=enumeration->QueryInterface(IID_IEnumVARIANT,(void**)it.out());if(FAILED(hr))return hr;
 Com<INetFwRule> found;unsigned matches=0;VARIANT value;VariantInit(&value);ULONG fetched=0;
 while((hr=it->Next(1,&value,&fetched))==S_OK){Com<INetFwRule> rule;if(value.vt==VT_DISPATCH&&value.pdispVal&&SUCCEEDED(value.pdispVal->QueryInterface(rule_iid,(void**)rule.out()))){Str name;if(SUCCEEDED(rule->get_Name(name.out()))&&equal(name.p,L"SV2 Extend Managed Host Outbound")){++matches;if(matches>1||!expected(rule.p,target)){VariantClear(&value);return HRESULT_FROM_WIN32(ERROR_INVALID_DATA);}found.p=rule.p;rule.p=nullptr;}}VariantClear(&value);}
 if(FAILED(hr))return hr;if(matches!=1)return HRESULT_FROM_WIN32(ERROR_NOT_FOUND);*output=found.p;found.p=nullptr;return S_OK;
}
}
