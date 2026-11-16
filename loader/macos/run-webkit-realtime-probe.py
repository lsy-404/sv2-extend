from pathlib import Path
import json, os, plistlib, socket, subprocess, threading, time, uuid
from host_probe_support import prepare_host
source=Path(__file__).resolve().parent;base=source.parents[3];work=base/'work/webkit-realtime';work.mkdir(parents=True,exist_ok=True)
fixture=work/'fixture.dylib';subprocess.run(['xcrun','clang','-dynamiclib','-fobjc-arc','-Wall','-Wextra','-Werror',str(source/'webkit_realtime_fixture.m'),'-framework','Foundation','-framework','WebKit','-o',str(fixture)],check=True)
if os.environ.get('LOADER_WEBRTC_STANDALONE'):
 run=work/('standalone-'+uuid.uuid4().hex);run.mkdir(mode=0o700);root=run/'profile';root.mkdir(mode=0o700);app=run/'WebKit Fixture.app';(app/'Contents/MacOS').mkdir(parents=True);exe=app/'Contents/MacOS/fixture'
 (app/'Contents/Info.plist').write_bytes(plistlib.dumps({'CFBundleIdentifier':'io.opensv2.webkit-fixture.p'+run.name.split('-')[-1],'CFBundleExecutable':'fixture','CFBundlePackageType':'APPL','LSUIElement':True}))
 subprocess.run(['xcrun','clang','-fobjc-arc','-Wall','-Wextra','-Werror',str(source/'webkit_fixture_host.m'),'-framework','AppKit','-o',str(exe)],check=True)
 handle={'root':str(root),'executable':str(exe),'runDirectory':str(run),'standalone':True}
 env={k:v for k,v in os.environ.items() if not k.startswith(('OPEN_SV2_','LOADER_PROBE_','DYLD_'))};env.update(DYLD_INSERT_LIBRARIES=str(source/'loader-probe.dylib'),OPEN_SV2_LOADER_DATA_DIR=str(root),OPEN_SV2_ISOLATE_WEBSITE_DATA='1')
else:handle,env=prepare_host(source,work,'webkit-realtime')
run=Path(handle['runDirectory']);result=run/'result.json';packets=[];stop=threading.Event();servers=[];threads=[]
def receive(server,phase):
 while not stop.is_set():
  try:
   data,_=server.recvfrom(4096);packets.append({'destinationPhase':phase,'receivedAtEpoch':time.time(),'length':len(data),'stunCookie':len(data)>=8 and data[4:8]==bytes.fromhex('2112a442')})
  except socket.timeout:pass
  except OSError:break
for phase in range(4):
 server=socket.socket(socket.AF_INET,socket.SOCK_DGRAM);server.bind(('127.0.0.1',0));server.settimeout(.05);servers.append(server);env['LOADER_WEBRTC_PORT'+str(phase)]=str(server.getsockname()[1]);thread=threading.Thread(target=receive,args=(server,phase),daemon=True);thread.start();threads.append(thread)
env.update(LOADER_PROBE_PLUGIN=str(fixture),LOADER_WEBRTC_RESULT=str(result))
with (run/'stdout.log').open('wb') as stdout,(run/'stderr.log').open('wb') as stderr:process=subprocess.Popen([handle['executable']],env=env,stdout=stdout,stderr=stderr,start_new_session=True)
handle['pid']=process.pid;(work/'live-host.json').write_text(json.dumps(handle,indent=2));print(json.dumps(handle),flush=True)
deadline=time.monotonic()+35
while process.poll() is None and not result.exists() and time.monotonic()<deadline:time.sleep(.1)
if not result.exists():raise RuntimeError('Observe the same precise PID before any restart: '+str(process.pid))
value=json.loads(result.read_text());alive=process.poll() is None
if alive:process.terminate();process.wait(timeout=8)
stop.set()
for server in servers:server.close()
for thread in threads:thread.join(timeout=1)
counts=[sum(p['destinationPhase']==phase and p['stunCookie'] for p in packets) for phase in range(4)]
oldAfterBlock=sum(p['destinationPhase']==0 and p['stunCookie'] and value['phases'][1]['startedAtEpoch']+.2<p['receivedAtEpoch']<value['phases'][3]['startedAtEpoch'] for p in packets)
newGuardPassed=bool(counts[0]>0 and counts[1]==counts[2]==0 and counts[3]>0 and all(p['policyCode']==0 for p in value['phases']) and value['unverifiedPaths']&(1<<11) and not value['wholeHostVerified'])
report={'handle':handle,'fixture':value,'packets':packets,'stunCounts':counts,'oldSTUNAfterBlock':oldAfterBlock,'hostAliveAtCompletion':alive,'blockedNewRTCBypassObserved':counts[1]+counts[2]>0,'baselineSTUNObserved':counts[0]>0,'newConnectionGuardPassed':newGuardPassed,'restorationScope':'Fresh online document in the same WebView; offline-created document needs navigation to expose WebRTC again. Retained old RTC revocation is NOT proved.','existingRevocationVerified':False,'scope':'Independent loopback UDP observation; absent STUN or unavailable API is inconclusive. No credentials/media and no internet endpoint.'}
if os.environ.get('LOADER_WEBRTC_POOL_RESET') or os.environ.get('LOADER_WEBRTC_PENDING_CONTROL') or os.environ.get('LOADER_WEBRTC_CORE_RETIRE'):
 phase=value['phases'][1];report['rendererRetirementObserved']=bool(phase['poolReset'] and phase['poolSPIAvailable'] and phase['processesBefore']>0 and phase['oldPIDsStillAlive']==phase['processesAfter']==0 and oldAfterBlock==0);report['pendingNativeJSCompletionObserved']=bool(phase['poolReset'] and phase['pendingAtReset'] and phase['pendingJSReplies']==1 and phase['pendingJSReplyAtEpoch']>=phase['resetAtEpoch'] and phase['pendingJSError']!=0 and phase['pendingJSOnMain']);report['pendingJSControlRemainedPending']=bool(not phase['poolReset'] and phase['pendingAtReset'] and phase['pendingJSReplies']==0 and phase['oldPIDsStillAlive']>0)
if os.environ.get('LOADER_WEBRTC_CORE_RETIRE'):
 report['coreRetirementPassed']=bool(all(value['coreRetirementChecks'].values()) and value['coreRetirementReceipt'].get('ok') and value['coreReplyCount']==1 and value['coreReplyOnMain'] and value['coreReplyCounted'] and value['coreCountCleared'] and value['rejectedCallReplies']==0 and report['rendererRetirementObserved'] and report['pendingNativeJSCompletionObserved'] and newGuardPassed)
name='core-retirement-results.json' if os.environ.get('LOADER_WEBRTC_CORE_RETIRE') else 'pending-control-results.json' if os.environ.get('LOADER_WEBRTC_PENDING_CONTROL') else 'pool-reset-results.json' if os.environ.get('LOADER_WEBRTC_POOL_RESET') else 'standalone-results.json' if os.environ.get('LOADER_WEBRTC_STANDALONE') else 'production-results.json' if os.environ.get('LOADER_WEBRTC_EXPECT_GUARD') else 'native-preference-results.json' if os.environ.get('LOADER_WEBRTC_NATIVE_PREF') else 'results.json'
(work/name).write_text(json.dumps(report,indent=2));print(json.dumps(report,indent=2));assert value['pid']==process.pid and alive
if os.environ.get('LOADER_WEBRTC_EXPECT_GUARD'):assert newGuardPassed
if os.environ.get('LOADER_WEBRTC_CORE_RETIRE'):assert report['coreRetirementPassed']
