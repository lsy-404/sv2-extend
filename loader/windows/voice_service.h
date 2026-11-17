#pragma once
#include "voice_catalog_native.h"
namespace voice_service {
using json=nlohmann::json;
struct Service {
 DWORD thread;bool installed=false,validated=false,catalogValidated=false;ULONGLONG deadline=0;std::string token;json captured,selection,result;
 explicit Service(DWORD mainThread):thread(mainThread){}
 static json fail(const char *code){return {{"ok",false},{"error",{{"code",code}}}};}
 static json success(json result=json::object()){return {{"ok",true},{"result",result}};}
 static bool key(const json &value){if(!value.is_string())return false;auto text=value.get<std::string>();return text.size()==64&&text.find_first_not_of("0123456789abcdef")==std::string::npos;}
 static void apply(void *reference,Service &self,const json &before){
  if(before.value("referenceKey",std::string())!=self.selection["expectedReferenceKey"]||before.value("identityKey",std::string())!=self.selection["expectedIdentityKey"]){self.result=fail("STALE_VOICE_REFERENCE");return;}
  auto image=reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr));if(!image||!voice_catalog_native::supported(image)){self.result=fail("VOICE_CATALOG_UNAVAILABLE");return;}
  voice_catalog_native::Database target(image);json catalog;
  if(!voice_catalog_native::probe(self.thread,catalog,self.selection["voicebankId"].get<std::string>(),&target)){self.result=fail("VOICEBANK_UNAVAILABLE");return;}
  json desired;if(!target.identity(desired)){self.result=fail("VOICE_CATALOG_INVALID");return;}
  if(before["identityKey"]==desired["identityKey"]){self.result=success({{"voicebank",before},{"undoRecordCreated",false}});return;}
  uintptr_t fields[5]{};if(!voice_identity::read_memory(static_cast<char*>(reference)+0x80,fields,sizeof fields)){self.result=fail("VOICE_REFERENCE_UNREADABLE");return;}
  voice_catalog_native::Database previous(image);previous.copy_from(fields);
  auto invoke=[&](const voice_catalog_native::Database &source,bool undo){voice_catalog_native::Database argument(image);argument.copy_from(source.fields);argument.transfer();reinterpret_cast<void(*)(void*,void*,bool)>(image+0xad62a0)(reference,argument.fields,undo);};
  bool changed=false;try{invoke(target,true);json after;if(voice_identity::read(reference,after)&&after["identityKey"]==desired["identityKey"]){self.result=success({{"voicebank",after},{"undoRecordCreated",true}});return;}changed=true;}catch(...){changed=true;}
  bool restored=false;if(changed)try{invoke(previous,false);json after;restored=voice_identity::read(reference,after)&&after["identityKey"]==before["identityKey"];}catch(...){restored=false;}
  self.result=fail(restored?"VOICE_CHANGE_FAILED_ROLLED_BACK":"VOICE_CHANGE_ROLLBACK_FAILED");self.result["error"]["rolledBack"]=restored;self.result["error"]["undoRecordMayExist"]=true;
 }
 static void observe(void *reference,void *context){auto &self=*static_cast<Service*>(context);json value;if(voice_identity::read(reference,value)){if(self.selection.is_object())apply(reference,self,value);else self.captured=std::move(value);}self.deadline=GetTickCount64()+15000;}
 void clear(){token.clear();deadline=0;captured=json();selection=json();result=json();}
 void reset(){if(GetCurrentThreadId()!=thread||voice_adapter::callback_active())return;if(!token.empty())voice_adapter::finish();clear();validated=false;catalogValidated=false;}
 void tick(){if(GetCurrentThreadId()==thread&&!token.empty()&&!voice_adapter::callback_active()&&GetTickCount64()>deadline){voice_adapter::finish();clear();}}
 json call(const std::string &method,const json &args){
  if(GetCurrentThreadId()!=thread)return fail("MAIN_THREAD_REQUIRED");if(!args.is_object())return fail("INVALID_ARGUMENT");tick();
  if(method=="voice.native_status")return success({{"identityReadable",installed&&validated},{"probeAvailable",installed},{"catalogReadable",installed&&validated&&catalogValidated},{"selectable",installed&&validated&&catalogValidated}});
  if(!installed)return fail("VOICE_NATIVE_ADAPTER_UNAVAILABLE");
  if(method=="voice.catalog"||method=="voice.select.validate"){
   if(!validated)return fail("VOICE_CAPTURE_REQUIRED");if(method=="voice.catalog"&&!args.empty())return fail("INVALID_ARGUMENT");
   if(method=="voice.select.validate"&&(args.size()!=1||!args.contains("voicebankId")||!key(args["voicebankId"])))return fail("INVALID_ARGUMENT");
   json catalog;if(!voice_catalog_native::probe(thread,catalog)){catalogValidated=false;return fail("VOICE_CATALOG_UNAVAILABLE");}catalogValidated=true;
   if(method=="voice.catalog")return success(catalog);
   for(const auto &voice:catalog["voicebanks"])if(voice["voicebankId"]==args["voicebankId"])return success(voice);return fail("VOICEBANK_UNAVAILABLE");
  }
  bool selecting=method=="voice.select.begin";
  if(method=="voice.capture.begin"||selecting){
   if(!selecting&&!args.empty())return fail("INVALID_ARGUMENT");
   if(selecting&&(args.size()!=3||!args.contains("voicebankId")||!key(args["voicebankId"])||!args.contains("expectedIdentityKey")||!key(args["expectedIdentityKey"])||!args.contains("expectedReferenceKey")||!key(args["expectedReferenceKey"])))return fail("INVALID_ARGUMENT");
   if(selecting){auto found=call("voice.select.validate",{{"voicebankId",args["voicebankId"]}});if(!found["ok"])return found;}
   if(!token.empty())return fail("VOICE_CAPTURE_BUSY");
   std::array<unsigned char,16> random{};if(BCryptGenRandom(nullptr,random.data(),static_cast<ULONG>(random.size()),BCRYPT_USE_SYSTEM_PREFERRED_RNG)<0)return fail("VOICE_CAPTURE_UNAVAILABLE");
   static constexpr char hex[]="0123456789abcdef";std::string next(32,'0');for(size_t i=0;i<random.size();++i){next[2*i]=hex[random[i]>>4];next[2*i+1]=hex[random[i]&15];}
   if(!voice_adapter::begin(observe,this,selecting))return fail("VOICE_CAPTURE_BUSY");token=std::move(next);captured=json();result=json();selection=selecting?args:json();deadline=GetTickCount64()+500;return success({{"token",token}});
  }
  bool selectionEnd=method=="voice.select.end";
  if(method=="voice.capture.end"||selectionEnd){
   if(voice_adapter::callback_active())return fail("VOICE_CAPTURE_BUSY");
   if(args.size()!=1||!args.contains("token")||!args["token"].is_string()||args["token"]!=token||token.empty())return fail("VOICE_CAPTURE_TOKEN_MISMATCH");
   if(selectionEnd!=selection.is_object())return fail("VOICE_CAPTURE_MODE_MISMATCH");
   bool valid=voice_adapter::finish()&&(selectionEnd?result.is_object():captured.is_object());auto value=selectionEnd?std::move(result):success(std::move(captured));clear();
   if(!valid)return fail("VOICE_CAPTURE_MISSING_OR_AMBIGUOUS");if(!selectionEnd){validated=true;json catalog;catalogValidated=voice_catalog_native::probe(thread,catalog);}return value;
  }
  return fail("UNSUPPORTED_HOST_CAPABILITY");
 }
};
}
