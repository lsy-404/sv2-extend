"""Read/select voices through a copied installed Loader in a disposable profile.

No candidate Core replacement, original profile migration, or account operations.
Incomplete observation leaves the same host alive for inspection.
"""
from pathlib import Path
import hashlib,json,os,plistlib,shutil,subprocess,time,urllib.request,uuid

source=Path(__file__).resolve().parent
work=source.parents[3]/'work/installed-pitch-regression'
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
domain='io.opensv2.installed-pitch-test.p'+run.name
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
scripts=root/'host-home/Library/Application Support/Dreamtonics/Synthesizer V Studio 2/scripts';scripts.mkdir(parents=True,exist_ok=True)
fixture="""function getClientInfo() return {name='Extend Pitch Fixture',author='Open SV2',category='Extend Acceptance',versionNumber=1,minEditorVersion=131840} end
function main()
 assert(SV:getProject():getFileName()=='','Blank owned project required')
 local track=SV:getMainEditor():getCurrentTrack();assert(track:getNumGroups()==1,'Empty test track required');local g=SV:create('NoteGroup');g:setName('Extend pitch acceptance')
 SV:getProject():newUndoRecord();local n=SV:create('Note');n:setOnset(0);n:setDuration(SV.QUARTER);n:setPitch(60);n:setLyrics('la');g:addNote(n);SV:getProject():addNoteGroup(g);local ref=SV:create('NoteGroupReference');ref:setTarget(g);track:addGroupReference(ref);SV:finish()
end
"""
(scripts/'ExtendPitchFixture.lua').write_text(fixture)
with (run/'stdout.log').open('wb') as out,(run/'stderr.log').open('wb') as err:
 process=subprocess.Popen(args,env=env,stdout=out,stderr=err,start_new_session=True)
handle={'pid':process.pid,'runDirectory':str(run),'profile':str(root),'managedApplication':str(managed),'coreSHA256':core_hash,'preferencesDomain':domain}
(work/'live-host.json').write_text(json.dumps(handle,indent=2));print(json.dumps(handle),flush=True)
responses=[]
def call(name,arguments=None):
 endpoint=json.loads((root/f'endpoints/sv2-mcp-{process.pid}.json').read_text())
 request=urllib.request.Request(endpoint['url'],data=json.dumps({'jsonrpc':'2.0','id':1,'method':'tools/call','params':{'name':name,'arguments':arguments or {}}}).encode(),headers={'Content-Type':'application/json','Authorization':'Bearer '+endpoint['token']})
 answer=json.load(urllib.request.urlopen(request,timeout=8))['result']['structuredContent'];responses.append({'method':name,'response':answer});return answer
deadline=time.monotonic()+300;ready=False
while process.poll() is None and time.monotonic()<deadline:
 try:
  value=call('sv_read_editor')
  if value.get('ok') and value['result']['noteCount']==1:ready=True;break
 except (OSError,KeyError,json.JSONDecodeError):pass
 time.sleep(.2)
assert ready,'Inspect same host PID '+str(process.pid)
checks={}
read=call('sv_read_editor',{});assert read['ok'];selection=read['result'];print('selection',selection)
assert selection['noteCount']==1,'Select the dedicated one-note test group first'
before=selection['notes'][0]['midiPitch'];guard=selection['guard']
updated=call('sv_set_note_pitch',{'noteIndex':1,'midiPitch':before+2,'guard':guard});checks['note-write']=updated.get('ok') and updated['result']['after']==before+2
readback=call('sv_read_editor',{});checks['note-readback']=readback['result']['notes'][0]['midiPitch']==before+2
stale=call('sv_set_note_pitch',{'noteIndex':1,'midiPitch':before+3,'guard':guard});checks['stale-note-guard-rejected']=not stale['ok']
restore=call('sv_set_note_pitch',{'noteIndex':1,'midiPitch':before,'guard':readback['result']['guard']});checks['note-restored']=restore.get('ok') and restore['result']['after']==before
curve=call('sv_read_pitch_range',{'startBlick':0,'endBlick':705600000})['result']
points=[[0,0],[352800000,100],[705600000,0]]
args={'startBlick':0,'endBlick':705600000,'points':points,'guard':curve['guard'],'curveGuard':curve['curveGuard']}
change=call('sv_set_pitch_curve',args);checks['curve-write']=change.get('ok') and change['result']['after']==points
reread=call('sv_read_pitch_range',{'startBlick':0,'endBlick':705600000})['result'];checks['curve-readback']=reread['points']==points
stale=call('sv_set_pitch_curve',args);checks['stale-curve-guard-rejected']=not stale['ok']
args.update(points=curve['points'],guard=reread['guard'],curveGuard=reread['curveGuard']);restored=call('sv_set_pitch_curve',args);checks['curve-restored']=restored.get('ok') and restored['result']['after']==curve['points']
checks['hostAlive']=process.poll() is None
report={'handle':handle,'checks':checks,'passed':all(checks.values()),'responses':responses,'scope':'Copied installed Core/MCP, official Lua-created one-note disposable blank project; real note and pitch curve writes/readback/stale rejection/restoration. No user file saved; not native Undo or whole-host isolation proof.'}
(run/'results.json').write_text(json.dumps(report,indent=2));(work/'results.json').write_text(json.dumps(report,indent=2));print(json.dumps({'checks':checks,'passed':report['passed']},indent=2),flush=True)
assert report['passed'],'Inspect same host before cleanup'
process.terminate();process.wait(timeout=8)
subprocess.run(['defaults','delete',domain],check=True,capture_output=True)
