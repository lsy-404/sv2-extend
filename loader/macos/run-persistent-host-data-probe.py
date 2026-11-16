from pathlib import Path
import json,subprocess,plistlib,uuid,shutil,os,time,urllib.request,hashlib
base=Path(__file__).resolve().parents[4];probe=base/'work/persistent-host-data/Extend Loader Persistent Probe.app';run=Path.home()/'Library/Application Support/Dreamtonics/Open SV2 Loader/experiments/persistent-host-data'/uuid.uuid4().hex;run.mkdir(parents=True,mode=0o700);domain='io.opensv2.loader.persistent-test.p'+run.name;infoPath=probe/'Contents/Info.plist';info=plistlib.loads(infoPath.read_bytes());info['CFBundleIdentifier']=domain;infoPath.write_bytes(plistlib.dumps(info));subprocess.run(['codesign','--force','--sign','-',str(probe)],capture_output=True,check=True);exe=probe/'Contents/MacOS/ExtendLoader'
def pref(name):
 p=subprocess.run(['defaults','read',name,'loaderDataDirectory'],capture_output=True,text=True);return p.stdout.strip() if p.returncode==0 else None
productionPref=pref('io.opensv2.extend-loader');production=Path.home()/'Library/Application Support/Dreamtonics/Open SV2 Loader/settings.json';prodBefore=production.read_bytes();entry=Path.home()/'Library/Application Support/Dreamtonics/Synthesizer V Studio 2/scripts/OpenSV2LoaderRuntime.lua';entryBefore=entry.read_bytes()
def cli(*args,ok=True):
 p=subprocess.run([str(exe),*args],capture_output=True,text=True);assert (p.returncode==0)==ok,p.stderr;return p
fresh=run/'fresh';fresh.mkdir();subprocess.run(['defaults','write',domain,'loaderDataDirectory',str(fresh)],check=True);checks={};checks['freshDefaultsToIsolation']=json.loads(cli('--status').stdout)['hostDataNamespace'] is True;cli('--configure-only');checks['freshDefaultPersists']=json.loads((fresh/'settings.json').read_text())['hostDataNamespace'] is True
legacy=run/'legacy';legacy.mkdir();legacySettings={'schema':1,'sourceApplication':'/Applications/Synthesizer V Studio 2 Pro.app','dataMode':'independent','dataDirectory':str(legacy)};legacyFile=legacy/'settings.json';legacyFile.write_text(json.dumps(legacySettings));legacyBefore=legacyFile.read_bytes();cli('--data-dir',str(legacy),'--configure-only');checks['legacyActivationPreservesBytes']=legacyFile.read_bytes()==legacyBefore;checks['legacyMissingFieldDecodes']=json.loads(cli('--status').stdout).get('hostDataNamespace') is None
cli('--data-dir',str(fresh),'--configure-only');cli('--data-dir',str(legacy),'--isolate-host-data','--script-http-online','deny','--configure-only');settings=json.loads(legacyFile.read_text());checks['existingProfileExplicitOptionsApplied']=settings['hostDataNamespace'] is True and settings['scriptHTTPOnline'] is False
before=legacyFile.read_bytes();cli('--isolate-host-data','--legacy-host-data','--configure-only',ok=False);checks['conflictingOptionsPreserveSettings']=legacyFile.read_bytes()==before
# No experimental flag for preparation or launch.
app=Path(cli('--prepare-only').stdout.strip());meta=plistlib.loads((app/'Contents/Info.plist').read_bytes());checks['ordinaryPrepareBindsIsolation']=meta['OpenSV2HostDataNamespace'] is True and meta['OpenSV2LoaderDataDirectory']==str(legacy);checks['ordinaryPrepareUsesProfileIdentity']=meta['CFBundleIdentifier'].startswith('io.opensv2.extended-host.p')
product=legacy/'host-home/Library/Application Support/Dreamtonics/Synthesizer V Studio 2';sentinel=product/'persistent-test-marker';sentinel.write_text('preserve');again=Path(cli('--prepare-only').stdout.strip());checks['ordinaryPrepareReusesRuntime']=again==app;checks['repreparePreservesPrivateData']=sentinel.read_text()=='preserve';checks['privateLuaEntryInstalled']=(product/'scripts/OpenSV2LoaderRuntime.lua').exists()
shutil.copy2(base/'work/host-data-integration/host-data-fixture.dylib',app/'Contents/PlugIns/network-plugin.dylib');subprocess.run(['codesign','--force','--sign','-',str(app/'Contents/PlugIns/network-plugin.dylib')],check=True,capture_output=True);subprocess.run(['codesign','--force','--sign','-','--identifier',meta['CFBundleIdentifier'],'--options','runtime','--entitlements',str(app.parent/'entitlements.plist'),str(app)],check=True,capture_output=True)
env=dict(os.environ,OPEN_SV2_NETWORK_DIAGNOSTICS='1',LOADER_HOST_DATA_RESULT=str(run/'native-directory.json'),LOADER_HOST_DATA_SENTINEL='persistent-host-'+run.name);env.pop('OPEN_SV2_LOADER_DATA_DIR',None);env.pop('OPEN_SV2_HOST_DATA_HOME',None);env.pop('OPEN_SV2_ISOLATE_HOST_DATA',None)
with (run/'stdout.log').open('wb') as out,(run/'stderr.log').open('wb') as err:p=subprocess.Popen([str(exe)],env=env,stdout=out,stderr=err,start_new_session=True)
handle={'pid':p.pid,'root':str(legacy),'app':str(app),'runDirectory':str(run),'probeApp':str(probe),'domain':domain};(base/'work/persistent-host-data/host.json').write_text(json.dumps(handle,indent=2));print(json.dumps(handle),flush=True)
def call(name):
 d=json.loads((legacy/f'endpoints/sv2-mcp-{p.pid}.json').read_text());req=urllib.request.Request(d['url'],data=json.dumps({'jsonrpc':'2.0','id':1,'method':'tools/call','params':{'name':name,'arguments':{}}}).encode(),headers={'Content-Type':'application/json','Authorization':'Bearer '+d['token']});return json.load(urllib.request.urlopen(req,timeout=8))['result']['structuredContent']
