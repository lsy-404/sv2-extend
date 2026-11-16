from pathlib import Path
import subprocess,json,plistlib,uuid,os,shutil,socket,threading,time,struct
base=Path(__file__).resolve().parents[4];work=base/'work/network-group';work.mkdir(exist_ok=True);source=base/'projects/sv2-extend/loader/macos';(work/'swift-lifecycle').mkdir(exist_ok=True);subprocess.run(['xcrun','clang','-c','-I',str(source),str(source/'swift_network_fixture_entry.c'),'-o',str(work/'swift-lifecycle/host-entry.o')],check=True);subprocess.run(['xcrun','swiftc','-emit-library',str(source/'swift_connection_fixture.swift'),str(work/'swift-lifecycle/host-entry.o'),'-o',str(work/'group-fixture.dylib')],check=True);run=Path.home()/'Library/Application Support/Dreamtonics/Open SV2 Loader/experiments/network-group'/uuid.uuid4().hex;run.mkdir(parents=True,mode=0o700);root=run/'profile';probe=work/('Extend Group '+run.name+'.app');subprocess.run(['ditto',str(Path.home()/'Applications/Extend Loader.app'),str(probe)],check=True);override=os.environ.get('LOADER_GROUP_CORE')
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
thread=threading.Thread(target=consume,daemon=True);thread.start();result=run/'group-result.json';env=dict(os.environ,LOADER_SWIFT_PEER_PORT=str(receiver.getsockname()[1]),LOADER_GROUP_PORT=str(port),LOADER_GROUP_SECOND_PORT=str(ports[1]),LOADER_GROUP_THIRD_PORT=str(ports[2]),LOADER_GROUP_RESULT=str(result));env.pop('OPEN_SV2_LOADER_DATA_DIR',None);env.pop('OPEN_SV2_NETWORK_DIAGNOSTICS',None)
with (run/'stdout.log').open('wb') as out,(run/'stderr.log').open('wb') as err:p=subprocess.Popen([str(exe)],env=env,stdout=out,stderr=err,start_new_session=True)
handle={'pid':p.pid,'root':str(root),'managedApplication':str(managed),'probeApplication':str(probe),'runDirectory':str(run)};(work/'live-host.json').write_text(json.dumps(handle,indent=2));print(json.dumps(handle),flush=True);deadline=time.monotonic()+35
while p.poll() is None and not result.exists() and time.monotonic()<deadline:time.sleep(.1)
if not result.exists():raise RuntimeError('Inspect same live host '+str(p.pid)+'; observation did not complete')
time.sleep(.3);value=json.loads(result.read_text());alive=p.poll() is None;stop.set();receiver.close();thread.join(timeout=1);report={'handle':handle,'fixture':value,'packets':packets,'hostAlive':alive}
if alive:p.terminate();p.wait(timeout=8)
checks={'sameHost':value['pid']==p.pid,'baseline':value['activeReady'],'tracked':value['tracked'],'dormantNotRevived':not value['dormantReadyAfterAllow'],'newBlockedNotRevived':not value['createdWhileBlockedReady'],'pluginExcluded':value['pluginExcluded'],'unixExcluded':value['unixExcluded'],'pluginReady':value['pluginReady'],'freshReady':value['freshReady'],'released':value['outstandingAfterRelease']==value['initial'],'expectedPayloads':sorted(v['text'] for v in packets)==['swift-connection-before','swift-connection-plugin','swift-connection-restored']}
for name in ['before','pluginSend','restored','oldSend']:
 item=value[name];checks[name]=bool(item['completed'] and item['callbackCount']==1 and item['correctQueue'] and item['error']==(1 if name=='oldSend' else 0))
report['checks']=checks;report['passed']=all(checks.values())
(work/'swift-lifecycle/connection-host-results.json').write_text(json.dumps(report,indent=2));print(json.dumps(report,indent=2));assert report['passed']
