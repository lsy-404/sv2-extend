from pathlib import Path
import subprocess,json,plistlib,uuid,os,shutil,socket,threading,time,struct
base=Path(__file__).resolve().parents[4];work=base/'work/network-group';work.mkdir(exist_ok=True);source=Path(__file__).resolve().parent;subprocess.run(['xcrun','clang','-arch','arm64','-arch','x86_64','-fobjc-arc','-Wall','-Wextra','-Werror','-dynamiclib',str(source/'network_group_fixture.m'),'-framework','Foundation','-framework','Network','-o',str(work/'group-fixture.dylib')],check=True);run=Path.home()/'Library/Application Support/Dreamtonics/Open SV2 Loader/experiments/network-group'/uuid.uuid4().hex;run.mkdir(parents=True,mode=0o700);root=run/'profile';probe=work/('Extend Group '+run.name+'.app');subprocess.run(['ditto',str(Path.home()/'Applications/Extend Loader.app'),str(probe)],check=True);override=os.environ.get('LOADER_GROUP_CORE')
if override:shutil.copy2(override,probe/'Contents/Resources/Native/loader-probe.dylib')
domain='io.opensv2.group-probe.p'+run.name;infoPath=probe/'Contents/Info.plist';info=plistlib.loads(infoPath.read_bytes());info['CFBundleIdentifier']=domain;infoPath.write_bytes(plistlib.dumps(info));subprocess.run(['codesign','--force','--sign','-',str(probe)],capture_output=True,check=True);empty=run/'empty';empty.mkdir();subprocess.run(['defaults','write',domain,'loaderDataDirectory',str(empty)],check=True);exe=probe/'Contents/MacOS/ExtendLoader';p=subprocess.run([str(exe),'--data-dir',str(root),'--isolate-host-data','--prepare-only'],capture_output=True,text=True);assert p.returncode==0,p.stderr;managed=Path(p.stdout.strip());package=root/'plugins/group-probe';package.mkdir(mode=0o700);shutil.copy2(work/'group-fixture.dylib',package/'plugin.dylib');subprocess.run(['codesign','--force','--sign','-',str(package/'plugin.dylib')],capture_output=True,check=True);(package/'plugin.json').write_text(json.dumps({'schema':1,'id':'test.group-probe','name':'NW Group Probe','version':'1','nativeLibrary':'plugin.dylib'}))
reservations=[socket.socket(socket.AF_INET,socket.SOCK_DGRAM) for _ in range(3)]
for reservation in reservations:reservation.bind(('',0))
ports=[r.getsockname()[1] for r in reservations]
for reservation in reservations:reservation.close()
receiver=socket.socket(socket.AF_INET,socket.SOCK_DGRAM);receiver.bind(('',0));receiver.setsockopt(socket.IPPROTO_IP,socket.IP_MULTICAST_TTL,0);receiver.setsockopt(socket.IPPROTO_IP,socket.IP_MULTICAST_LOOP,1);receiver.settimeout(.05);packets=[];stop=threading.Event()
def consume():
 while not stop.is_set():
  try:
   for port in ports:receiver.sendto(b'group-hello',('239.255.18.42',port))
   data,address=receiver.recvfrom(4096);packets.append({'text':data.decode()})
  except socket.timeout:pass
  except OSError:break
