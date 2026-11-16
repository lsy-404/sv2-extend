"""Read/select voices through a copied installed Loader in a disposable profile.

No candidate Core replacement, original profile migration, or account operations.
Incomplete observation leaves the same host alive for inspection.
"""
from pathlib import Path
import hashlib,json,os,plistlib,shutil,subprocess,time,urllib.request,uuid

source=Path(__file__).resolve().parent
work=source.parents[3]/'work/installed-export-regression'
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
domain='io.opensv2.installed-export-test.p'+run.name
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
scripts=root/'host-home/Library/Application Support/Dreamtonics/Synthesizer V Studio 2/scripts'
scripts.mkdir(parents=True,exist_ok=True)
lua_module=probe/'Contents/Resources/Native/Lua'
lua_script="function getClientInfo() return {name='Export Public Lua Validation',author='Open SV2 Loader',category='Extend Acceptance',versionNumber=1,minEditorVersion=131330} end\nfunction main()\n package.path=MODULEPATH..'/?.lua;'..package.path\n local E=require('sv2_extend');local json=require('sv2_extend_json');local checks={};local deadline=os.time()+60\n local function finish(error) SV:print('EXTEND_LUA_EXPORT_RESULT '..json.encode({checks=checks,error=error}));SV:finish() end\n E.exports.snapshot(function(snapshot,error) if error then finish(error.code);return end;checks.snapshot=not snapshot.busy\n E.exports.start({guard=snapshot.guard,outputPath=OUTPUTPATH,channels=1,sampleRate=44100,bitDepth=16},function(job,error) if error then finish(error.code);return end;checks.started=job.state=='running' and job.configRestored\n local function poll() E.exports.status(job.jobId,function(status,error) if error then finish(error.code);return end\n if status.state=='running' then if os.time()>deadline then finish('TIMEOUT');return end;SV:setTimeout(100,poll);return end\n checks.completed=status.state=='completed';checks.audio=status.audio and status.audio.sampleRate==44100 and status.audio.channels==1 and status.audio.bitDepth==16;finish(status.error and status.error.code)\n end) end;poll() end) end)\nend\n"
js_script="function getClientInfo(){return {name:'Export Public JS Validation',author:'Open SV2 Loader',category:'Extend Acceptance',versionNumber:1,minEditorVersion:131330};}\nfunction main(){var E=eval(Duktape.enc('opensv2.module.v1','')),checks={},deadline=Date.now()+60000;\n function finish(error){SV.print('EXTEND_JS_EXPORT_RESULT '+JSON.stringify({checks:checks,error:error||null}));SV.finish();}\n E.exports.snapshot(function(error,snapshot){if(error){finish(error.code);return;}checks.snapshot=!snapshot.busy;\n E.exports.start({guard:snapshot.guard,outputPath:OUTPUTPATH,channels:1,sampleRate:48000,bitDepth:24},function(error,job){if(error){finish(error.code);return;}checks.started=job.state==='running'&&job.configRestored;\n function poll(){E.exports.status(job.jobId,function(error,status){if(error){finish(error.code);return;}if(status.state==='running'){if(Date.now()>deadline){finish('TIMEOUT');return;}SV.setTimeout(100,poll);return;}checks.completed=status.state==='completed';checks.audio=status.audio&&status.audio.sampleRate===48000&&status.audio.channels===1&&status.audio.bitDepth===24;finish(status.error&&status.error.code);});}poll();});});}\n"
(scripts/'ExtendLuaExportAcceptance.lua').write_text(lua_script.replace('MODULEPATH',json.dumps(str(lua_module))).replace('OUTPUTPATH',json.dumps(str(run/'lua.wav'))))
(scripts/'ExtendJSExportAcceptance.js').write_text(js_script.replace('OUTPUTPATH',json.dumps(str(run/'javascript.wav'))))
# Copy only a non-personal one-note fixture, and select it in the GUI.
fixture=source.parents[1]/'tests/fixtures/export-one-note.svp'
project=run/'export-fixture.svp';shutil.copy2(fixture,project)
with (run/'stdout.log').open('wb') as out,(run/'stderr.log').open('wb') as err:
 process=subprocess.Popen(args,env=env,stdout=out,stderr=err,start_new_session=True)
handle={'pid':process.pid,'runDirectory':str(run),'profile':str(root),'managedApplication':str(managed),'project':str(project),'coreSHA256':core_hash,'preferencesDomain':domain}
(work/'live-host.json').write_text(json.dumps(handle,indent=2));print(json.dumps(handle),flush=True)
deadline=time.monotonic()+300;results={}
while process.poll() is None and time.monotonic()<deadline:
 for line in (run/'stdout.log').read_text(errors='replace').splitlines():
  for language in ['LUA','JS']:
   marker='EXTEND_'+language+'_EXPORT_RESULT '
   if line.startswith(marker):results[language]=json.loads(line[len(marker):])
 if len(results)==2:break
 time.sleep(.2)
assert len(results)==2,'Inspect same host PID '+str(process.pid)
import wave
checks={language:not result.get('error') and len(result['checks'])==4 and all(result['checks'].values()) for language,result in results.items()}
for name,rate,width in [('lua',44100,2),('javascript',48000,3)]:
 with wave.open(str(run/(name+'.wav'))) as wav:
  checks[name+'FileFormat']=wav.getnchannels()==1 and wav.getframerate()==rate and wav.getsampwidth()==width and wav.getnframes()>0
checks['hostAlive']=process.poll() is None
report={'handle':handle,'checks':checks,'results':results,'passed':all(checks.values()),'scope':'Official Lua and JS scripts, copied installed Loader Core, disposable copied one-note project; real WAV start/status/completion and independently parsed formats. Not cancellation or no-overwrite proof.'}
(run/'results.json').write_text(json.dumps(report,indent=2));(work/'results.json').write_text(json.dumps(report,indent=2));print(json.dumps(report,indent=2),flush=True)
assert report['passed'],'Inspect same host before cleanup'
process.terminate();process.wait(timeout=8)
subprocess.run(['defaults','delete',domain],check=True,capture_output=True)
