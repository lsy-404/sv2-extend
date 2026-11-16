from pathlib import Path
import os,json,subprocess,shutil,uuid,time,urllib.request,hashlib,wave
source=Path(__file__).resolve().parent;base=source.parents[3];probe=base/'work/script-http-settings/Extend Loader Policy Probe.app';assets=probe/'Contents/Resources/Native';shutil.copy2(source/'loader-probe.dylib',assets/'loader-probe.dylib');subprocess.run(['codesign','--force','--sign','-',str(probe)],check=True,capture_output=True)
run=Path.home()/'Library/Application Support/Dreamtonics/Open SV2 Loader/experiments/export-return-abi'/uuid.uuid4().hex;root=run/'profile';exe=probe/'Contents/MacOS/ExtendLoader';prepared=subprocess.run([str(exe),'--experimental-host-data','--data-dir',str(root),'--prepare-only'],capture_output=True,text=True);assert prepared.returncode==0,prepared.stderr;app=Path(prepared.stdout.strip());project=run/'export-fixture.svp';shutil.copy2(base/'work/export-native/test-render.svp',project);env=os.environ.copy();env.pop('OPEN_SV2_LOADER_DATA_DIR',None);env.pop('OPEN_SV2_NETWORK_DIAGNOSTICS',None)
with (run/'stdout.log').open('wb') as out,(run/'stderr.log').open('wb') as err:p=subprocess.Popen([str(app/'Contents/MacOS/loader-launcher')],env=env,stdout=out,stderr=err,start_new_session=True)
handle={'pid':p.pid,'root':str(root),'app':str(app),'runDirectory':str(run),'project':str(project)};(base/'work/export-abi/host.json').write_text(json.dumps(handle,indent=2));print(json.dumps(handle),flush=True)
def call(name,args=None):
 d=json.loads((root/f'endpoints/sv2-mcp-{p.pid}.json').read_text());req=urllib.request.Request(d['url'],data=json.dumps({'jsonrpc':'2.0','id':1,'method':'tools/call','params':{'name':name,'arguments':args or {}}}).encode(),headers={'Content-Type':'application/json','Authorization':'Bearer '+d['token']});return json.load(urllib.request.urlopen(req,timeout=8))['result']['structuredContent']
deadline=time.monotonic()+300;editor=None
while p.poll() is None and time.monotonic()<deadline:
 try:
  if call('sv_status').get('editorReady'):
   editor=call('sv_read_editor')
   if editor.get('ok') and editor['result']['noteCount']==1:break
 except (OSError,KeyError):pass
 time.sleep(.2)
assert editor and editor.get('result',{}).get('noteCount')==1,'Inspect same live host '+str(p.pid)
checks={};checks['fiftyNativeProjectReturns']=all(call('sv_export_snapshot').get('ok') for _ in range(50));snap=call('sv_export_snapshot');target=run/'abi-render.wav';job=call('sv_export_wav',{'guard':snap['result']['guard'],'outputPath':str(target),'channels':1,'sampleRate':48000,'bitDepth':24});checks['renderStarted']=job.get('ok') is True;assert checks['renderStarted'],job
identifier=job['result']['jobId'];deadline=time.monotonic()+45;status=None
while p.poll() is None and time.monotonic()<deadline:
 status=call('sv_export_status',{'jobId':identifier})
 if status.get('result',{}).get('state') in ['completed','failed','cancelled']:break
 time.sleep(.15)
checks['renderCompleted']=status.get('result',{}).get('state')=='completed';checks['baselineByteIdentical']=target.is_file() and hashlib.sha256(target.read_bytes()).hexdigest()=='d93caf0b01d5af2d0b1cb7d936152f1a06937254cfedf94014a5f70e1a379d6e'
if target.exists():
 with wave.open(str(target)) as wav:checks['formatPreserved']=wav.getnchannels()==1 and wav.getframerate()==48000 and wav.getsampwidth()==3
snap=call('sv_export_snapshot');again=call('sv_export_wav',{'guard':snap['result']['guard'],'outputPath':str(target),'channels':1,'sampleRate':48000,'bitDepth':24});checks['existingOutputRejected']=again.get('error',{}).get('code')=='OUTPUT_EXISTS';checks['voiceReadStillWorks']=call('sv_read_voicebank').get('ok') is True;checks['hostAlive']=p.poll() is None
report=dict(handle,checks=checks,passed=all(checks.values()),coreSHA256=hashlib.sha256((app/'Contents/Frameworks/OpenSV2Loader.dylib').read_bytes()).hexdigest(),scope='Real supported ARM SV2 with both opaque return veneers, official Lua/MCP export snapshot/start/status, exact baseline WAV and no-overwrite rejection in disposable copied project. No installed delivery claim.');(run/'result.json').write_text(json.dumps(report,indent=2));(base/'work/export-abi/results.json').write_text(json.dumps(report,indent=2));print(json.dumps(report,indent=2));assert report['passed']
