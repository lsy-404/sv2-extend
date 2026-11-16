"""Read/select voices through a copied installed Loader in a disposable profile.

No candidate Core replacement, original profile migration, or account operations.
Incomplete observation leaves the same host alive for inspection.
"""
from pathlib import Path
import hashlib,json,os,plistlib,shutil,subprocess,time,urllib.request,uuid

source=Path(__file__).resolve().parent
work=source.parents[3]/'work/installed-export-boundaries'
work.mkdir(parents=True,exist_ok=True)
run=work/uuid.uuid4().hex
run.mkdir(mode=0o700)
installed=Path.home()/'Applications/Extend Loader.app'
probe=run/'Extend Loader.app'
subprocess.run(['ditto',str(installed),str(probe)],check=True)
core_relative=Path('Contents/Resources/Native/loader-probe.dylib')
digest=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
core_hash=digest(installed/core_relative)
assert digest(probe/core_relative)==core_hash
info_path=probe/'Contents/Info.plist'
info=plistlib.loads(info_path.read_bytes())
domain='io.opensv2.installed-export-boundaries.p'+run.name
info['CFBundleIdentifier']=domain
info_path.write_bytes(plistlib.dumps(info))
subprocess.run(['codesign','--force','--sign','-',str(probe)],check=True,capture_output=True)
empty=run/'empty';empty.mkdir()
subprocess.run(['defaults','write',domain,'loaderDataDirectory',str(empty)],check=True)
root=run/'profile'
args=[str(probe/'Contents/MacOS/ExtendLoader'),'--data-dir',str(root),'--isolate-host-data']
env={k:v for k,v in os.environ.items() if not k.startswith(('OPEN_SV2_','LOADER_PROBE_','DYLD_','EXTEND_'))}
prepared=subprocess.run(args+['--prepare-only'],env=env,check=True,capture_output=True,text=True)
managed=Path(prepared.stdout.strip())
project=run/'export-boundary-fixture.svp'
fixture=json.loads((source.parents[1]/'tests/fixtures/export-one-note.svp').read_text())
# Longer owned note keeps native rendering pending long enough to cancel.
for group in fixture['library']:
 for note in group.get('notes',[]):note['duration']=705600000*120
project.write_text(json.dumps(fixture,ensure_ascii=False))
with (run/'stdout.log').open('wb') as out,(run/'stderr.log').open('wb') as err:
 process=subprocess.Popen(args,env=env,stdout=out,stderr=err,start_new_session=True)
handle={'pid':process.pid,'runDirectory':str(run),'profile':str(root),'managedApplication':str(managed),'project':str(project),'coreSHA256':core_hash,'preferencesDomain':domain}
(work/'live-host.json').write_text(json.dumps(handle,indent=2));print(json.dumps(handle),flush=True)
def call(name,arguments=None):
 endpoint=json.loads((root/f'endpoints/sv2-mcp-{process.pid}.json').read_text())
 request=urllib.request.Request(endpoint['url'],data=json.dumps({'jsonrpc':'2.0','id':1,'method':'tools/call','params':{'name':name,'arguments':arguments or {}}}).encode(),headers={'Content-Type':'application/json','Authorization':'Bearer '+endpoint['token']})
 return json.load(urllib.request.urlopen(request,timeout=8))['result']['structuredContent']
deadline=time.monotonic()+300;ready=False
while process.poll() is None and time.monotonic()<deadline:
 try:
  value=call('sv_export_snapshot')
  # The GUI opens the owned fixture. Never act on an unrelated project.
  editor=call('sv_read_editor')
  if value.get('ok') and (run/'project-open-confirmed').is_file():ready=True;break
 except (OSError,KeyError,json.JSONDecodeError):pass
 time.sleep(.2)
assert ready,'Inspect same host PID '+str(process.pid)
checks={};responses=[]
sentinel=run/'existing.wav';sentinel.write_bytes(b'owned output must survive\x00')
opts={'outputPath':str(sentinel),'channels':1,'sampleRate':48000,'bitDepth':24,'guard':call('sv_export_snapshot')['result']['guard']}
answer=call('sv_export_wav',opts);responses.append(answer)
checks['existingOutputRejected']=answer.get('error',{}).get('code')=='OUTPUT_EXISTS'
checks['existingBytesPreserved']=sentinel.read_bytes()==b'owned output must survive\x00'
target=run/'cancelled.wav';opts.update(outputPath=str(target),guard=call('sv_export_snapshot')['result']['guard'])
started=call('sv_export_wav',opts);responses.append(started);checks['nativeRenderStarted']=started.get('ok') is True
assert checks['nativeRenderStarted'],started
job=started['result']['jobId'];cancel=call('sv_export_cancel',{'jobId':job});responses.append(cancel);checks['cancelAccepted']=cancel.get('ok') is True
deadline=time.monotonic()+45;status=None
while process.poll() is None and time.monotonic()<deadline:
 status=call('sv_export_status',{'jobId':job})
 if status.get('result',{}).get('state') in ['cancelled','completed','failed']:break
 time.sleep(.1)
responses.append(status);checks['nativeCancelledTerminal']=status.get('result',{}).get('state')=='cancelled'
checks['cancelledOutputNotPublished']=not target.exists()
checks['subsequentSnapshotAvailable']=call('sv_export_snapshot').get('ok') is True
checks['hostAlive']=process.poll() is None
report={'handle':handle,'checks':checks,'responses':responses,'passed':all(checks.values()),'scope':'Copied installed Core and MCP, real resident native export in disposable extended-note project; existing target rejection, actual cancellation terminal and no final output. Not direct Lua/JS cancellation callbacks or network/account proof.'}
(run/'results.json').write_text(json.dumps(report,indent=2));(work/'results.json').write_text(json.dumps(report,indent=2));print(json.dumps(report,indent=2),flush=True)
assert report['passed'],'Inspect same host before cleanup'
process.terminate();process.wait(timeout=8)
subprocess.run(['defaults','delete',domain],check=True,capture_output=True)
