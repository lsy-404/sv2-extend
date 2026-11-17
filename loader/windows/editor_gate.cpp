#include "editor_gate.h"
#include "vendor/minhook/include/MinHook.h"
#include <bcrypt.h>
#include <array>
#include <cstring>
#include <fstream>
#include <vector>

namespace editor_gate {
using Getenv=char *(*)(const char*);
static Getenv original;
static editor_lua::Service *resident;
static Router route=nullptr;
void set_router(Router router){route=router;}
static thread_local std::string response;
static constexpr char prefix[]="__OPEN_SV2_NATIVE_CALL_V1__:";
const char *call_json(const char *body,size_t length,bool javascript){
 if(!body||length>1048576){response=editor_lua::Service::fail("REQUEST_TOO_LARGE").dump();return response.c_str();}
 auto request=editor_lua::json::parse(body,body+length,nullptr,false);
 if(!request.is_object()||!request.contains("method")||!request["method"].is_string())response=editor_lua::Service::fail("INVALID_ARGUMENT").dump();
 else{auto method=request["method"].get<std::string>();auto args=request.value("arguments",editor_lua::json::object());if(!args.is_object())response=editor_lua::Service::fail("INVALID_ARGUMENT").dump();else if(length>262144&&method!="editor.reply")response=editor_lua::Service::fail("REQUEST_TOO_LARGE").dump();else response=(route?route(method,args,javascript):resident?resident->call(method,args):editor_lua::Service::fail("UNSUPPORTED_HOST_CAPABILITY")).dump();}
 return response.c_str();
}
static char *intercept(const char *name){
 if(!name||strncmp(name,prefix,sizeof(prefix)-1)!=0)return original(name);
 const char *body=name+sizeof(prefix)-1;return const_cast<char*>(call_json(body,strnlen(body,1048577),false));
}

bool version_matches(){
 wchar_t path[32768];DWORD length=GetModuleFileNameW(nullptr,path,32768);if(!length||length>=32768)return false;
 HANDLE file=CreateFileW(path,GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);if(file==INVALID_HANDLE_VALUE)return false;
 BCRYPT_ALG_HANDLE algorithm=nullptr;BCRYPT_HASH_HANDLE hash=nullptr;DWORD objectSize=0,got=0;std::array<unsigned char,32> digest{};bool ok=false;
 if(BCryptOpenAlgorithmProvider(&algorithm,BCRYPT_SHA256_ALGORITHM,nullptr,0)>=0 && BCryptGetProperty(algorithm,BCRYPT_OBJECT_LENGTH,reinterpret_cast<PUCHAR>(&objectSize),sizeof objectSize,&got,0)>=0){
  std::vector<unsigned char> object(objectSize),buffer(65536);
  if(BCryptCreateHash(algorithm,&hash,object.data(),objectSize,nullptr,0,0)>=0){
   bool readOk=true;DWORD read=0;do{if(!ReadFile(file,buffer.data(),static_cast<DWORD>(buffer.size()),&read,nullptr)){readOk=false;break;}if(read&&BCryptHashData(hash,buffer.data(),read,0)<0){readOk=false;break;}}while(read);
   static constexpr std::array<unsigned char,32> expected={0x37,0x59,0x2b,0x7d,0x3c,0x41,0xff,0xaf,0x04,0x91,0xb6,0xb3,0x35,0xee,0x63,0x0a,0x34,0xf0,0x34,0x16,0xb8,0x5f,0xe9,0x08,0x10,0x21,0xe6,0x98,0x1e,0xb5,0xc2,0xd3};
   ok=readOk && BCryptFinishHash(hash,digest.data(),static_cast<ULONG>(digest.size()),0)>=0 && digest==expected;
   BCryptDestroyHash(hash);hash=nullptr;
  }
 }
 if(hash)BCryptDestroyHash(hash);if(algorithm)BCryptCloseAlgorithmProvider(algorithm,0);CloseHandle(file);return ok;
}
int install(editor_lua::Service *service){
 if(!service || resident || GetCurrentThreadId()!=service->thread || !version_matches())return LOADER_ERR_NOT_SUPPORTED;
 auto base=reinterpret_cast<unsigned char*>(GetModuleHandleW(nullptr));auto dos=reinterpret_cast<IMAGE_DOS_HEADER*>(base);
 if(dos->e_magic!=IMAGE_DOS_SIGNATURE)return LOADER_ERR_NOT_SUPPORTED;auto nt=reinterpret_cast<IMAGE_NT_HEADERS64*>(base+dos->e_lfanew);
 constexpr size_t offset=0x10cc5f8;constexpr unsigned char signature[]={0xe9,0x57,0xfd,0xff,0xff};
 if(nt->Signature!=IMAGE_NT_SIGNATURE||nt->OptionalHeader.SizeOfImage<offset+sizeof signature)return LOADER_ERR_NOT_SUPPORTED;
 MEMORY_BASIC_INFORMATION page{};if(!VirtualQuery(base+offset,&page,sizeof page)||page.State!=MEM_COMMIT||page.Type!=MEM_IMAGE||(page.Protect&0xff)!=PAGE_EXECUTE_READ||memcmp(base+offset,signature,sizeof signature))return LOADER_ERR_NOT_SUPPORTED;
 void *trampoline=nullptr;if(MH_CreateHook(base+offset,reinterpret_cast<void*>(intercept),&trampoline)!=MH_OK)return LOADER_ERR_NOT_SUPPORTED;
 original=reinterpret_cast<Getenv>(trampoline);resident=service;
 if(MH_QueueEnableHook(base+offset)!=MH_OK||MH_ApplyQueued()!=MH_OK){resident=nullptr;MH_RemoveHook(base+offset);original=nullptr;return LOADER_ERR_NOT_SUPPORTED;}
 return LOADER_OK;
}
}
