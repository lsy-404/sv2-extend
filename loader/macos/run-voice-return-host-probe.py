from pathlib import Path
import os,json,subprocess,shutil,uuid,time,urllib.request,hashlib
source=Path(__file__).resolve().parent;base=source.parents[3];probe=base/'work/script-http-settings/Extend Loader Policy Probe.app';assets=probe/'Contents/Resources/Native';shutil.copy2(source/'loader-probe.dylib',assets/'loader-probe.dylib');shutil.copy2(base/'projects/sv2-extend/plugins/lua-extend/lua/sv2_extend.lua',assets/'Lua/sv2_extend.lua');subprocess.run(['codesign','--force','--sign','-',str(probe)],check=True,capture_output=True)
run=Path.home()/'Library/Application Support/Dreamtonics/Open SV2 Loader/experiments/voice-return-abi'/uuid.uuid4().hex;root=run/'profile';exe=probe/'Contents/MacOS/ExtendLoader';args=[str(exe),'--experimental-host-data','--data-dir',str(root)];prepared=subprocess.run(args+['--prepare-only'],capture_output=True,text=True);assert prepared.returncode==0,prepared.stderr;app=Path(prepared.stdout.strip());env=os.environ.copy();env.pop('OPEN_SV2_LOADER_DATA_DIR',None);env.pop('OPEN_SV2_NETWORK_DIAGNOSTICS',None)
with (run/'stdout.log').open('wb') as out,(run/'stderr.log').open('wb') as err:p=subprocess.Popen(args,env=env,stdout=out,stderr=err,start_new_session=True)
handle={'pid':p.pid,'root':str(root),'app':str(app),'runDirectory':str(run)};(base/'work/voice-abi/host.json').write_text(json.dumps(handle,indent=2));print(json.dumps(handle),flush=True)
def call(name,args=None):
 d=json.loads((root/f'endpoints/sv2-mcp-{p.pid}.json').read_text());req=urllib.request.Request(d['url'],data=json.dumps({'jsonrpc':'2.0','id':1,'method':'tools/call','params':{'name':name,'arguments':args or {}}}).encode(),headers={'Content-Type':'application/json','Authorization':'Bearer '+d['token']});return json.load(urllib.request.urlopen(req,timeout=8))['result']['structuredContent']
deadline=time.monotonic()+45;inventory=None
while p.poll() is None and time.monotonic()<deadline:
 try:
  if call('sv_status').get('editorReady'):
   inventory=call('sv_list_voicebanks')
   if inventory.get('ok') and len(inventory['result']['voicebanks'])>=2:break
 except (OSError,KeyError):pass
 time.sleep(.2)
assert inventory and inventory.get('ok') and len(inventory['result']['voicebanks'])>=2,'Inspect same live host '+str(p.pid)
checks={'samePIDLauncherAndHost':call('sv_status')['hostPid']==p.pid,'voiceInventoryAvailable':True};voices=inventory['result']['voicebanks'];readable=True
for i in range(100):
 current=call('sv_read_voicebank');editor=call('sv_read_editor');readable=readable and current.get('ok') and editor.get('ok') and isinstance(editor['result'].get('voiceParameters'),dict)
checks['hundredNativeReturnReads']=bool(readable);selected=True;readback=True
for i in range(8):
 for voice in voices[:2]:
  current=call('sv_read_voicebank');answer=call('sv_select_voicebank',{'voicebankId':voice['voicebankId'],'guard':current['result']['guard']});selected=selected and answer.get('ok');current=call('sv_read_voicebank');readback=readback and current.get('ok') and current['result']['voicebank']['identityKey']==voice['voicebankId']
checks['sixteenVoiceSelections']=bool(selected);checks['selectionReadbackMatches']=bool(readback);checks['hostAliveAfterReturns']=p.poll() is None;checks['adapterInstalled']='VOICE_HOST identity-adapter-installed catalog=1' in (root/'logs/last-launch.log').read_text(errors='replace');report=dict(handle,checks=checks,passed=all(checks.values()),coreSHA256=hashlib.sha256((app/'Contents/Frameworks/OpenSV2Loader.dylib').read_bytes()).hexdigest(),scope='Real supported SV2 2.3.0 ARM native-return veneer via official Lua/MCP; 100 voice/editor reads and 16 voice selections in a fresh disposable blank project.');(run/'result.json').write_text(json.dumps(report,indent=2));(base/'work/voice-abi/host-results.json').write_text(json.dumps(report,indent=2));print(json.dumps(report,indent=2));assert report['passed'];p.terminate();p.wait(timeout=8)
