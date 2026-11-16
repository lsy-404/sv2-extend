from pathlib import Path
import subprocess,json,plistlib,uuid,os,shutil,time,urllib.request,socket,urllib.error
source=Path(__file__).resolve().parent;base=source.parents[3];work=base/'work/runtime-settings';work.mkdir(exist_ok=True)
subprocess.run(['xcrun','clang','-dynamiclib','-fobjc-arc','-Wall','-Wextra','-Werror',str(source/'runtime_settings_fixture.m'),'-framework','Foundation','-o',str(work/'fixture.dylib')],check=True)
run=Path.home()/'Library/Application Support/Dreamtonics/Open SV2 Loader/experiments/runtime-settings'/uuid.uuid4().hex;run.mkdir(parents=True,mode=0o700);root=run/'profile';probe=work/('Extend Settings '+run.name+'.app');subprocess.run(['ditto',str(Path.home()/'Applications/Extend Loader.app'),str(probe)],check=True)
installed_mode=os.environ.get('LOADER_SETTINGS_USE_INSTALLED')=='1'
if not installed_mode:
 for name in ['loader-probe.dylib','sv2-mcp.dylib']:shutil.copy2(source/name,probe/'Contents/Resources/Native'/name)
# Use the launcher's explicit diagnostic slot in this isolated test bundle.
# Production launcher deliberately ignores arbitrary LOADER_PROBE_PLUGIN paths.
shutil.copy2(work/'fixture.dylib',probe/'Contents/Resources/Native/network-plugin.dylib')
domain='io.opensv2.settings-probe.p'+run.name;infoPath=probe/'Contents/Info.plist';info=plistlib.loads(infoPath.read_bytes());info['CFBundleIdentifier']=domain;infoPath.write_bytes(plistlib.dumps(info));subprocess.run(['codesign','--force','--sign','-',str(probe)],capture_output=True,check=True)
empty=run/'empty';empty.mkdir();subprocess.run(['defaults','write',domain,'loaderDataDirectory',str(empty)],check=True);exe=probe/'Contents/MacOS/ExtendLoader'
r=subprocess.run([str(exe),'--data-dir',str(root),'--isolate-host-data','--prepare-only'],capture_output=True,text=True);assert r.returncode==0,r.stderr;managed=Path(r.stdout.strip());env=dict(os.environ,OPEN_SV2_UI_DIAGNOSTICS="1",OPEN_SV2_NETWORK_DIAGNOSTICS="1")
env.pop('OPEN_SV2_LOADER_DATA_DIR',None)
with (run/'stdout.log').open('wb') as out,(run/'stderr.log').open('wb') as err:p=subprocess.Popen([str(exe)],env=env,stdout=out,stderr=err,start_new_session=True)
h={'pid':p.pid,'root':str(root),'managedApplication':str(managed),'probeApplication':str(probe),'runDirectory':str(run)};(work/'live-host.json').write_text(json.dumps(h,indent=2));print(json.dumps(h),flush=True)
control=root/f'endpoints/settings-probe-{p.pid}.json';mcp=root/f'endpoints/sv2-mcp-{p.pid}.json';deadline=time.monotonic()+35
while p.poll() is None and not (control.exists() and mcp.exists()) and time.monotonic()<deadline:time.sleep(.1)
if not(control.exists() and mcp.exists()):raise RuntimeError('Inspect same host PID '+str(p.pid))
def request(endpoint,body):
 req=urllib.request.Request(endpoint['url'],data=json.dumps(body).encode(),headers={'Content-Type':'application/json','Authorization':'Bearer '+endpoint['token']})
 return json.load(urllib.request.urlopen(req,timeout=8))
def script(method,arguments=None,js=False):return request(json.loads(control.read_text()),{'method':method,'arguments':arguments or {},'javascript':js})
def snapshot():return script('ui.snapshot')['result']
def setting(id,key,value):return script('ui.set_setting',{'id':id,'key':key,'value':value,'generation':snapshot()['generation']})
def status():return request(json.loads(mcp.read_text()),{'jsonrpc':'2.0','id':1,'method':'tools/call','params':{'name':'sv_status','arguments':{}}})['result']['structuredContent']
checks={};original=snapshot()
while time.monotonic()<deadline:
 try:
  value=status()
  if value.get('editorReady'):break
 except (OSError,KeyError):pass
 time.sleep(.15)
