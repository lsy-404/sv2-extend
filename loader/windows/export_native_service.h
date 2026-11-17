#pragma once
#include "export_native_adapter.h"
#include "export_wave.h"
#include <bcrypt.h>
#include <map>

namespace export_service {
using json=nlohmann::json;
inline json fail(const char *code){return {{"ok",false},{"error",{{"code",code}}}};}
inline json success(json value=json::object()){return {{"ok",true},{"result",value}};}
inline std::string token(){unsigned char bytes[16];if(BCryptGenRandom(nullptr,bytes,sizeof bytes,BCRYPT_USE_SYSTEM_PREFERRED_RNG)<0)return {};static const char hex[]="0123456789abcdef";std::string out;for(auto b:bytes){out+=hex[b>>4];out+=hex[b&15];}return out;}
inline bool project(void *&p,void *&renderer){if(!export_native::root)return false;auto root=static_cast<unsigned char*>(export_native::root);void *owner=nullptr,*render_owner=nullptr,*render_project=nullptr;if(!export_native::read(root+0x2ae8,p)||!p||!export_native::read(root+0x2a68,owner)||!owner||!export_native::read(static_cast<unsigned char*>(owner)+0x1c8,renderer)||!renderer||!export_native::read(static_cast<unsigned char*>(renderer)+0x10,render_owner)||render_owner!=owner||!export_native::read(static_cast<unsigned char*>(owner)+0x1d8,render_project)||render_project!=p)return false;return true;}
inline bool render_flags(void *renderer,bool &busy,bool &waiting,bool &stopping){unsigned char a=0,b=0,c=0;auto p=static_cast<unsigned char*>(renderer);if(!export_native::read(p+0x2e4d0,a)||!export_native::read(p+0x2e4d1,b)||!export_native::read(p+0x2e4d2,c)||a>1||b>1||c>1)return false;busy=a;waiting=b;stopping=c;return true;}
struct Job {std::string id,project_context;void *project=nullptr;std::filesystem::path output,stage;int channels=0,rate=0,bits=0;std::string phase="opening",state="running",error,error_reason;ULONGLONG deadline=0;bool path_owned=false,format_owned=false,config_restored=true,restore_requested=false,busy_seen=false,cancel_requested=false,cancel_dispatched=false,cancel_seen=false,native_done=false;export_wave::Audio audio{};};
enum class Progress {waiting,request_cancel,finished};
inline Progress observe(Job &job,bool busy,bool prepared,bool finished,bool cancelled,bool pending){if(prepared)job.busy_seen=true;if(cancelled)job.cancel_seen=true;if(finished&&job.busy_seen&&!pending){job.native_done=true;if(job.cancel_requested&&!job.cancel_seen&&job.error.empty())job.error="CANCEL_UNCONFIRMED";return Progress::finished;}if(job.cancel_requested&&!job.cancel_dispatched&&busy&&job.busy_seen)return Progress::request_cancel;return Progress::waiting;}
struct Service {
 std::map<std::string,Job> jobs;std::string active,guard,guard_context,capture_token,capture_kind,capture_context;void *guard_project=nullptr,*capture_project=nullptr;json capture_args;ULONGLONG guard_time=0,capture_time=0;bool available=false,recovery_required=false,profile_exclusive=false;
 json view(const Job &job) const{json value={{"jobId",job.id},{"state",job.state},{"configRestored",job.config_restored}};if(!job.error.empty()){value["error"]={{"code",job.error}};if(!job.error_reason.empty())value["error"]["reason"]=job.error_reason;}if(job.state=="completed")value["audio"]={{"channels",job.audio.channels},{"sampleRate",job.audio.rate},{"bitDepth",job.audio.bits},{"audioBytes",job.audio.audio_bytes},{"fileBytes",job.audio.file_bytes},{"durationSeconds",double(job.audio.audio_bytes)/(job.audio.rate*job.audio.channels*(job.audio.bits/8))}};return value;}
 void finish_error(Job &job,const char *error){if(job.error.empty())job.error=error;job.phase="restore";job.deadline=GetTickCount64()+5000;}
 bool restore(Job &job){
  if(!job.restore_requested){if(job.path_owned){auto r=export_native::restore_path();if(r.contains("error")){job.error="CONFIG_RESTORE_FAILED";job.phase="done";return false;}}if(job.format_owned){auto r=export_native::restore_format();if(r.contains("error")){job.error="CONFIG_RESTORE_FAILED";job.phase="done";return false;}}job.restore_requested=true;}
  if(job.path_owned){auto s=export_native::path_status();if(s.contains("error")){job.error="CONFIG_RESTORE_FAILED";job.phase="done";return false;}if(s["restored"]!=true)return false;auto done=export_native::finish_path();if(done.contains("error")){job.error="CONFIG_RESTORE_FAILED";job.phase="done";return false;}job.path_owned=false;}
  if(job.format_owned){auto s=export_native::format_status();if(s.contains("error")){job.error="CONFIG_RESTORE_FAILED";job.phase="done";return false;}if(s["controlsRestored"]!=true)return false;auto done=export_native::finish_format();if(done.contains("error")){job.error="CONFIG_RESTORE_FAILED";job.phase="done";return false;}job.format_owned=false;}return true;
 }
 void tick(){
  if(!available||GetCurrentThreadId()!=export_native::thread||active.empty())return;auto it=jobs.find(active);if(it==jobs.end()){active.clear();return;}auto &j=it->second;void *p=nullptr,*renderer=nullptr;
  if(j.phase!="done"&&(!project(p,renderer)||p!=j.project)){j.error="EXPORT_PROJECT_CHANGED";j.phase="done";j.state="failed";if(j.path_owned||j.format_owned)recovery_required=true;export_native::unwatch();active.clear();return;}
  bool busy=false,waiting=false,stopping=false;if(j.phase!="done"&&!render_flags(renderer,busy,waiting,stopping)){j.error="RENDER_STATE_UNREADABLE";j.phase="done";j.state="failed";if(j.path_owned||j.format_owned)recovery_required=true;export_native::unwatch();active.clear();return;}
  auto now=GetTickCount64();
  if(j.phase=="opening"){
   if(j.cancel_requested){j.phase="restore";j.deadline=now+5000;}
   else if(export_native::dialog.load()){auto c=export_native::configuration();if(c.contains("error")){finish_error(j,"RENDER_CONTEXT_UNAVAILABLE");}else if(busy||waiting){finish_error(j,"RENDER_BUSY");}else{auto path=export_native::configure_path(j.stage.u8string());j.path_owned=export_native::path_scope.project!=nullptr;if(j.path_owned)j.config_restored=false;if(path.contains("error")){finish_error(j,"EXPORT_PATH_CONFIGURATION_FAILED");}else{auto format=export_native::configure_format(j.channels,j.rate,j.bits);j.format_owned=export_native::format_scope.project!=nullptr;if(j.format_owned)j.config_restored=false;if(format.contains("error"))finish_error(j,"EXPORT_FORMAT_CONFIGURATION_FAILED");else{j.phase="configuring";j.deadline=now+5000;}}}}
   else if(now>=j.deadline)finish_error(j,"RENDER_DIALOG_TIMEOUT");
  }
  if(j.phase=="configuring"){
   if(j.cancel_requested){j.phase="restore";j.deadline=now+5000;}
   else {auto path=export_native::path_status(),format=export_native::format_status();if(path.contains("error")||format.contains("error"))finish_error(j,"EXPORT_CONFIGURATION_FAILED");else if(path["applied"]==true&&format["applied"]==true){export_native::watch(renderer);auto result=export_native::trigger();if(result.contains("error")){export_native::unwatch();finish_error(j,"RENDER_START_REJECTED");}else{j.phase="starting";j.deadline=now+5000;}}else if(now>=j.deadline)finish_error(j,"EXPORT_CONFIGURATION_TIMEOUT");}
  }
  if(j.phase=="starting"||j.phase=="start_unconfirmed"||j.phase=="rendering"){
   auto progress=observe(j,busy,export_native::watched_prepared.load(),export_native::watched_finished.load(),export_native::watched_cancelled.load(),export_native::cancel_pending.load());if(busy||waiting)j.phase="rendering";
   if(progress==Progress::request_cancel){auto cancel=export_native::cancel();if(cancel.contains("error")){j.error="RENDER_CANCEL_REJECTED";if(cancel["error"].is_string())j.error_reason=cancel["error"].get<std::string>();}j.cancel_dispatched=true;}
   else if(progress==Progress::finished){j.phase="restore";j.deadline=now+5000;}
   else if(j.phase=="starting"&&now>=j.deadline){j.error="RENDER_START_UNCONFIRMED";j.state="failed";j.phase="start_unconfirmed";}
  }
  if(j.phase=="restore"){
   if(restore(j)){j.config_restored=true;j.phase="done";if(!j.error.empty())j.state="failed";else if(j.cancel_requested||j.cancel_seen)j.state="cancelled";else if(!j.native_done){j.state="failed";j.error="RENDER_NOT_COMPLETED";}else{auto source=j.stage/L"render_MixDown.wav";auto r=export_wave::publish(source,j.output,j.channels,j.rate,j.bits,j.audio);if(r==export_wave::Result::ok){j.state="completed";RemoveDirectoryW(j.stage.c_str());}else{j.state="failed";j.error=r==export_wave::Result::output_exists?"OUTPUT_EXISTS":r==export_wave::Result::invalid_output||r==export_wave::Result::format_mismatch?"INVALID_RENDER_OUTPUT":"OUTPUT_PUBLISH_FAILED";}}}
   else if(now>=j.deadline&&j.phase!="done"){j.error="CONFIG_RESTORE_FAILED";j.phase="done";}
  }
  if(j.phase=="done"){if(!j.error.empty())j.state="failed";if(j.error=="CONFIG_RESTORE_FAILED")recovery_required=true;export_native::unwatch();active.clear();}
 }
 json perform(const std::string &kind,const json &args,const std::string &context,void *p,void *renderer){
  tick();bool busy=false,waiting=false,stopping=false;if(!render_flags(renderer,busy,waiting,stopping))return fail("RENDER_STATE_UNREADABLE");
  if(kind=="snapshot"){if(!args.empty())return fail("INVALID_ARGUMENT");guard=token();if(guard.empty())return fail("RANDOM_UNAVAILABLE");guard_context=context;guard_project=p;guard_time=GetTickCount64();return success({{"guard",guard},{"guardLifetimeSeconds",15},{"busy",busy||!active.empty()||profile_exclusive},{"pending",waiting},{"formats",json::array({"wav"})},{"sampleRates",json::array({44100,48000,96000})},{"mixdown",true},{"recoveryRequired",recovery_required}});}
  if(kind=="start"){
   if(profile_exclusive)return fail("PROFILE_BUSY");
   if(recovery_required)return fail("EXPORT_RECOVERY_REQUIRED");
   if(args.size()!=5||!args.contains("guard")||!args["guard"].is_string()||!args.contains("outputPath")||!args["outputPath"].is_string()||!args.contains("channels")||!args["channels"].is_number_integer()||!args.contains("sampleRate")||!args["sampleRate"].is_number_integer()||!args.contains("bitDepth")||!args["bitDepth"].is_number_integer())return fail("INVALID_ARGUMENT");
   if(guard.empty()||args["guard"]!=guard||p!=guard_project||context!=guard_context||GetTickCount64()-guard_time>15000)return fail("STALE_EXPORT_GUARD");if(busy||waiting||!active.empty())return fail("RENDER_BUSY");int ch=args["channels"],rate=args["sampleRate"],bits=args["bitDepth"];if((ch!=1&&ch!=2)||(rate!=44100&&rate!=48000&&rate!=96000)||(bits!=16&&bits!=24&&bits!=32))return fail("INVALID_ARGUMENT");
   auto path=args["outputPath"].get<std::string>();if(path.empty()||path.size()>4096||path.find('\0')!=std::string::npos)return fail("INVALID_ARGUMENT");std::wstring wide;if(!native_string::utf16(path.c_str(),wide))return fail("INVALID_ARGUMENT");std::filesystem::path output(wide);auto extension=output.extension().wstring();for(auto &c:extension)c=towlower(c);if(!output.is_absolute()||extension!=L".wav"||output.lexically_normal()!=output)return fail("INVALID_ARGUMENT");DWORD attr=GetFileAttributesW(output.c_str());if(attr!=INVALID_FILE_ATTRIBUTES)return fail("OUTPUT_EXISTS");auto last=GetLastError();if(last!=ERROR_FILE_NOT_FOUND&&last!=ERROR_PATH_NOT_FOUND)return fail("OUTPUT_UNAVAILABLE");auto parent=output.parent_path();attr=GetFileAttributesW(parent.c_str());if(attr==INVALID_FILE_ATTRIBUTES||!(attr&FILE_ATTRIBUTE_DIRECTORY)||(attr&FILE_ATTRIBUTE_REPARSE_POINT))return fail("OUTPUT_DIRECTORY_MISSING");
   if(jobs.size()>=32){for(auto it=jobs.begin();it!=jobs.end();++it)if(it->second.phase=="done"){jobs.erase(it);break;}}if(jobs.size()>=32)return fail("EXPORT_JOB_LIMIT");auto id=token();if(id.empty())return fail("RANDOM_UNAVAILABLE");auto stage=parent/(L".svext-export-"+std::wstring(id.begin(),id.end()));if(!CreateDirectoryW(stage.c_str(),nullptr))return fail("OUTPUT_DIRECTORY_UNWRITABLE");
   if(!export_native::dialog.load()){auto st=export_native::status();if(st["modalActive"]!=0){RemoveDirectoryW(stage.c_str());return fail("RENDER_MODAL_ACTIVE");}auto r=export_native::request();if(r.contains("error")||r.value("accepted",false)==false){RemoveDirectoryW(stage.c_str());return fail("RENDER_COMMAND_REJECTED");}}
   Job j;j.id=id;j.project=p;j.project_context=context;j.output=output;j.stage=stage;j.channels=ch;j.rate=rate;j.bits=bits;j.deadline=GetTickCount64()+5000;jobs.emplace(id,j);active=id;guard.clear();return success(view(jobs.at(id)));
  }
  if((kind!="status"&&kind!="cancel")||args.size()!=1||!args.contains("jobId")||!args["jobId"].is_string())return fail("INVALID_ARGUMENT");auto it=jobs.find(args["jobId"].get<std::string>());if(it==jobs.end())return fail("EXPORT_JOB_NOT_FOUND");auto &j=it->second;if(j.project!=p||j.project_context!=context){if(j.phase!="done"){j.error="EXPORT_PROJECT_CHANGED";j.state="failed";j.phase="done";if(j.path_owned||j.format_owned)recovery_required=true;export_native::unwatch();if(active==j.id)active.clear();}return success(view(j));}if(kind=="cancel"&&j.phase!="done"&&j.phase!="restore"&&!j.native_done){j.cancel_requested=true;j.state="cancelling";}return success(view(j));
 }
 json call(const std::string &method,const json &args){
  if(GetCurrentThreadId()!=export_native::thread)return fail("MAIN_THREAD_REQUIRED");if(!args.is_object())return fail("INVALID_ARGUMENT");if(method=="export.native_status")return success({{"available",available}});if(!available)return fail("EXPORT_NATIVE_UNAVAILABLE");
  if(method=="export.capture.begin"){if(!capture_token.empty()&&GetTickCount64()-capture_time<=500)return fail("EXPORT_CAPTURE_BUSY");if(args.size()!=3||!args.contains("operation")||!args["operation"].is_string()||!args.contains("arguments")||!args["arguments"].is_object()||!args.contains("projectContext")||!args["projectContext"].is_string()||args["projectContext"].get_ref<const std::string&>().size()>16384)return fail("INVALID_ARGUMENT");auto kind=args["operation"].get<std::string>();if(kind!="snapshot"&&kind!="start"&&kind!="status"&&kind!="cancel")return fail("INVALID_ARGUMENT");void *renderer=nullptr;if(!project(capture_project,renderer))return fail("PROJECT_RENDERER_MISMATCH");capture_token=token();if(capture_token.empty())return fail("RANDOM_UNAVAILABLE");capture_kind=kind;capture_context=args["projectContext"];capture_args=args["arguments"];capture_time=GetTickCount64();return success({{"token",capture_token}});}
  if(method=="export.capture.end"){if(args.size()!=1||!args.contains("token")||args["token"]!=capture_token||capture_token.empty())return fail("EXPORT_CAPTURE_TOKEN_MISMATCH");auto kind=capture_kind,context=capture_context;auto values=capture_args;auto expected=capture_project;bool expired=GetTickCount64()-capture_time>500;capture_token.clear();capture_kind.clear();capture_context.clear();capture_args=json();capture_project=nullptr;if(expired)return fail("EXPORT_CAPTURE_EXPIRED");void *p=nullptr,*renderer=nullptr;if(!project(p,renderer)||p!=expected)return fail("PROJECT_RENDERER_MISMATCH");return perform(kind,values,context,p,renderer);}
  return fail("UNSUPPORTED_HOST_CAPABILITY");
 }
};
}
