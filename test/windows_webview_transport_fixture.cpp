#include "../loader/windows/windows_webview_transport.h"
#include <windows.h>
#include <cstdio>
#include <atomic>
#include <string>
#include <thread>
#include <filesystem>
#include <map>
#include "../loader/windows/vendor/json.hpp"

struct Completion : IUnknown { virtual HRESULT STDMETHODCALLTYPE Invoke(HRESULT, IUnknown*) = 0; };
static const GUID env_iid={0x4e8a3389,0xc9d8,0x4bd2,{0xb6,0xb5,0x12,0x4f,0xee,0x6c,0xc1,0x4d}};
static const GUID controller_iid={0x6c4819f3,0xc9b7,0x4260,{0x81,0x27,0xc9,0xf5,0xbd,0xe7,0xf6,0x8c}};
using Factory=HRESULT(WINAPI*)(bool,int,PCWSTR,IUnknown*,IUnknown*);
using CreateController=HRESULT(WINAPI*)(IUnknown*,HWND,Completion*);
using GetCore=HRESULT(WINAPI*)(IUnknown*,IUnknown**);
using Navigate=HRESULT(WINAPI*)(IUnknown*,PCWSTR);
using Script=HRESULT(WINAPI*)(IUnknown*,PCWSTR,IUnknown*);
static HWND window_handle; static IUnknown *controller=nullptr,*web=nullptr; static bool env_done=false,controller_done=false,script_done=false; static HRESULT env_result=E_PENDING,controller_result=E_PENDING,script_result=E_PENDING; static std::wstring script_value;
static std::atomic<bool> monitor_stop{false},monitor_done{false},monitor_valid{true};static std::atomic<unsigned> monitor_samples{0},monitor_unknown{0};
static void monitor_images(HANDLE job,const std::wstring &browser){
 struct Finished {~Finished(){monitor_done=true;}} finished;
 wchar_t own[32768];DWORD own_length=GetModuleFileNameW(nullptr,own,32768);if(!own_length||own_length>=32768){monitor_valid=false;return;}
 while(!monitor_stop.load()){
  BYTE buffer[8192]{};auto list=reinterpret_cast<JOBOBJECT_BASIC_PROCESS_ID_LIST*>(buffer);DWORD n=0;
  if(!QueryInformationJobObject(job,JobObjectBasicProcessIdList,list,sizeof buffer,&n)||list->NumberOfProcessIdsInList!=list->NumberOfAssignedProcesses){monitor_valid=false;break;}
  for(DWORD i=0;i<list->NumberOfProcessIdsInList;i++){HANDLE p=OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION,FALSE,(DWORD)list->ProcessIdList[i]);wchar_t path[32768];DWORD size=32768;if(!p||!QueryFullProcessImageNameW(p,0,path,&size)){if(p)CloseHandle(p);monitor_valid=false;break;}CloseHandle(p);if(_wcsicmp(path,own)&&_wcsicmp(path,browser.c_str()))++monitor_unknown;}
  ++monitor_samples;Sleep(50);
 }
}
static bool capture_done=false; static HRESULT capture_result=E_PENDING;
struct CaptureHandler : IUnknown {
 std::atomic<ULONG> refs{1};
 HRESULT STDMETHODCALLTYPE QueryInterface(REFIID,void**o) override {if(!o)return E_POINTER;*o=this;AddRef();return S_OK;}
 ULONG STDMETHODCALLTYPE AddRef() override{return ++refs;} ULONG STDMETHODCALLTYPE Release() override{auto n=--refs;if(!n)delete this;return n;}
 virtual HRESULT STDMETHODCALLTYPE Invoke(HRESULT r){capture_result=r;capture_done=true;return S_OK;}
};
struct ScriptHandler : IUnknown {
 std::atomic<ULONG> refs{1};
 HRESULT STDMETHODCALLTYPE QueryInterface(REFIID,void**o) override {if(!o)return E_POINTER;*o=this;AddRef();return S_OK;}
 ULONG STDMETHODCALLTYPE AddRef() override{return ++refs;} ULONG STDMETHODCALLTYPE Release() override{auto n=--refs;if(!n)delete this;return n;}
 virtual HRESULT STDMETHODCALLTYPE Invoke(HRESULT r,PCWSTR value){script_result=r;script_value=value?value:L"";script_done=true;return S_OK;}
};
struct Handler : Completion {
 std::atomic<ULONG> refs{1}; bool environment; explicit Handler(bool e):environment(e){}
 HRESULT STDMETHODCALLTYPE QueryInterface(REFIID i,void**o) override {if(!o)return E_POINTER;*o=nullptr;if(IsEqualIID(i,IID_IUnknown)||IsEqualIID(i,environment?env_iid:controller_iid)){*o=this;AddRef();return S_OK;}return E_NOINTERFACE;}
 ULONG STDMETHODCALLTYPE AddRef() override{return ++refs;} ULONG STDMETHODCALLTYPE Release() override{auto n=--refs;if(!n)delete this;return n;}
 HRESULT STDMETHODCALLTYPE Invoke(HRESULT r,IUnknown *o) override {
  if(environment){env_result=r;env_done=true;if(SUCCEEDED(r)&&o){auto table=*reinterpret_cast<void***>(o);auto h=new Handler(false);HRESULT s=reinterpret_cast<CreateController>(table[3])(o,window_handle,h);h->Release();if(FAILED(s)){controller_result=s;controller_done=true;}}}
  else{controller_result=r;controller_done=true;if(SUCCEEDED(r)&&o){controller=o;o->AddRef();auto table=*reinterpret_cast<void***>(o);RECT bounds{};GetClientRect(window_handle,&bounds);controller_result=reinterpret_cast<HRESULT(WINAPI*)(IUnknown*,RECT)>(table[6])(o,bounds);if(SUCCEEDED(controller_result))controller_result=reinterpret_cast<GetCore>(table[25])(o,&web);}}
  return S_OK;
 }
};
static bool wait(bool &done,DWORD ms){auto end=GetTickCount64()+ms;while(!done&&GetTickCount64()<end){MSG m;while(PeekMessageW(&m,nullptr,0,0,PM_REMOVE)){TranslateMessage(&m);DispatchMessageW(&m);}Sleep(10);}return done;}
static bool evaluate(PCWSTR expression){script_done=false;auto h=new ScriptHandler;auto table=*reinterpret_cast<void***>(web);HRESULT result=reinterpret_cast<Script>(table[29])(web,expression,h);h->Release();return SUCCEEDED(result)&&wait(script_done,10000)&&SUCCEEDED(script_result);}
static bool cdp_call(PCWSTR method,PCWSTR params){script_done=false;auto h=new ScriptHandler;auto table=*reinterpret_cast<void***>(web);auto call=reinterpret_cast<HRESULT(WINAPI*)(IUnknown*,PCWSTR,PCWSTR,IUnknown*)>(table[36]);HRESULT result=call(web,method,params,h);h->Release();return SUCCEEDED(result)&&wait(script_done,10000)&&SUCCEEDED(script_result)&&script_value.find(L"error")==std::wstring::npos;}
static IUnknown *web11=nullptr;static bool session_offline=false,session_error=false;static unsigned session_pending=0;static std::map<std::wstring,std::string> sessions;
static const GUID web11_iid={0x0be78e56,0xc193,0x4051,{0xb9,0x43,0x23,0xb4,0x60,0xc0,0x8b,0xdb}};
static PCWSTR cdp_params(bool offline){return offline?L"{\"offline\":true,\"latency\":0,\"downloadThroughput\":-1,\"uploadThroughput\":-1}":L"{\"offline\":false,\"latency\":0,\"downloadThroughput\":-1,\"uploadThroughput\":-1}";}
struct SessionHandler : IUnknown {
 std::atomic<ULONG> refs{1};std::wstring session;int stage;bool offline,waiting;
 SessionHandler(std::wstring s,int n,bool o,bool w):session(std::move(s)),stage(n),offline(o),waiting(w){}
 HRESULT STDMETHODCALLTYPE QueryInterface(REFIID,void**o)override{if(!o)return E_POINTER;*o=this;AddRef();return S_OK;}
 ULONG STDMETHODCALLTYPE AddRef()override{return ++refs;}ULONG STDMETHODCALLTYPE Release()override{auto n=--refs;if(!n)delete this;return n;}
 virtual HRESULT STDMETHODCALLTYPE Invoke(HRESULT result,PCWSTR data);
};
static void session_step(const std::wstring &session,int stage,bool offline,bool waiting){
 auto h=new SessionHandler(session,stage,offline,waiting);auto table=*reinterpret_cast<void***>(web11);auto call=reinterpret_cast<HRESULT(WINAPI*)(IUnknown*,PCWSTR,PCWSTR,PCWSTR,IUnknown*)>(table[99]);PCWSTR method=stage==0?L"Network.enable":stage==1||stage==4?L"Network.emulateNetworkConditions":stage==2?L"Target.setAutoAttach":L"Runtime.runIfWaitingForDebugger";PCWSTR params=stage==1||stage==4?cdp_params(offline):stage==2?L"{\"autoAttach\":true,\"waitForDebuggerOnStart\":true,\"flatten\":true}":L"{}";HRESULT result=call(web11,session.c_str(),method,params,h);h->Release();if(FAILED(result)){session_error=true;--session_pending;}
}
HRESULT STDMETHODCALLTYPE SessionHandler::Invoke(HRESULT result,PCWSTR data){
 if(FAILED(result)||(data&&wcsstr(data,L"error"))){wprintf(L"session_error_stage=%d hresult=%08lx data=%ls\n",stage,(unsigned long)result,data?data:L"");fflush(stdout);session_error=true;--session_pending;return S_OK;}
 if(stage==0)session_step(session,1,offline,waiting);else if(stage==1)session_step(session,2,offline,waiting);else if(stage==2&&waiting)session_step(session,3,offline,waiting);else --session_pending;return S_OK;
}
struct TargetHandler : IUnknown {
 std::atomic<ULONG> refs{1};
 HRESULT STDMETHODCALLTYPE QueryInterface(REFIID,void**o)override{if(!o)return E_POINTER;*o=this;AddRef();return S_OK;}
 ULONG STDMETHODCALLTYPE AddRef()override{return ++refs;}ULONG STDMETHODCALLTYPE Release()override{auto n=--refs;if(!n)delete this;return n;}
 virtual HRESULT STDMETHODCALLTYPE Invoke(IUnknown*,IUnknown *args){
  LPWSTR value=nullptr;auto table=*reinterpret_cast<void***>(args);if(FAILED(reinterpret_cast<HRESULT(WINAPI*)(IUnknown*,LPWSTR*)>(table[3])(args,&value))||!value){session_error=true;return S_OK;}
  std::wstring wide(value);CoTaskMemFree(value);std::string data(wide.begin(),wide.end());auto obj=nlohmann::json::parse(data,nullptr,false);if(obj.is_discarded()||!obj.contains("sessionId")||!obj.contains("targetInfo")){session_error=true;return S_OK;}
  std::string id=obj.value("sessionId",std::string()),type=obj["targetInfo"].value("type",std::string());bool waiting=obj.value("waitingForDebugger",false);std::wstring sid(id.begin(),id.end());if(sid.empty()){session_error=true;return S_OK;}sessions[sid]=type;
  wprintf(L"target_type=%hs waiting=%d\n",type.c_str(),waiting);fflush(stdout);++session_pending;session_step(sid,0,session_offline,waiting);return S_OK;
 }
};
static bool wait_sessions(){ULONGLONG end=GetTickCount64()+10000;while(session_pending&&GetTickCount64()<end){MSG m;while(PeekMessageW(&m,nullptr,0,0,PM_REMOVE)){TranslateMessage(&m);DispatchMessageW(&m);}Sleep(10);}return !session_error&&!session_pending;}
static bool attach_targets(){
 if(FAILED(web->QueryInterface(web11_iid,reinterpret_cast<void**>(&web11))))return false;IUnknown *receiver=nullptr;auto table=*reinterpret_cast<void***>(web);if(FAILED(reinterpret_cast<HRESULT(WINAPI*)(IUnknown*,PCWSTR,IUnknown**)>(table[42])(web,L"Target.attachedToTarget",&receiver))||!receiver)return false;
 auto handler=new TargetHandler;long long token=0;auto rtable=*reinterpret_cast<void***>(receiver);HRESULT result=reinterpret_cast<HRESULT(WINAPI*)(IUnknown*,IUnknown*,long long*)>(rtable[3])(receiver,handler,&token);handler->Release();receiver->Release();if(FAILED(result))return false;
 if(!cdp_call(L"Target.getTargets",L"{}"))return false;std::string inventory(script_value.begin(),script_value.end());auto list=nlohmann::json::parse(inventory,nullptr,false);if(list.is_discarded()||!list.contains("targetInfos"))return false;unsigned existing=0;
 for(const auto &target:list["targetInfos"]){std::string type=target.value("type",std::string());if(type!="worker"&&type!="shared_worker"&&type!="service_worker"&&type!="iframe")continue;std::string id=target.value("targetId",std::string());if(id.empty())return false;auto params=nlohmann::json{{"targetId",id},{"flatten",true}}.dump();std::wstring wide(params.begin(),params.end());if(!cdp_call(L"Target.attachToTarget",wide.c_str()))return false;++existing;}wprintf(L"existing_targets=%u\n",existing);fflush(stdout);
 if(!cdp_call(L"Target.setAutoAttach",L"{\"autoAttach\":true,\"waitForDebuggerOnStart\":true,\"flatten\":true}"))return false;return wait_sessions();
}
static bool set_sessions(bool offline){session_offline=offline;for(auto &entry:sessions){++session_pending;session_step(entry.first,4,offline,false);}return wait_sessions();}
static bool set_cdp_network(bool offline,bool emulate){
 script_done=false;auto h=new ScriptHandler;auto table=*reinterpret_cast<void***>(web);auto call=reinterpret_cast<HRESULT(WINAPI*)(IUnknown*,PCWSTR,PCWSTR,IUnknown*)>(table[36]);
 PCWSTR params=offline?L"{\"offline\":true,\"latency\":0,\"downloadThroughput\":-1,\"uploadThroughput\":-1}":L"{\"offline\":false,\"latency\":0,\"downloadThroughput\":-1,\"uploadThroughput\":-1}";
 HRESULT result=call(web,emulate?L"Network.emulateNetworkConditions":L"Network.overrideNetworkState",params,h);h->Release();if(FAILED(result)||!wait(script_done,10000)||FAILED(script_result))return false;wprintf(L"cdp_result=%ls\n",script_value.c_str());fflush(stdout);return script_value.find(L"error")==std::wstring::npos;
}
static bool enable_cdp_network(){script_done=false;auto h=new ScriptHandler;auto table=*reinterpret_cast<void***>(web);auto call=reinterpret_cast<HRESULT(WINAPI*)(IUnknown*,PCWSTR,PCWSTR,IUnknown*)>(table[36]);HRESULT result=call(web,L"Network.enable",L"{}",h);h->Release();if(FAILED(result)||!wait(script_done,10000)||FAILED(script_result))return false;wprintf(L"cdp_enable=%ls\n",script_value.c_str());fflush(stdout);return script_value.find(L"error")==std::wstring::npos;}
static bool capture_preview(PCWSTR path){
 IStream *stream=nullptr;if(FAILED(CreateStreamOnHGlobal(nullptr,TRUE,&stream)))return false;capture_done=false;auto capture=new CaptureHandler;auto table=*reinterpret_cast<void***>(web);HRESULT start=reinterpret_cast<HRESULT(WINAPI*)(IUnknown*,int,IStream*,IUnknown*)>(table[30])(web,0,stream,capture);capture->Release();if(FAILED(start)||!wait(capture_done,10000)||FAILED(capture_result)){stream->Release();return false;}
 HGLOBAL memory=nullptr;STATSTG stat{};if(FAILED(GetHGlobalFromStream(stream,&memory))||FAILED(stream->Stat(&stat,STATFLAG_NONAME))||stat.cbSize.HighPart){stream->Release();return false;}void *bytes=GlobalLock(memory);HANDLE output=CreateFileW(path,GENERIC_WRITE,0,nullptr,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);DWORD written=0;bool ok=bytes&&output!=INVALID_HANDLE_VALUE&&WriteFile(output,bytes,stat.cbSize.LowPart,&written,nullptr)&&written==stat.cbSize.LowPart;if(output!=INVALID_HANDLE_VALUE)CloseHandle(output);if(bytes)GlobalUnlock(memory);stream->Release();return ok;
}
static bool network_observation(int phase){
 const wchar_t *start=LR"JS((() => { window.fixtureNet={http:'pending',http6:'pending',stun:'pending',srflx4:0,srflx6:0,existingTcp:'pending',newTcp:'pending',newTcpPostman:'pending'}; const s=window.fixtureNet; const get=(url,key)=>{const ac=new AbortController();setTimeout(()=>ac.abort(),5000);fetch(url+'?fixture='+Date.now(),{mode:'no-cors',cache:'no-store',signal:ac.signal}).then(()=>s[key]='success').catch(()=>s[key]='failed');};get('https://www.cloudflare.com/cdn-cgi/trace','http');get('https://ipv6.google.com/generate_204','http6');try { const pc=new RTCPeerConnection({iceServers:[{urls:'stun:stun.cloudflare.com:3478'}]}); pc.onicecandidate=e=>{if(e.candidate && e.candidate.type==='srflx'){const a=e.candidate.address||'';if(a.includes(':'))s.srflx6++;else if(a.includes('.'))s.srflx4++;} if(!e.candidate){s.stun='complete';pc.close();}};pc.createDataChannel('fixture');pc.createOffer().then(o=>pc.setLocalDescription(o)).catch(()=>{s.stun='failed';pc.close();});setTimeout(()=>{if(s.stun==='pending'){s.stun='timeout';pc.close();}},6000);}catch(e){s.stun='unavailable';} const probe=(ws,key,close)=>{const token='fixture-'+Math.random();const send=()=>{if(ws.readyState!==WebSocket.OPEN){s[key]='closed';return;}ws.addEventListener('message',e=>{if(e.data===token){s[key]='echo';if(close)ws.close();}});ws.send(token);};if(ws.readyState===WebSocket.CONNECTING)ws.addEventListener('open',send,{once:true});else send();ws.addEventListener('error',()=>{if(s[key]==='pending')s[key]='error';},{once:true});setTimeout(()=>{if(s[key]==='pending'){s[key]='timeout';if(close)ws.close();}},6000);};try {if(!window.fixtureSocket)window.fixtureSocket=new WebSocket('wss://echo.websocket.org');probe(window.fixtureSocket,'existingTcp',false);}catch(e){s.existingTcp='error';}try{probe(new WebSocket('wss://echo.websocket.org'),'newTcp',true);}catch(e){s.newTcp='error';}try{probe(new WebSocket('wss://ws.postman-echo.com/raw'),'newTcpPostman',true);}catch(e){s.newTcpPostman='error';}return true;})())JS";
 if(!evaluate(start))return false;
 const wchar_t *workers=LR"JS((() => {const s=window.fixtureNet;s.workerExistingHttp='pending';s.workerExistingWs='pending';s.workerNewHttp='pending';s.workerNewWs='pending';const run=(w,prefix,terminate)=>{w.onmessage=e=>{s[prefix+e.data.key]=e.data.value;if(terminate&&s.workerNewHttp!=='pending'&&s.workerNewWs!=='pending')w.terminate();};w.onerror=()=>{if(s[prefix+'Http']==='pending')s[prefix+'Http']='error';if(s[prefix+'Ws']==='pending')s[prefix+'Ws']='error';};w.postMessage('probe');setTimeout(()=>{if(s[prefix+'Http']==='pending')s[prefix+'Http']='timeout';if(s[prefix+'Ws']==='pending')s[prefix+'Ws']='timeout';if(terminate)w.terminate();},6500);};try {if(!window.fixtureWorker)window.fixtureWorker=new Worker(window.fixtureWorkerUrl);run(window.fixtureWorker,'workerExisting',false);run(new Worker(window.fixtureWorkerUrl),'workerNew',true);}catch(e){s.workerExistingHttp=s.workerExistingWs=s.workerNewHttp=s.workerNewWs='error';}return true;})())JS";
 if(!evaluate(workers))return false;
 std::wstring local=L"window.fixturePhase="+std::to_wstring(phase)+LR"JS(;(()=>{if(!window.fixturePorts)return true;const s=window.fixtureNet,p=fixturePhase,port=fixturePorts[0],udp=fixturePorts[p];s.localHttp=s.localOldWs=s.localNewWs=s.localStun='pending';const ac=new AbortController();setTimeout(()=>ac.abort(),3000);fetch('http://127.0.0.1:'+port+'/http?phase='+p,{mode:'no-cors',cache:'no-store',signal:ac.signal}).then(()=>s.localHttp='success').catch(()=>s.localHttp='failed');const probe=(ws,key)=>{const token='phase'+p;const send=()=>{if(ws.readyState!==WebSocket.OPEN){s[key]='closed';return;}ws.addEventListener('message',e=>{if(e.data===token)s[key]='echo';});ws.send(token);};if(ws.readyState===WebSocket.CONNECTING)ws.addEventListener('open',send,{once:true});else send();ws.addEventListener('error',()=>{if(s[key]==='pending')s[key]='error';},{once:true});setTimeout(()=>{if(s[key]==='pending')s[key]='timeout';},3500);};try{if(!window.fixtureLocalSocket)window.fixtureLocalSocket=new WebSocket('ws://127.0.0.1:'+port+'/ws');probe(window.fixtureLocalSocket,'localOldWs');probe(new WebSocket('ws://127.0.0.1:'+port+'/ws'),'localNewWs');}catch(e){s.localOldWs=s.localNewWs='error';}try{const pc=new RTCPeerConnection({iceServers:[{urls:'stun:127.0.0.1:'+udp}]});pc.createDataChannel('local');pc.createOffer().then(o=>pc.setLocalDescription(o)).catch(()=>{});setTimeout(()=>{pc.close();s.localStun='done';},3500);}catch(e){s.localStun='error';}return true;})())JS";
 if(!evaluate(local.c_str()))return false;ULONGLONG deadline=GetTickCount64()+10000;while(GetTickCount64()<deadline){if(!evaluate(L"JSON.stringify({...window.fixtureNet,navigatorOnline:navigator.onLine,events:window.fixtureEvents})"))return false;if(script_value.find(L"pending")==std::wstring::npos){wprintf(L"network_phase=%d result=%ls\n",phase,script_value.c_str());fflush(stdout);return true;}Sleep(100);}return false;
}
int wmain(int argc,wchar_t **argv){
 if(argc!=4&&argc!=5&&argc!=6)return 2;if(MH_Initialize()!=MH_OK||!windows_socket_gate::install()||!windows_socket_gate::setblocked(true)||!windows_webview_transport::install()||!windows_webview_transport::prepare_profile(argv[2]))return 30;auto transport_args=windows_webview_transport::arguments();if(!SetEnvironmentVariableW(L"WEBVIEW2_ADDITIONAL_BROWSER_ARGUMENTS",transport_args.c_str()))return 31;bool cdp=argc==6&&!wcscmp(argv[5],L"--cdp-network-state");bool emulate=argc==6&&!wcscmp(argv[5],L"--cdp-offline");bool automatic=argc==6&&!wcscmp(argv[5],L"--cdp-auto");if(argc==6&&!cdp&&!emulate&&!automatic)return 2;if(FAILED(CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED)))return 3;
 HANDLE job=CreateJobObjectW(nullptr,nullptr);if(!job||!AssignProcessToJobObject(job,GetCurrentProcess()))return 4;
 auto browser=(std::filesystem::path(argv[1]).parent_path().parent_path().parent_path()/L"msedgewebview2.exe").wstring();std::thread monitor(monitor_images,job,browser);monitor.detach();
 HMODULE dll=LoadLibraryExW(argv[1],nullptr,LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR|LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);if(!dll){wprintf(L"load_error=%lu\n",GetLastError());return 5;}
 auto f=reinterpret_cast<Factory>(GetProcAddress(dll,"CreateWebViewEnvironmentWithOptionsInternal"));if(!f)return 6;
 window_handle=CreateWindowExW(0,L"STATIC",L"WebView fixture",WS_OVERLAPPEDWINDOW,0,0,400,300,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);if(!window_handle)return 7;ShowWindow(window_handle,SW_SHOW);
 auto h=new Handler(true);HRESULT start=f(false,0,argv[2],nullptr,h);h->Release();if(FAILED(start)||!wait(controller_done,15000)||FAILED(controller_result)||!web){printf("start=%08lx env=%08lx controller=%08lx\n",(long)start,(long)env_result,(long)controller_result);return 8;}
 auto table=*reinterpret_cast<void***>(web);HRESULT nav=reinterpret_cast<Navigate>(table[6])(web,L"<!doctype html><button id='b' onclick='this.textContent=Number(this.textContent)+1'>0</button>");if(FAILED(nav))return 9;
 bool ok=false;for(int i=0;i<100&&!ok;i++){if(evaluate(L"document.getElementById('b') && document.getElementById('b').textContent"))ok=script_value==L"\"0\"";if(!ok)Sleep(50);}if(!ok||!evaluate(L"document.getElementById('b').click();document.getElementById('b').textContent")||script_value!=L"\"1\"")return 10;
 if(!evaluate(L"window.fixtureEvents=[];addEventListener('online',()=>fixtureEvents.push('online'));addEventListener('offline',()=>fixtureEvents.push('offline'));true"))return 24;
 wchar_t local_ports[128]{};DWORD port_length=GetEnvironmentVariableW(L"SV2_CDP_LOCAL_PORTS",local_ports,128);if(port_length){if(port_length>=128)return 31;unsigned commas=0;for(DWORD i=0;i<port_length;i++){if(local_ports[i]==L',')++commas;else if(local_ports[i]<L'0'||local_ports[i]>L'9')return 31;}if(commas!=3)return 31;std::wstring init=L"window.fixturePorts=["+std::wstring(local_ports)+L"];true";if(!evaluate(init.c_str()))return 31;}
 if(!evaluate(LR"JS((() => {const code=`onmessage=()=>{const report=(key,value)=>postMessage({key,value});const ac=new AbortController();setTimeout(()=>ac.abort(),5000);fetch('https://www.cloudflare.com/cdn-cgi/trace?worker='+Date.now(),{mode:'no-cors',cache:'no-store',signal:ac.signal}).then(()=>report('Http','success')).catch(()=>report('Http','failed'));try{const ws=new WebSocket('wss://ws.postman-echo.com/raw');const token='worker-'+Math.random();let finished=false;const done=v=>{if(!finished){finished=true;report('Ws',v);ws.close();}};ws.onopen=()=>ws.send(token);ws.onmessage=e=>{if(e.data===token)done('echo');};ws.onerror=()=>done('error');setTimeout(()=>done('timeout'),6000);}catch(e){report('Ws','error');}}`;window.fixtureWorkerUrl=URL.createObjectURL(new Blob([code],{type:'text/javascript'}));return true;})())JS"))return 26;
 if((cdp||emulate||automatic)&&!enable_cdp_network())return 27;
 if(!capture_preview(argv[3]))return 14;
 JOBOBJECT_BASIC_PROCESS_ID_LIST *list=(JOBOBJECT_BASIC_PROCESS_ID_LIST*)HeapAlloc(GetProcessHeap(),HEAP_ZERO_MEMORY,8192);DWORD n=0;if(!list||!QueryInformationJobObject(job,JobObjectBasicProcessIdList,list,8192,&n))return 11;
 wprintf(L"dom=1 process_count=%lu\n",list->NumberOfProcessIdsInList);for(DWORD i=0;i<list->NumberOfProcessIdsInList;i++){HANDLE p=OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION,FALSE,(DWORD)list->ProcessIdList[i]);wchar_t path[32768];DWORD size=32768;if(!p||!QueryFullProcessImageNameW(p,0,path,&size)){wprintf(L"image_error=%lu\n",GetLastError());return 12;}wprintf(L"image=%ls\n",path);CloseHandle(p);}fflush(stdout);
 if(argc>=5){auto cold=windows_socket_gate::state();if(!cold.blocked||cold.active||cold.unknown||cold.close_failed)return 33;printf("cold_socket_drained=1\n");if(!windows_socket_gate::setblocked(false)||!windows_webview_transport::setblocked(false))return 34;if(!network_observation(1))return 21;if(automatic&&!attach_targets())return 28;for(int phase=2;phase<=3;phase++){wprintf(L"ready=%d\n",phase);fflush(stdout);wchar_t input[16];if(!fgetws(input,16,stdin))return 18;if(phase==2){if(!windows_webview_transport::setblocked(true)||!windows_socket_gate::setblocked(true))return 32;auto drained=windows_socket_gate::state();if(drained.active||drained.unknown||drained.close_failed||drained.discovery_error)return 35;printf("offline_socket_drained=1\n");}else if(!windows_socket_gate::setblocked(false)||!windows_webview_transport::setblocked(false))return 32;if(automatic&&!set_sessions(phase==2))return 29;if((cdp||emulate||automatic)&&!set_cdp_network(phase==2,emulate||automatic))return 25;if(!evaluate(L"document.getElementById('b').click();document.getElementById('b').textContent"))return 19;wchar_t expected[16];swprintf(expected,16,L"\"%d\"",phase);if(script_value!=expected)return 20;std::wstring path=std::wstring(argv[3])+L"."+std::to_wstring(phase)+L".png";if(!capture_preview(path.c_str()))return 22;wprintf(L"dom=%d\n",phase);fflush(stdout);if(!network_observation(phase))return 21;if(automatic&&!wait_sessions())return 30;}}
 monitor_stop=true;ULONGLONG monitor_deadline=GetTickCount64()+5000;while(!monitor_done&&GetTickCount64()<monitor_deadline)Sleep(10);if(!monitor_done)monitor_valid=false;wprintf(L"job_samples=%u valid=%d unknown=%u\n",monitor_samples.load(),monitor_valid.load(),monitor_unknown.load());fflush(stdout);reinterpret_cast<HRESULT(WINAPI*)(IUnknown*)>((*reinterpret_cast<void***>(controller))[24])(controller);web->Release();controller->Release();DestroyWindow(window_handle);return monitor_valid&&monitor_samples&&monitor_unknown==0?0:23;
}
