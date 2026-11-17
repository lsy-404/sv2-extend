#pragma once
#include "voice_identity.h"

namespace voice_service {
using json=nlohmann::json;
struct Service {
 DWORD thread;bool installed=false,validated=false;ULONGLONG deadline=0;std::string token;json captured;
 explicit Service(DWORD mainThread):thread(mainThread){}
 static json fail(const char *code){return {{"ok",false},{"error",{{"code",code}}}};}
 static json success(json result=json::object()){return {{"ok",true},{"result",result}};}
 static void observe(void *reference,void *context){auto &self=*static_cast<Service*>(context);json value;if(voice_identity::read(reference,value))self.captured=std::move(value);}
 void clear(){token.clear();deadline=0;captured=json();}
 void reset(){if(GetCurrentThreadId()==thread&&!token.empty()){voice_adapter::finish();clear();}}
 void tick(){if(GetCurrentThreadId()==thread&&!token.empty()&&GetTickCount64()>deadline){voice_adapter::finish();clear();}}
 json call(const std::string &method,const json &args){
  if(GetCurrentThreadId()!=thread)return fail("MAIN_THREAD_REQUIRED");if(!args.is_object())return fail("INVALID_ARGUMENT");tick();
  if(method=="voice.native_status")return success({{"identityReadable",installed&&validated},{"probeAvailable",installed},{"catalogReadable",false},{"selectable",false}});
  if(!installed)return fail("VOICE_NATIVE_ADAPTER_UNAVAILABLE");
  if(method=="voice.capture.begin"){
   if(!args.empty())return fail("INVALID_ARGUMENT");if(!token.empty())return fail("VOICE_CAPTURE_BUSY");
   std::array<unsigned char,16> random{};if(BCryptGenRandom(nullptr,random.data(),static_cast<ULONG>(random.size()),BCRYPT_USE_SYSTEM_PREFERRED_RNG)<0)return fail("VOICE_CAPTURE_UNAVAILABLE");
   static constexpr char hex[]="0123456789abcdef";std::string next(32,'0');for(size_t i=0;i<random.size();++i){next[2*i]=hex[random[i]>>4];next[2*i+1]=hex[random[i]&15];}
   if(!voice_adapter::begin(observe,this))return fail("VOICE_CAPTURE_BUSY");token=std::move(next);captured=json();deadline=GetTickCount64()+500;return success({{"token",token}});
  }
  if(method=="voice.capture.end"){
   if(args.size()!=1||!args.contains("token")||!args["token"].is_string()||args["token"]!=token||token.empty())return fail("VOICE_CAPTURE_TOKEN_MISMATCH");
   bool valid=voice_adapter::finish()&&captured.is_object();auto value=std::move(captured);clear();if(!valid)return fail("VOICE_CAPTURE_MISSING_OR_AMBIGUOUS");validated=true;return success(std::move(value));
  }
  return fail("UNSUPPORTED_HOST_CAPABILITY");
 }
};
}
