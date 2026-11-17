#pragma once
#include <windows.h>
#include <mmdeviceapi.h>
#include <audioclient.h>
#include "../loader/windows/vendor/json.hpp"
namespace audio_render_probe {
using json=nlohmann::json;
inline uint32_t code(HRESULT value){return static_cast<uint32_t>(value);}
inline json snapshot(){
 HRESULT initialized=CoInitializeEx(nullptr,COINIT_MULTITHREADED);bool uninitialize=SUCCEEDED(initialized);
 json result={{"coInitialize",code(initialized)}};
 IMMDeviceEnumerator *enumerator=nullptr;IMMDeviceCollection *collection=nullptr;IMMDevice *endpoint=nullptr;IAudioClient *client=nullptr;
 HRESULT create=CoCreateInstance(__uuidof(MMDeviceEnumerator),nullptr,CLSCTX_INPROC_SERVER,__uuidof(IMMDeviceEnumerator),reinterpret_cast<void**>(&enumerator));result["coCreateEnumerator"]=code(create);
 if(SUCCEEDED(create)&&enumerator){
  HRESULT enumerate=enumerator->EnumAudioEndpoints(eRender,DEVICE_STATE_ACTIVE,&collection);result["enumRender"]=code(enumerate);
  if(SUCCEEDED(enumerate)&&collection){
   UINT count=0;HRESULT countResult=collection->GetCount(&count);result["getCount"]=code(countResult);
   if(SUCCEEDED(countResult)){
    result["activeRenderCount"]=count;json activations=json::object();
    for(UINT i=0;i<count;i++){IMMDevice *item=nullptr;IAudioClient *value=nullptr;HRESULT status=collection->Item(i,&item);if(SUCCEEDED(status)&&item)status=item->Activate(__uuidof(IAudioClient),CLSCTX_ALL,nullptr,reinterpret_cast<void**>(&value));auto key=std::to_string(code(status));activations[key]=activations.value(key,0)+1;if(value)value->Release();if(item)item->Release();}
    result["activationCountsByHresult"]=activations;
   }
  }
  HRESULT getDefault=enumerator->GetDefaultAudioEndpoint(eRender,eConsole,&endpoint);result["defaultRender"]=code(getDefault);
  if(SUCCEEDED(getDefault)&&endpoint){HRESULT activate=endpoint->Activate(__uuidof(IAudioClient),CLSCTX_ALL,nullptr,reinterpret_cast<void**>(&client));result["defaultAudioClientActivation"]=code(activate);}
 }
 if(client)client->Release();if(endpoint)endpoint->Release();if(collection)collection->Release();if(enumerator)enumerator->Release();if(uninitialize)CoUninitialize();return result;
}
}
