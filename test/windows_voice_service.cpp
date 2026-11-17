#include "../loader/windows/voice_service.h"
#include "windows_voice_stub.h"
#include <cassert>
namespace editor_gate { bool version_matches(){return false;} }
using voice_service::json;
int main(){
 voice_service::Service service(GetCurrentThreadId());assert(service.call("voice.native_status",json::object())["result"]["identityReadable"]==false);
 assert(service.call("voice.capture.begin",json::object())["error"]["code"]=="VOICE_NATIVE_ADAPTER_UNAVAILABLE");service.installed=true;
 assert(service.call("voice.select.validate",{{"voicebankId",std::string(64,'a')}})["error"]["code"]=="VOICE_CAPTURE_REQUIRED");
 auto begin=service.call("voice.capture.begin",json::object());assert(begin["ok"]);auto token=begin["result"]["token"];
 assert(service.call("voice.capture.begin",json::object())["error"]["code"]=="VOICE_CAPTURE_BUSY");
 assert(service.call("voice.capture.end",{{"token","wrong"}})["error"]["code"]=="VOICE_CAPTURE_TOKEN_MISMATCH");
 assert(service.call("voice.select.end",{{"token",token}})["error"]["code"]=="VOICE_CAPTURE_MODE_MISMATCH");
 unsigned char reference[0xa8]{};const char *fields[]={"","en","","backend","1"};for(size_t i=0;i<5;i++)memcpy(reference+0x80+i*8,&fields[i],8);
 voice_adapter::stubDelay=550;voice_adapter::deliver(reference);voice_adapter::stubDelay=0;auto end=service.call("voice.capture.end",{{"token",token}});assert(end["ok"]&&end["result"]["assigned"]==false);assert(service.captured.is_null()&&service.token.empty());
 assert(service.call("voice.select.begin",{{"voicebankId","bad"},{"expectedIdentityKey",std::string(64,'a')},{"expectedReferenceKey",std::string(64,'b')}})["error"]["code"]=="INVALID_ARGUMENT");
 auto status=service.call("voice.native_status",json::object());assert(status["result"]["identityReadable"]==true&&status["result"]["selectable"]==false&&status["result"]["catalogReadable"]==false);
 assert(voice_adapter::begin(voice_service::Service::observe,&service,true));voice_adapter::deliver(reference);voice_adapter::deliver(reference);assert(voice_adapter::finish());
 begin=service.call("voice.capture.begin",json::object());token=begin["result"]["token"];voice_adapter::deliver(reference);voice_adapter::deliver(reference);end=service.call("voice.capture.end",{{"token",token}});assert(!end["ok"]&&end["error"]["code"]=="VOICE_CAPTURE_MISSING_OR_AMBIGUOUS");assert(service.captured.is_null());
 begin=service.call("voice.capture.begin",json::object());assert(begin["ok"]);voice_adapter::deliver(reference);service.deadline=GetTickCount64()-1;service.tick();assert(service.token.empty()&&service.captured.is_null()&&!voice_adapter::stubObserver);
}
