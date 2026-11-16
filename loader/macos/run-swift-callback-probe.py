from pathlib import Path
import json,os,socket,subprocess,threading,time,shutil
from host_probe_support import prepare_host
source=Path(__file__).resolve().parent;work=source.parents[3]/'work/swift-callback';work.mkdir(parents=True,exist_ok=True)
subprocess.run(['xcrun','clang','-c',str(source/'swift_network_fixture_entry.c'),'-o',str(work/'entry.o')],check=True)
subprocess.run(['xcrun','clang','-Wall','-Wextra','-Werror','-c',str(source/'swift_callback_inspector.c'),'-o',str(work/'inspector.o')],check=True)
subprocess.run(['xcrun','swiftc','-emit-library','-import-objc-header',str(source/'swift_callback_fixture.h'),str(source/'swift_callback_fixture.swift'),str(work/'entry.o'),str(work/'inspector.o'),'-o',str(work/'fixture.dylib')],check=True)
handle,env=prepare_host(source,work,'swift-callback');root=Path(handle['root']);package=root/'plugins/swift-callback';package.mkdir(mode=0o700);shutil.copy2(work/'fixture.dylib',package/'fixture.dylib');(package/'plugin.json').write_text(json.dumps({'schema':1,'id':'test.swift-callback','name':'Swift callback diagnostic','version':'1','nativeLibrary':'fixture.dylib'}))
peer=socket.socket(socket.AF_INET,socket.SOCK_DGRAM);peer.bind(('127.0.0.1',0));peer.settimeout(.1);packets=[];stop=threading.Event()
def consume():
 while not stop.is_set():
  try:
   data,address=peer.recvfrom(4096);packets.append(data.decode())
   if data==b'swift-callback-receive':peer.sendto(b'swift-callback-response',address)
  except socket.timeout:pass
  except OSError:break
thread=threading.Thread(target=consume,daemon=True);thread.start();run=Path(handle['runDirectory']);result=run/'result.json';env.update(EXTEND_CALLBACK_PORT=str(peer.getsockname()[1]),EXTEND_CALLBACK_RESULT=str(result))
try:
 with (run/'stdout.log').open('wb') as out,(run/'stderr.log').open('wb') as err:p=subprocess.Popen([handle['executable']],env=env,stdout=out,stderr=err,start_new_session=True)
 handle['pid']=p.pid;(work/'live-host.json').write_text(json.dumps(handle,indent=2));print(json.dumps(handle),flush=True)
 deadline=time.monotonic()+25
 while p.poll() is None and not result.exists() and time.monotonic()<deadline:time.sleep(.1)
 if not result.exists():raise RuntimeError('Inspect same host PID '+str(p.pid)+'; observation incomplete')
 value=json.loads(result.read_text());time.sleep(.15)
 checks={'guardedSocketsActuallyPresent':any(case['guardedBeforeBlock']>0 for case in value['cases']),'sameHost':value['pid']==p.pid,'realPayloadsObserved':all(packets.count(tag)==1 for tag in ['swift-callback-held','swift-callback-receive']),'onlySubmittedPayloads':all(tag in ['swift-callback-held','swift-callback-receive','swift-callback-queued'] for tag in packets)}
 for case in value['cases']:
  mode=case['mode'];checks[mode+'Ready']=case['ready'];checks[mode+'NativeCompletionOnce']=case['gotCallback'] and case['returned'] and case['replies']==1 and case['correctQueue']
  checks[mode+'HeldCallbackCounted']=case['outstandingWhileHeld']>case['baseline'];checks[mode+'CallbackReturnDrains']=case['outstandingAfterReturn']==case['baseline']
  checks[mode+'UnifiedPolicyApplied']=case['policyRevoked'] and case['policyRestored'] and case['publicBlocked']==1
  checks[mode+'PublicSnapshotNotPrematurelyDrained']=case['publicDrainedWhileHeld']==0
  checks[mode+'UnverifiedFlagsRetained']=case['wholeHostVerified']==0 and case['swiftUnverified']
  checks[mode+'GuardedRawCoverageExplicit']=case['guardedBeforeBlock']==0 or case['rawCoverageUnverified']
  if mode!='queued':checks[mode+'NativeResultPreserved']=case['success'] and (mode!='receive' or case['bodyValid'])
  else:checks['queuedBeforeDispatchCounted']=case['outstandingWhileQueuePaused']>case['baseline']
 report={'checks':checks,'fixture':value,'packets':packets,'handle':handle,'passed':all(checks.values()),'scope':'Actual isolated SV2 Swift NWConnection send/receive and paused-queue callbacks, application owner released and host policy revoked while held; independent UDP peer. Not all Swift callbacks or whole-host coverage.'}
 suffix='audit' if os.environ.get('EXTEND_CALLBACK_AUDIT') else 'results';(work/(suffix+'.json')).write_text(json.dumps(report,indent=2));print(json.dumps(report,indent=2))
 if p.poll() is None:p.terminate();p.wait(timeout=8)
 if suffix!='audit':assert report['passed']
finally:stop.set();peer.close();thread.join(timeout=2)