deadline=time.monotonic()+45;status=None;voices=None
while p.poll() is None and time.monotonic()<deadline:
 try:
  status=call('sv_status')
  if status.get('editorReady'):
   voices=call('sv_list_voicebanks')
   if voices.get('ok') and len(voices['result']['voicebanks'])>=2 and (run/'native-directory.json').exists():break
 except (OSError,KeyError):pass
 time.sleep(.2)
checks['ordinaryLaunchSamePID']=bool(status and status.get('hostPid')==p.pid);checks['ordinaryLaunchEditorReady']=bool(status and status.get('editorReady'));checks['ordinaryLaunchTwoVoices']=bool(voices and voices.get('ok') and len(voices['result']['voicebanks'])>=2)
if (run/'native-directory.json').exists():
 value=json.loads((run/'native-directory.json').read_text());checks.update(value['checks']);checks['nativeFixtureSamePID']=value['pid']==p.pid
else:checks['nativeFixtureAvailable']=False
checks['stablePreferencesPreserved']=pref('io.opensv2.extend-loader')==productionPref;checks['stableSettingsPreserved']=production.read_bytes()==prodBefore;checks['originalRuntimeEntryPreserved']=entry.read_bytes()==entryBefore;checks['hostAlive']=p.poll() is None
report=dict(handle,checks=checks,checkCount=len(checks),passed=all(checks.values()),sourceSHA256=hashlib.sha256((base/'projects/sv2-extend/loader/macos/data_launcher.swift').read_bytes()).hexdigest(),scope='Actual persistent settings, legacy decode/activation, explicit selected-profile overrides and ordinary no-argument same-PID launch in private native data directory. Current supported SV2 build only.');(run/'result.json').write_text(json.dumps(report,indent=2));(base/'work/persistent-host-data/results.json').write_text(json.dumps(report,indent=2));print(json.dumps(report,indent=2));assert report['passed'];p.terminate();p.wait(timeout=8)
