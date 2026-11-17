#pragma once
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <bcrypt.h>
#include <aclapi.h>
#include <algorithm>
#include <array>
#include <cstring>
#include <filesystem>
#include <mutex>
#include <string>
#include <vector>
#include "../../sdk/include/loader_api.h"

namespace plugin_storage {
namespace fs=std::filesystem;
constexpr size_t limit=1024u*1024u;
inline fs::path profile;
inline std::mutex install_mutex;
struct Handle {
 HANDLE value=INVALID_HANDLE_VALUE;
 Handle()=default;explicit Handle(HANDLE h):value(h){}
 ~Handle(){if(value!=INVALID_HANDLE_VALUE)CloseHandle(value);}
 Handle(const Handle&)=delete;Handle& operator=(const Handle&)=delete;
 Handle(Handle&& other) noexcept:value(other.value){other.value=INVALID_HANDLE_VALUE;}
 Handle& operator=(Handle&& other) noexcept {if(this!=&other){if(value!=INVALID_HANDLE_VALUE)CloseHandle(value);value=other.value;other.value=INVALID_HANDLE_VALUE;}return *this;}
 explicit operator bool() const{return value!=INVALID_HANDLE_VALUE;}
};
inline bool component(const char *s,size_t max,bool id){
 if(!s)return false;size_t n=0;for(;n<=max&&s[n];++n){unsigned char c=s[n];if(!((c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='.'||c=='_'||c=='-'))return false;}
 if(!n||n>max||!strcmp(s,".")||!strcmp(s,"..")||_strnicmp(s,".loader-",8)==0)return false;
 return !id||((s[0]>='a'&&s[0]<='z')||(s[0]>='A'&&s[0]<='Z'));
}
inline std::wstring disk_name(const char *raw,bool id){
 static const wchar_t hex[]=L"0123456789abcdef";std::wstring result;
 for(auto p=reinterpret_cast<const unsigned char*>(raw);*p;++p){if(*p>='A'&&*p<='Z'){result+=L'%';result+=hex[*p>>4];result+=hex[*p&15];}else result+=wchar_t(*p);}
 if(result.size()<=255)return result;
 BCRYPT_ALG_HANDLE alg=nullptr;BCRYPT_HASH_HANDLE hash=nullptr;DWORD object_size=0,got=0;std::array<UCHAR,32> digest{};std::vector<UCHAR> object;bool ok=false;
 if(BCryptOpenAlgorithmProvider(&alg,BCRYPT_SHA256_ALGORITHM,nullptr,0)>=0&&BCryptGetProperty(alg,BCRYPT_OBJECT_LENGTH,reinterpret_cast<PUCHAR>(&object_size),sizeof object_size,&got,0)>=0){object.resize(object_size);if(BCryptCreateHash(alg,&hash,object.data(),object_size,nullptr,0,0)>=0&&BCryptHashData(hash,reinterpret_cast<PUCHAR>(const_cast<char*>(raw)),static_cast<ULONG>(strlen(raw)),0)>=0&&BCryptFinishHash(hash,digest.data(),static_cast<ULONG>(digest.size()),0)>=0)ok=true;}
 if(hash)BCryptDestroyHash(hash);if(alg)BCryptCloseAlgorithmProvider(alg,0);if(!ok)return {};
 result=id?L".loader-id-":L".loader-key-";for(auto byte:digest){result+=hex[byte>>4];result+=hex[byte&15];}return result;
}
inline bool info(HANDLE h,BY_HANDLE_FILE_INFORMATION &out,bool directory){
 if(!GetFileInformationByHandle(h,&out))return false;
 if(out.dwFileAttributes&FILE_ATTRIBUTE_REPARSE_POINT)return false;
 if(directory)return (out.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY)!=0;
 ULARGE_INTEGER size;size.LowPart=out.nFileSizeLow;size.HighPart=out.nFileSizeHigh;
 return !(out.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY)&&out.nNumberOfLinks==1&&size.QuadPart<=limit&&GetFileType(h)==FILE_TYPE_DISK;
}
inline bool same_owner(HANDLE first,HANDLE second){
 PSID a=nullptr,b=nullptr;PSECURITY_DESCRIPTOR da=nullptr,db=nullptr;DWORD x=GetSecurityInfo(first,SE_FILE_OBJECT,OWNER_SECURITY_INFORMATION,&a,nullptr,nullptr,nullptr,&da);DWORD y=GetSecurityInfo(second,SE_FILE_OBJECT,OWNER_SECURITY_INFORMATION,&b,nullptr,nullptr,nullptr,&db);bool result=x==ERROR_SUCCESS&&y==ERROR_SUCCESS&&a&&b&&EqualSid(a,b);if(da)LocalFree(da);if(db)LocalFree(db);return result;
}
inline bool same(const BY_HANDLE_FILE_INFORMATION &a,const BY_HANDLE_FILE_INFORMATION &b){return a.dwVolumeSerialNumber==b.dwVolumeSerialNumber&&a.nFileIndexHigh==b.nFileIndexHigh&&a.nFileIndexLow==b.nFileIndexLow&&a.nFileSizeHigh==b.nFileSizeHigh&&a.nFileSizeLow==b.nFileSizeLow&&CompareFileTime(&a.ftLastWriteTime,&b.ftLastWriteTime)==0;}
inline bool install(const fs::path &root){std::lock_guard<std::mutex> lock(install_mutex);if(root.empty()||!root.is_absolute())return false;profile=root.lexically_normal();return true;}
inline std::wstring native_path(const fs::path &path){auto value=path.wstring();if(value.rfind(L"\\\\?\\",0)==0)return value;if(value.rfind(L"\\\\",0)==0)return L"\\\\?\\UNC\\"+value.substr(2);return L"\\\\?\\"+value;}
inline bool directories(const char *id,fs::path &dir,std::vector<Handle> &held){
 fs::path root;{std::lock_guard<std::mutex> lock(install_mutex);root=profile;}if(root.empty())return false;
 auto encoded=disk_name(id,true);if(encoded.empty())return false;dir=root/L"storage"/L"plugin-data"/encoded;
 fs::path current=dir.root_path();for(const auto &part:dir.relative_path()){
  current/=part;
  if(current==root||current.native().size()<root.native().size()){if(GetFileAttributesW(native_path(current).c_str())==INVALID_FILE_ATTRIBUTES)return false;}else if(!CreateDirectoryW(native_path(current).c_str(),nullptr)&&GetLastError()!=ERROR_ALREADY_EXISTS)return false;
  DWORD access=FILE_READ_ATTRIBUTES|READ_CONTROL;if(current==dir)access|=FILE_ADD_FILE;
  Handle h(CreateFileW(native_path(current).c_str(),access,FILE_SHARE_READ|FILE_SHARE_WRITE,nullptr,OPEN_EXISTING,FILE_FLAG_BACKUP_SEMANTICS|FILE_FLAG_OPEN_REPARSE_POINT,nullptr));BY_HANDLE_FILE_INFORMATION data{};if(!h||!info(h.value,data,true))return false;held.push_back(std::move(h));
 }return true;
}
inline int unconfigured(){std::lock_guard<std::mutex> lock(install_mutex);return profile.empty()?LOADER_ERR_STORAGE_NOT_CONFIGURED:LOADER_ERR_STORAGE_IO;}
struct Namespace {
 fs::path dir;std::vector<Handle> held;Handle lock;OVERLAPPED overlap{};bool locked=false;
 bool open(const char *id){if(!directories(id,dir,held))return false;lock=Handle(CreateFileW(native_path(dir/L".loader-storage.lock").c_str(),GENERIC_READ|GENERIC_WRITE|READ_CONTROL,FILE_SHARE_READ|FILE_SHARE_WRITE,nullptr,OPEN_ALWAYS,FILE_ATTRIBUTE_NORMAL|FILE_FLAG_OPEN_REPARSE_POINT,nullptr));BY_HANDLE_FILE_INFORMATION i{};if(!lock||!info(lock.value,i,false)||!same_owner(lock.value,held.back().value))return false;locked=LockFileEx(lock.value,LOCKFILE_EXCLUSIVE_LOCK,0,1,0,&overlap)!=FALSE;return locked;}
 ~Namespace(){if(locked)UnlockFileEx(lock.value,0,1,0,&overlap);}
};
inline Handle file(const fs::path &path,DWORD access,DWORD share,DWORD creation){return Handle(CreateFileW(native_path(path).c_str(),access|READ_CONTROL,share,nullptr,creation,FILE_ATTRIBUTE_NORMAL|FILE_FLAG_OPEN_REPARSE_POINT,nullptr));}
inline int read(const char *id,const char *key,uint8_t *output,size_t *length){
 if(!component(id,96,true)||!component(key,128,false)||!length)return LOADER_ERR_INVALID;auto name=disk_name(key,false);if(name.empty())return LOADER_ERR_STORAGE_IO;
 Namespace ns;if(!ns.open(id))return unconfigured();auto path=ns.dir/name;auto f=file(path,GENERIC_READ,FILE_SHARE_READ,OPEN_EXISTING);if(!f)return GetLastError()==ERROR_FILE_NOT_FOUND?LOADER_ERR_NOT_FOUND:LOADER_ERR_STORAGE_IO;
 BY_HANDLE_FILE_INFORMATION before{},after{};if(!info(f.value,before,false)||!same_owner(f.value,ns.held.back().value))return LOADER_ERR_STORAGE_IO;size_t needed=before.nFileSizeLow,capacity=*length;*length=needed;if(!output)return LOADER_OK;if(capacity<needed)return LOADER_ERR_BUFFER_TOO_SMALL;
 std::vector<uint8_t> bytes(needed);DWORD read_bytes=0;if(needed&&(!ReadFile(f.value,bytes.data(),static_cast<DWORD>(needed),&read_bytes,nullptr)||read_bytes!=needed))return LOADER_ERR_STORAGE_IO;
 auto named=file(path,FILE_READ_ATTRIBUTES,FILE_SHARE_READ,OPEN_EXISTING);BY_HANDLE_FILE_INFORMATION named_info{};if(!info(f.value,after,false)||!named||!info(named.value,named_info,false)||!same(before,after)||!same(after,named_info))return LOADER_ERR_STORAGE_IO;
 if(needed)memcpy(output,bytes.data(),needed);return LOADER_OK;
}
inline int write(const char *id,const char *key,const uint8_t *data,size_t length){
 if(!component(id,96,true)||!component(key,128,false)||length>limit||(!data&&length))return LOADER_ERR_INVALID;auto name=disk_name(key,false);if(name.empty())return LOADER_ERR_STORAGE_IO;
 Namespace ns;if(!ns.open(id))return unconfigured();auto path=ns.dir/name;
 auto old=file(path,FILE_READ_ATTRIBUTES,FILE_SHARE_READ|FILE_SHARE_DELETE,OPEN_EXISTING);BY_HANDLE_FILE_INFORMATION before{};if(old){if(!info(old.value,before,false)||!same_owner(old.value,ns.held.back().value))return LOADER_ERR_STORAGE_IO;}else if(GetLastError()!=ERROR_FILE_NOT_FOUND)return LOADER_ERR_STORAGE_IO;
 std::array<unsigned char,16> random{};if(BCryptGenRandom(nullptr,random.data(),static_cast<ULONG>(random.size()),BCRYPT_USE_SYSTEM_PREFERRED_RNG)<0)return LOADER_ERR_STORAGE_IO;static const wchar_t hex[]=L"0123456789abcdef";std::wstring temp=L".loader-tmp-";for(auto b:random){temp+=hex[b>>4];temp+=hex[b&15];}auto temp_path=ns.dir/temp;
 auto out=file(temp_path,GENERIC_WRITE|DELETE,0,CREATE_NEW);if(!out)return LOADER_ERR_STORAGE_IO;DWORD written=0;bool ok=(!length||(WriteFile(out.value,data,static_cast<DWORD>(length),&written,nullptr)&&written==length))&&FlushFileBuffers(out.value);
 auto current=file(path,FILE_READ_ATTRIBUTES,FILE_SHARE_READ|FILE_SHARE_DELETE,OPEN_EXISTING);BY_HANDLE_FILE_INFORMATION current_info{};if(old)ok=ok&&current&&info(current.value,current_info,false)&&same_owner(current.value,ns.held.back().value)&&same(before,current_info);else ok=ok&&!current&&GetLastError()==ERROR_FILE_NOT_FOUND;
 if(ok){auto destination=native_path(path);size_t bytes=sizeof(FILE_RENAME_INFO)+destination.size()*sizeof(wchar_t);std::vector<unsigned char> buffer(bytes);struct RenameEx {DWORD flags;HANDLE root;DWORD length;WCHAR name[1];};auto rename=reinterpret_cast<RenameEx*>(buffer.data());rename->flags=old?3u:2u;rename->root=nullptr;rename->length=static_cast<DWORD>(destination.size()*sizeof(wchar_t));memcpy(rename->name,destination.data(),rename->length);ok=SetFileInformationByHandle(out.value,static_cast<FILE_INFO_BY_HANDLE_CLASS>(22),rename,static_cast<DWORD>(bytes))!=FALSE;}
 if(!ok){FILE_DISPOSITION_INFO disposition{TRUE};SetFileInformationByHandle(out.value,FileDispositionInfo,&disposition,sizeof disposition);return LOADER_ERR_STORAGE_IO;}return LOADER_OK;
}
}