checks['baselineMCPReady']=bool(value.get('editorReady'))
checks['javascriptDisabledSaved']=setting('opensv2.javascript-extend','enabled',False)['ok']
checks['javascriptDisabledEnforced']=script('runtime.capabilities',js=True).get('error',{}).get('code')=='EXTENSION_DISABLED'
checks['luaIndependent']=script('runtime.capabilities')['ok'];setting('opensv2.javascript-extend','enabled',True)
for method in ['voicebanks.current','voicebanks.list','voicebanks.select','exports.snapshot','exports.start','exports.status','exports.cancel','selection.read','notes.set_pitch','pitch.read_range','pitch.replace_range','transport.status','transport.play','transport.stop','http.start','http.status','http.cancel']:
 checks[method+'Saved']=setting('opensv2.lua-extend',method,False)['ok'];checks[method+'Enforced']=script('call.submit',{'method':method,'arguments':{}}).get('error',{}).get('code')=='FEATURE_DISABLED';setting('opensv2.lua-extend',method,True)
checks['jsFunctionSaved']=setting('opensv2.javascript-extend','voicebanks.list',False)['ok'];checks['jsFunctionEnforced']=script('call.submit',{'method':'voicebanks.list','arguments':{}},True).get('error',{}).get('code')=='FEATURE_DISABLED';setting('opensv2.javascript-extend','voicebanks.list',True)
checks['luaDisabledSaved']=setting('opensv2.lua-extend','enabled',False)['ok'];checks['luaDisabledEnforced']=script('call.submit',{'method':'voicebanks.list','arguments':{}}).get('error',{}).get('code')=='EXTENSION_DISABLED';checks['mcpEditorSurvivesLuaDisabled']=bool(status().get('editorReady'))
voice=request(json.loads(mcp.read_text()),{'jsonrpc':'2.0','id':2,'method':'tools/call','params':{'name':'sv_list_voicebanks','arguments':{}}})
checks['mcpVoiceSurvivesLuaDisabled']=bool(voice.get('result',{}).get('structuredContent',{}).get('ok'))
exported=request(json.loads(mcp.read_text()),{'jsonrpc':'2.0','id':5,'method':'tools/call','params':{'name':'sv_export_snapshot','arguments':{}}})
checks['mcpExportSurvivesLuaDisabled']=bool(exported.get('result',{}).get('structuredContent',{}).get('ok'));setting('opensv2.lua-extend','enabled',True)
checks['mcpToolSaved']=setting('opensv2.mcp','tool.sv_list_voicebanks',False)['ok']
listed=request(json.loads(mcp.read_text()),{'jsonrpc':'2.0','id':3,'method':'tools/list'})
checks['mcpToolHiddenFromList']=not any(t['name']=='sv_list_voicebanks' for t in listed['result']['tools'])
denied=request(json.loads(mcp.read_text()),{'jsonrpc':'2.0','id':4,'method':'tools/call','params':{'name':'sv_list_voicebanks','arguments':{}}})
checks['mcpToolCallDenied']=denied.get('error',{}).get('code')==-32002;setting('opensv2.mcp','tool.sv_list_voicebanks',True)
old=json.loads(mcp.read_text());checks['mcpDisabledSaved']=setting('opensv2.mcp','enabled',False)['ok'];checks['mcpDiscoveryRemoved']=not mcp.exists()
port=int(old['url'].split(':')[2].split('/')[0]);deadline2=time.monotonic()+2;refused=False
while time.monotonic()<deadline2:
 try:
  with socket.create_connection(('127.0.0.1',port),timeout=.2):pass
 except OSError:refused=True;break
 time.sleep(.05)
checks['mcpListenerClosed']=refused;checks['mcpRestoredSaved']=setting('opensv2.mcp','enabled',True)['ok'];new=json.loads(mcp.read_text());checks['mcpCredentialsRotated']=new['token']!=old['token'];checks['mcpRestoredReady']=bool(status().get('editorReady'))
for plugin in original['plugins']:
 if plugin['id'].startswith('opensv2.') and plugin['values']:script('ui.set_settings',{'id':plugin['id'],'generation':snapshot()['generation'],'settings':plugin['values']})
checks['entryHiddenSaved']=script('ui.set_extend_visible',{'generation':snapshot()['generation'],'visible':False})['ok'];checks['entryHidden']=not snapshot()['extendEntryVisible'];checks['hostAlive']=p.poll() is None
if os.environ.get('LOADER_SETTINGS_CLOSE_AFTER_TESTS')=='1':
 p.terminate();p.wait(timeout=8)
if p.poll() is not None:subprocess.run(['defaults','delete',domain],check=True,capture_output=True)
report={'installedBundledModulesUnreplaced':installed_mode,'testHostExited':p.poll() is not None,'handle':h,'checks':checks,'passed':all(checks.values()),'scope':'Actual resident per-function script gate, independent MCP tools and real listener stop/restart. '+('Test host terminated after completion.' if p.poll() is not None else 'Host remains live for CUA verification.')};(work/'host-results.json').write_text(json.dumps(report,indent=2));print(json.dumps({'checks':checks,'passed':report['passed']},indent=2));assert report['passed']