port=ports[0]
thread=threading.Thread(target=consume,daemon=True);thread.start();result=run/'group-result.json';env=dict(os.environ,LOADER_GROUP_PORT=str(port),LOADER_GROUP_SECOND_PORT=str(ports[1]),LOADER_GROUP_THIRD_PORT=str(ports[2]),LOADER_GROUP_RESULT=str(result));env.pop('OPEN_SV2_LOADER_DATA_DIR',None);env.pop('OPEN_SV2_NETWORK_DIAGNOSTICS',None)
with (run/'stdout.log').open('wb') as out,(run/'stderr.log').open('wb') as err:p=subprocess.Popen([str(exe)],env=env,stdout=out,stderr=err,start_new_session=True)
handle={'pid':p.pid,'root':str(root),'managedApplication':str(managed),'probeApplication':str(probe),'runDirectory':str(run)};(work/'live-host.json').write_text(json.dumps(handle,indent=2));print(json.dumps(handle),flush=True);deadline=time.monotonic()+35
while p.poll() is None and not result.exists() and time.monotonic()<deadline:time.sleep(.1)
if not result.exists():raise RuntimeError('Inspect same live host '+str(p.pid)+'; observation did not complete')
time.sleep(.3);value=json.loads(result.read_text());alive=p.poll() is None;stop.set();receiver.close();thread.join(timeout=1);report={'handle':handle,'fixture':value,'packets':packets,'hostAlive':alive,'coreOverride':override,'receiverEnabled':True,'validTransportBaseline':bool(value.get('readyAndPeerKnown') and value.get('before',{}).get('completed') and value.get('before',{}).get('error')==0 and all(tag in [v['text'] for v in packets] for tag in ['group-before','reply-before','group-restored','reply-restored'])),'scope':'Resident package and multicast handshake with TTL 0 and an independent unicast UDP peer. Diagnostic findings only; ready/send failures or missing observed payload do not prove transport coverage.'};(work/'baseline-results.json').write_text(json.dumps(report,indent=2));print(json.dumps(report,indent=2));
if alive:p.terminate();p.wait(timeout=8)
assert report['validTransportBaseline'],'Baseline transport was not independently observed'

if os.environ.get('LOADER_GROUP_EXPECT_CONTROLLED')=='1':
 from collections import Counter
 checks={'validTransportBaseline':report['validTransportBaseline'],'liveGroupTracked':bool(value['liveGroupTracked']),'queuedCancellationTracked':bool(value['blockedStartTrackedWhileQueuePaused']),'queuedRejectionTracked':bool(value['existingBlocked'].get('trackedWhileQueuePaused')),'oldDormantGroupNotRevived':bool(value['dormantOldGroupCancelledAfterAllow']),'createdWhileBlockedNotRevived':bool(value['createdDuringBlockGroupCancelledAfterAllow']),'blockedGroupNotReady':not value['newGroupReadyWhileBlocked'],'stateCallbackTracked':bool(value['stateCallbackTracked']),'receiveCallbackTracked':bool(value['receiveCallbackTracked']),'callbacksDrained':bool(value['networkCallbacksDrained']),'objectsDrained':bool(value['networkObjectsDrained']),'wholeHostRemainsUnverified':value['wholeHostVerified']==0,'swiftGapExplicit':bool(value['swiftGapReported']),'multiplexGapExplicit':bool(value['multiplexGapReported']),'cancelAcknowledged':bool(value['cancelAcknowledged']),'blockedEndpointExtractionRejected':bool(value['blockedExtractionRejected'])}
 for name in ['before','restored']:
  item=value[name];checks[name+'SendAndQueue']=bool(item['completed'] and item['error']==0 and item['trackedDuringCompletion'] and item['correctCallbackQueue'])
 for name in ['existingBlocked','newBlocked','revokedAfterRestore','dormantSendRejectedAfterAllow','duringBlockDormantSendRejectedAfterAllow']:
  item=value[name];checks[name+'ErrorAndQueue']=bool(item['completed'] and item['error']!=0 and item['trackedDuringCompletion'] and item['correctCallbackQueue'])
 child=value['endpointChild'];checks['endpointChildLifecycle']=bool(child.get('created') and child.get('ready') and child.get('sent') and child.get('error')==0 and child.get('callbackTracked') and child.get('reinserted'))
 burst=value['rejectionBurst'];checks['rejectionBurstExactlyOnce']=bool(burst['completed'] and burst['callbackCount']==burst['expected']==64 and burst['allErrorsTrackedOnOriginalQueue'] and burst['pendingWhileQueuePaused'])
 checks['onlyExpectedPayloadsArrived']=Counter(v['text'] for v in packets)==Counter(['group-before','reply-before','extracted-endpoint-before','group-restored','reply-restored'])
 report['checks']=checks;report['passed']=all(checks.values());report['unverified']=['Swift facade does not enter this C interposer; raw-layer standalone test separately blocks its tested send','Positive message-context extraction unavailable in this multicast fixture','QUIC/multiplex new-connection callbacks and private NW paths not yet verified'];report['scope']='Actual SV2 resident C group API: multicast handshake TTL0, independent UDP payload observer, send/reply policy, endpoint extraction/reinsertion, queue/cancellation accounting. Not whole-host isolation.'
 (work/'controlled-results.json').write_text(json.dumps(report,indent=2));print(json.dumps({'checks':checks,'passed':report['passed']},indent=2));assert report['passed']
