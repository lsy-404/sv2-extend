#pragma once
#include "voice_adapter.h"
#include "vendor/json.hpp"
#include <bcrypt.h>
#include <array>
#include <cstring>
#include <string>
#include <vector>

namespace voice_identity {
using json=nlohmann::json;
inline bool read_memory(const void *value,void *result,size_t length){
 auto address=reinterpret_cast<uintptr_t>(value);if(address<4096||address+length<address)return false;auto end=address+length;
 while(address<end){MEMORY_BASIC_INFORMATION page{};if(!VirtualQuery(reinterpret_cast<void*>(address),&page,sizeof page))return false;DWORD p=page.Protect&0xff;
  if(page.State!=MEM_COMMIT||(page.Protect&PAGE_GUARD)||(p!=PAGE_READONLY&&p!=PAGE_READWRITE&&p!=PAGE_WRITECOPY&&p!=PAGE_EXECUTE_READ&&p!=PAGE_EXECUTE_READWRITE&&p!=PAGE_EXECUTE_WRITECOPY))return false;
  auto start=reinterpret_cast<uintptr_t>(page.BaseAddress);if(start>address||page.RegionSize>UINTPTR_MAX-start)return false;auto next=start+page.RegionSize;if(next<=address)return false;address=next<end?next:end;
 }memcpy(result,value,length);return true;
}
inline bool string_at(const char *pointer,std::string &result){
 if(!pointer)return false;result.clear();for(size_t i=0;i<512;i++){char c;if(!read_memory(pointer+i,&c,1))return false;if(!c){if(result.empty())return true;return MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,result.c_str(),-1,nullptr,0)>0;}result.push_back(c);}return false;
}
inline bool digest(const void *data,size_t length,std::string &result){
 if(length>ULONG_MAX)return false;BCRYPT_ALG_HANDLE algorithm=nullptr;BCRYPT_HASH_HANDLE hash=nullptr;DWORD objectSize=0,got=0;std::array<unsigned char,32> output{};bool ok=false;
 if(BCryptOpenAlgorithmProvider(&algorithm,BCRYPT_SHA256_ALGORITHM,nullptr,0)>=0&&BCryptGetProperty(algorithm,BCRYPT_OBJECT_LENGTH,reinterpret_cast<PUCHAR>(&objectSize),sizeof objectSize,&got,0)>=0){
  std::vector<unsigned char> object(objectSize);if(BCryptCreateHash(algorithm,&hash,object.data(),objectSize,nullptr,0,0)>=0){ok=BCryptHashData(hash,reinterpret_cast<PUCHAR>(const_cast<void*>(data)),static_cast<ULONG>(length),0)>=0&&BCryptFinishHash(hash,output.data(),static_cast<ULONG>(output.size()),0)>=0;BCryptDestroyHash(hash);hash=nullptr;}
 }
 if(hash)BCryptDestroyHash(hash);if(algorithm)BCryptCloseAlgorithmProvider(algorithm,0);if(!ok)return false;
 static constexpr char hex[]="0123456789abcdef";result.resize(64);for(size_t i=0;i<output.size();++i){result[2*i]=hex[output[i]>>4];result[2*i+1]=hex[output[i]&15];}return true;
}
inline bool read(void *reference,json &result){
 if(!voice_adapter::is_observing(reference))return false;
 static constexpr const char *names[]={"name","language","languageOverride","backendType","version"};json value=json::object();
 for(size_t i=0;i<5;i++){const char *pointer=nullptr;std::string text;if(!read_memory(static_cast<unsigned char*>(reference)+0x80+i*8,&pointer,sizeof pointer)||!string_at(pointer,text))return false;value[names[i]]=text;}
 value["assigned"]=!value["name"].get<std::string>().empty();value["source"]="native-reference";
 auto encoded=value.dump();std::string identity;if(!digest(encoded.data(),encoded.size(),identity))return false;value["identityKey"]=identity;
 static std::array<unsigned char,32> salt{};static const bool ready=BCryptGenRandom(nullptr,salt.data(),static_cast<ULONG>(salt.size()),BCRYPT_USE_SYSTEM_PREFERRED_RNG)>=0;if(!ready)return false;
 std::array<unsigned char,32+sizeof(uintptr_t)> input{};memcpy(input.data(),salt.data(),salt.size());auto pointer=reinterpret_cast<uintptr_t>(reference);memcpy(input.data()+salt.size(),&pointer,sizeof pointer);
 std::string key;if(!digest(input.data(),input.size(),key))return false;value["referenceKey"]=key;result=std::move(value);return true;
}
}
