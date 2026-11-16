"""Read/select voices through a copied installed Loader in a disposable profile.

No candidate Core replacement, original profile migration, or account operations.
Incomplete observation leaves the same host alive for inspection.
"""
from pathlib import Path
import hashlib,json,os,plistlib,shutil,subprocess,time,urllib.request,uuid

source=Path(__file__).resolve().parent
work=source.parents[3]/'work/installed-script-regression'
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
domain='io.opensv2.installed-script-test.p'+run.name
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
lua_script="function getClientInfo() return {name='Extend Lua Voice Acceptance',author='Open SV2',category='Extend Acceptance',versionNumber=1,minEditorVersion=131840} end\nfunction main()\n package.path=MODULEPATH..'/?.lua;'..package.path\n local E=require('sv2_extend');local J=require('sv2_extend_json');local c={};local i=E.runtimeInfo();c.pid=i.pid;c.mainThread=i.mainThread==true;c.noPipe=i.usesFilesOrPipes==false\n local function done() SV:print('EXTEND_LUA_VOICE_RESULT '..J.encode(c));SV:finish() end\n E.voicebanks.list(function(v,e)\n  c.inventory=not e and #v.voicebanks>=2;if not c.inventory then done();return end\n  E.voicebanks.current(function(b,e)\n   c.current=not e;if e then done();return end\n   E.voicebanks.select(v.voicebanks[2].voicebankId,b.guard,function(a,e)\n    c.selected=not e\n    E.voicebanks.current(function(a,e)\n     c.readback=not e and a.voicebank.identityKey==v.voicebanks[2].voicebankId\n     E.exports.snapshot(function(a,e) c.exportSnapshot=not e;done() end)\n    end)\n   end)\n  end)\n end)\nend\n"
js_script="function getClientInfo(){return {name:'Extend JS Voice Acceptance',author:'Open SV2',category:'Extend Acceptance',versionNumber:1,minEditorVersion:131840};}\nfunction main(){var E=eval(Duktape.enc('opensv2.module.v1','')),c={},i=E.runtimeInfo();c.pid=i.result.pid;c.mainThread=i.result.mainThread===true;c.noPipe=i.result.usesFilesOrPipes===false;\n function done(){SV.print('EXTEND_JS_VOICE_RESULT '+JSON.stringify(c));SV.finish();}\n E.voicebanks.list(function(e,v){c.inventory=!e&&v.voicebanks.length>=2;if(!c.inventory){done();return;}\n E.voicebanks.current(function(e,b){c.current=!e;if(e){done();return;}\n E.voicebanks.select(v.voicebanks[0].voicebankId,b.guard,function(e,a){c.selected=!e;\n E.voicebanks.current(function(e,a){c.readback=!e&&a.voicebank.identityKey===v.voicebanks[0].voicebankId;\n E.exports.snapshot(function(e,a){c.exportSnapshot=!e;done();});});});});});}\n"
(scripts/'ExtendLuaVoiceAcceptance.lua').write_text(lua_script.replace('MODULEPATH',json.dumps(str(lua_module))))
(scripts/'ExtendJSVoiceAcceptance.js').write_text(js_script)
with (run/'stdout.log').open('wb') as out,(run/'stderr.log').open('wb') as err:
 process=subprocess.Popen(args,env=env,stdout=out,stderr=err,start_new_session=True)
handle={'pid':process.pid,'runDirectory':str(run),'profile':str(root),'managedApplication':str(managed),'coreSHA256':core_hash,'preferencesDomain':domain}
(work/'live-host.json').write_text(json.dumps(handle,indent=2));print(json.dumps(handle),flush=True)
# Official scripts are invoked through the host GUI; output is diagnostic only.
deadline=time.monotonic()+300;results={}
while process.poll() is None and time.monotonic()<deadline:
 for line in (run/'stdout.log').read_text(errors='replace').splitlines():
  for language in ['LUA','JS']:
   prefix='EXTEND_'+language+'_VOICE_RESULT '
   if line.startswith(prefix):results[language]=json.loads(line[len(prefix):])
 if len(results)==2:break
 time.sleep(.2)
assert len(results)==2,'Inspect same host PID '+str(process.pid)
checks={language:all(bool(value) for value in values.values()) and values['pid']==process.pid for language,values in results.items()}
report={'handle':handle,'checks':checks,'results':results,'passed':all(checks.values()),'scope':'Official user Lua and JS scripts invoked by GUI in copied installed Loader with unchanged Core, disposable isolated blank project; real voice list/current/select/readback and export snapshot, not WAV render or network/account proof.'}
(run/'results.json').write_text(json.dumps(report,indent=2));(work/'results.json').write_text(json.dumps(report,indent=2));print(json.dumps(report,indent=2),flush=True)
assert report['passed'],'Inspect same host before cleanup'
process.terminate();process.wait(timeout=8)
subprocess.run(['defaults','delete',domain],check=True,capture_output=True)
