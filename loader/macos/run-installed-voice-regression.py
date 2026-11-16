"""Read/select voices through a copied installed Loader in a disposable profile.

No candidate Core replacement, original profile migration, or account operations.
Incomplete observation leaves the same host alive for inspection.
"""
from pathlib import Path
import hashlib,json,os,plistlib,shutil,subprocess,time,urllib.request,uuid

source=Path(__file__).resolve().parent
work=source.parents[3]/'work/installed-voice-regression'
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
domain='io.opensv2.installed-voice-test.p'+run.name
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
with (run/'stdout.log').open('wb') as out,(run/'stderr.log').open('wb') as err:
    process=subprocess.Popen(args,env=env,stdout=out,stderr=err,start_new_session=True)
handle={'pid':process.pid,'runDirectory':str(run),'profile':str(root),'managedApplication':str(managed),'coreSHA256':core_hash}
(work/'live-host.json').write_text(json.dumps(handle,indent=2))
print(json.dumps(handle),flush=True)
def call(name,arguments=None):
    endpoint=json.loads((root/f'endpoints/sv2-mcp-{process.pid}.json').read_text())
    request=urllib.request.Request(endpoint['url'],data=json.dumps({'jsonrpc':'2.0','id':1,'method':'tools/call','params':{'name':name,'arguments':arguments or {}}}).encode(),headers={'Content-Type':'application/json','Authorization':'Bearer '+endpoint['token']})
    return json.load(urllib.request.urlopen(request,timeout=8))['result']['structuredContent']
deadline=time.monotonic()+60
inventory=None
while process.poll() is None and time.monotonic()<deadline:
    try:
        if call('sv_status').get('editorReady'):
            inventory=call('sv_list_voicebanks')
            if inventory.get('ok') and len(inventory['result']['voicebanks'])>=2:break
    except (OSError,KeyError,json.JSONDecodeError):pass
    time.sleep(.2)
assert inventory and inventory.get('ok') and len(inventory['result']['voicebanks'])>=2,'Inspect same live PID '+str(process.pid)
checks={'samePID':call('sv_status')['hostPid']==process.pid,'installedCoreUnmodified':digest(installed/core_relative)==core_hash,'copiedCoreUnmodified':digest(probe/core_relative)==core_hash}
checks['hundredNativeReads']=all(call('sv_read_voicebank').get('ok') and call('sv_read_editor').get('ok') for _ in range(100))
selected=True;readback=True
for _ in range(8):
    for voice in inventory['result']['voicebanks'][:2]:
        current=call('sv_read_voicebank')
        answer=call('sv_select_voicebank',{'voicebankId':voice['voicebankId'],'guard':current['result']['guard']})
        selected=selected and answer.get('ok') is True
        current=call('sv_read_voicebank')
        readback=readback and current.get('ok') and current['result']['voicebank']['identityKey']==voice['voicebankId']
checks['sixteenSelections']=selected
checks['selectionReadback']=bool(readback)
checks['hostAlive']=process.poll() is None
report={'handle':handle,'checks':checks,'passed':all(checks.values()),'scope':'Copied installed Loader with identical Core and bundled public plugins, unique preferences and disposable isolated blank project; resident MCP/native Lua voice adapter. Not direct user Lua/JS script invocation, native export, account switching or full network proof.'}
(run/'results.json').write_text(json.dumps(report,indent=2))
(work/'results.json').write_text(json.dumps(report,indent=2))
print(json.dumps({'checks':checks,'passed':report['passed']},indent=2),flush=True)
assert report['passed'],'Inspect same live host before cleanup'
process.terminate();process.wait(timeout=8)
subprocess.run(['defaults','delete',domain],check=True,capture_output=True)
