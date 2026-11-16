from pathlib import Path
import os,json,plistlib,subprocess,uuid,time,shutil,http.server,threading,hashlib
base=Path(__file__).resolve().parents[4];app=base/'work/script-http-settings/Extend Loader Policy Probe.app';run=Path.home()/'Library/Application Support/Dreamtonics/Open SV2 Loader/experiments/script-http-policy'/uuid.uuid4().hex;run.mkdir(parents=True,mode=0o700);root=run/'profile';root.mkdir();domain='io.opensv2.loader.policy-test.p'+run.name
infofile=app/'Contents/Info.plist';info=plistlib.loads(infofile.read_bytes());info['CFBundleIdentifier']=domain;infofile.write_bytes(plistlib.dumps(info));subprocess.run(['codesign','--force','--sign','-',str(app)],check=True,capture_output=True);exe=app/'Contents/MacOS/ExtendLoader'
def pref(name):
 p=subprocess.run(['defaults','read',name,'loaderDataDirectory'],capture_output=True,text=True);return p.stdout.strip() if p.returncode==0 else None
before=pref('io.opensv2.extend-loader');production=Path.home()/'Library/Application Support/Dreamtonics/Open SV2 Loader/settings.json';settingsBefore=production.read_bytes();officialEntry=Path.home()/'Library/Application Support/Dreamtonics/Synthesizer V Studio 2/scripts/OpenSV2LoaderRuntime.lua';entryBefore=officialEntry.read_bytes()
legacy={'schema':1,'sourceApplication':'/Applications/Synthesizer V Studio 2 Pro.app','dataMode':'independent','dataDirectory':str(root)};(root/'settings.json').write_text(json.dumps(legacy));subprocess.run(['defaults','write',domain,'loaderDataDirectory',str(root)],check=True)
def cli(*args,ok=True):
 p=subprocess.run([str(exe),*args],capture_output=True,text=True);assert (p.returncode==0)==ok,p.stderr;return p
checks={};old=json.loads(cli('--status').stdout);checks['legacySettingsAccepted']='scriptHTTPLocal' not in old
bad=cli('--script-http-local','maybe','--configure-only',ok=False);checks['invalidOptionDoesNotWrite']=json.loads((root/'settings.json').read_text())==legacy
seen=[]
class Handler(http.server.BaseHTTPRequestHandler):
 def log_message(self,*args):pass
 def do_GET(self):seen.append(self.path);self.send_response(200);self.send_header('Content-Length','2');self.end_headers();self.wfile.write(b'OK')
server=http.server.ThreadingHTTPServer(('127.0.0.1',0),Handler);threading.Thread(target=server.serve_forever,daemon=True).start();apps=[]
for label,local,online in [('localOnly',True,False),('onlineOnly',False,True)]:
 cli('--script-http-local','allow' if local else 'deny','--script-http-online','allow' if online else 'deny','--configure-only');saved=json.loads((root/'settings.json').read_text());checks[label+'Persists']=saved['scriptHTTPLocal']==local and saved['scriptHTTPOnline']==online;status=json.loads(cli('--status').stdout);checks[label+'Reloads']=status==saved
 managed=Path(cli('--experimental-host-data','--prepare-only').stdout.strip());apps.append(managed);meta=plistlib.loads((managed/'Contents/Info.plist').read_bytes());checks[label+'BundlePolicy']=meta['OpenSV2ScriptHTTPLocal']==local and meta['OpenSV2ScriptHTTPOnline']==online
 shutil.copy2(base/'work/script-http-settings/fixture.dylib',managed/'Contents/PlugIns/network-plugin.dylib');subprocess.run(['codesign','--force','--sign','-',str(managed/'Contents/PlugIns/network-plugin.dylib')],check=True,capture_output=True);subprocess.run(['codesign','--force','--sign','-','--identifier',meta['CFBundleIdentifier'],'--options','runtime','--entitlements',str(managed.parent/'entitlements.plist'),str(managed)],check=True,capture_output=True)
 for mode in ['loader','direct']:
  result=run/(label+'-'+mode+'.json');env=dict(os.environ,OPEN_SV2_NETWORK_DIAGNOSTICS='1',LOADER_POLICY_TEST_RESULT=str(result),LOADER_POLICY_TEST_URL=f'http://127.0.0.1:{server.server_port}/{label}-{mode}',OPEN_SV2_SCRIPT_HTTP_LOCAL='0' if local else '1',OPEN_SV2_SCRIPT_HTTP_ONLINE='0' if online else '1');env.pop('OPEN_SV2_LOADER_DATA_DIR',None)
  command=[str(exe),'--experimental-host-data'] if mode=='loader' else [str(managed/'Contents/MacOS/loader-launcher')]
  with (run/(label+'-'+mode+'.out')).open('wb') as out,(run/(label+'-'+mode+'.err')).open('wb') as err:p=subprocess.Popen(command,env=env,stdout=out,stderr=err,start_new_session=True)
  handle={'pid':p.pid,'root':str(root),'app':str(managed),'runDirectory':str(run),'domain':domain,'probeApp':str(app)};(base/'work/script-http-settings/live-host.json').write_text(json.dumps(handle,indent=2));print(json.dumps(handle),flush=True)
  deadline=time.monotonic()+25
  while p.poll() is None and not result.exists() and time.monotonic()<deadline:time.sleep(.1)
  if not result.exists():raise RuntimeError('Inspect same live host '+str(p.pid))
  value=json.loads(result.read_text());name=label+mode;checks[name+'HostPID']=value['pid']==p.pid;checks[name+'SealedEnvironment']=value['localEnvironment']==('1' if local else '0') and value['onlineEnvironment']==('1' if online else '0')
  for key,allowed in [('local',local),('online',online)]:
   response=value['responses'][key];checks[name+key]=response.get('ok') is True and response['result']['status']==200 if allowed else response.get('error',{}).get('code')=='HTTP_ACCESS_DENIED'
  checks[name+'Alive']=p.poll() is None
  if not all(checks.values()):raise RuntimeError('Failed checks; retain host '+str(p.pid))
  p.terminate();p.wait(timeout=8)
checks['differentPolicyDifferentRuntime']=apps[0]!=apps[1];checks['localDeniedMadeNoRequests']=len(seen)==2;checks['stablePreferencesPreserved']=pref('io.opensv2.extend-loader')==before;checks['stableSettingsPreserved']=production.read_bytes()==settingsBefore;checks['originalScriptPreserved']=officialEntry.read_bytes()==entryBefore;checks['settingsBackupsExist']=len(list((root/'backups').glob('settings-*.json')))>=2
report={'checks':checks,'passed':all(checks.values()),'runDirectory':str(run),'probeApp':str(app),'domain':domain,'scope':'Actual Swift settings persistence, legacy decode, prepare, and same-PID managed host requests through Loader and direct managed bootstrap; no UI or installed update claim.'};(run/'result.json').write_text(json.dumps(report,indent=2));(base/'work/script-http-settings/results.json').write_text(json.dumps(report,indent=2));print(json.dumps(report,indent=2));server.shutdown();assert report['passed']
