#pragma once
#include <windows.h>
#include <cstdint>
#include <cstring>
#include <string>
#include "network_path.h"
namespace windows_network_bootstrap {
struct Record {
 uint32_t magic=0,version=0,size=0,pid=0;
 uint64_t creation=0,generation=0;
 uint32_t requested=0,applied=0,verified=0,reserved=0;
 wchar_t image[1024]{};
};
static_assert(sizeof(Record)<3072,"Network handoff must remain bounded");
inline constexpr uint32_t record_magic=0x5356324e;
inline bool process_identity(HANDLE process,uint32_t &pid,uint64_t &creation,std::wstring &image){
 pid=GetProcessId(process);if(!pid)return false;FILETIME created{},exit{},kernel{},user{};
 if(!GetProcessTimes(process,&created,&exit,&kernel,&user))return false;
 creation=(uint64_t(created.dwHighDateTime)<<32)|created.dwLowDateTime;
 wchar_t path[32768];DWORD length=32768;if(!QueryFullProcessImageNameW(process,0,path,&length)||!length||length>=32768)return false;
 return windows_network::canonical_executable_path(std::wstring(path,length),image);
}
class Handoff {
 HANDLE writer_=nullptr,reader_=nullptr;Record *view_=nullptr;bool published_=false;
 void reset(){if(view_)UnmapViewOfFile(view_);if(writer_)CloseHandle(writer_);if(reader_)CloseHandle(reader_);view_=nullptr;writer_=reader_=nullptr;published_=false;}
public:
 Handoff()=default;Handoff(const Handoff&)=delete;Handoff& operator=(const Handoff&)=delete;~Handoff(){reset();}
 bool create(){
  if(writer_||reader_)return false;writer_=CreateFileMappingW(INVALID_HANDLE_VALUE,nullptr,PAGE_READWRITE,0,sizeof(Record),nullptr);if(!writer_)return false;
  view_=static_cast<Record*>(MapViewOfFile(writer_,FILE_MAP_WRITE,0,0,sizeof(Record)));if(!view_){reset();return false;}
  if(!DuplicateHandle(GetCurrentProcess(),writer_,GetCurrentProcess(),&reader_,FILE_MAP_READ,TRUE,0)){reset();return false;}
  Record blank{};std::memcpy(view_,&blank,sizeof blank);return true;
 }
 HANDLE read_handle()const{return reader_;}
 bool publish(HANDLE process,const std::wstring &physical,uint64_t generation,bool requested,bool applied,bool verified){
  if(!view_||published_||!verified||requested!=applied||physical.empty()||physical.size()>=1024)return false;
  uint32_t pid=0;uint64_t creation=0;std::wstring actual;if(!process_identity(process,pid,creation,actual)||CompareStringOrdinal(actual.c_str(),-1,physical.c_str(),-1,TRUE)!=CSTR_EQUAL)return false;
  Record value{};value.magic=record_magic;value.version=1;value.size=sizeof value;value.pid=pid;value.creation=creation;value.generation=generation;value.requested=requested?1u:0u;value.applied=applied?1u:0u;value.verified=1;std::memcpy(value.image,actual.c_str(),(actual.size()+1)*sizeof(wchar_t));
  std::memcpy(view_,&value,sizeof value);published_=true;return true;
 }
 bool seal(){if(!view_||!writer_||!published_||!UnmapViewOfFile(view_))return false;view_=nullptr;if(!CloseHandle(writer_))return false;writer_=nullptr;return true;}
};
inline bool read(HANDLE inherited,Record &output){
 if(!inherited||inherited==INVALID_HANDLE_VALUE)return false;
 const auto view=static_cast<const Record*>(MapViewOfFile(inherited,FILE_MAP_READ,0,0,sizeof(Record)));if(!view)return false;
 Record value{};std::memcpy(&value,view,sizeof value);UnmapViewOfFile(view);
 if(value.magic!=record_magic||value.version!=1||value.size!=sizeof value||value.verified!=1||value.requested>1||value.applied!=value.requested||value.reserved!=0||!wmemchr(value.image,0,1024))return false;
 uint32_t pid=0;uint64_t creation=0;std::wstring actual;if(!process_identity(GetCurrentProcess(),pid,creation,actual)||value.pid!=pid||value.creation!=creation||CompareStringOrdinal(value.image,-1,actual.c_str(),-1,TRUE)!=CSTR_EQUAL)return false;
 output=value;return true;
}
}
