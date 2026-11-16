from pathlib import Path
import json,os,urllib.request,time,shutil,uuid,hashlib,wave
base=Path(__file__).resolve().parents[4];h=json.loads((base/'work/release-0.7.0/live-host.json').read_text());pid=h['pid'];os.kill(pid,0);root=Path(h['dataDirectory']);run=root/'validation/0.7.0'/uuid.uuid4().hex;run.mkdir(parents=True,mode=0o700);project=run/'export-fixture.svp';shutil.copy2(base/'work/export-native/test-render.svp',project);handle={'pid':pid,'root':str(root),'runDirectory':str(run),'project':str(project)};(base/'work/release-0.7.0/editor-host.json').write_text(json.dumps(handle,indent=2));print(json.dumps(handle),flush=True)
def call(name,args=None):
 d=json.loads((root/f'endpoints/sv2-mcp-{pid}.json').read_text());req=urllib.request.Request(d['url'],data=json.dumps({'jsonrpc':'2.0','id':1,'method':'tools/call','params':{'name':name,'arguments':args or {}}}).encode(),headers={'Content-Type':'application/json','Authorization':'Bearer '+d['token']});return json.load(urllib.request.urlopen(req,timeout=8))['result']['structuredContent']
deadline=time.monotonic()+420;editor=None
while time.monotonic()<deadline:
 os.kill(pid,0);editor=call('sv_read_editor')
 if editor.get('ok') and editor['result']['noteCount']==1 and editor['result']['notes'][0]['midiPitch']==60:break
 time.sleep(.2)
assert editor and editor.get('result',{}).get('notes',[{}])[0].get('midiPitch')==60,'Inspect same host'
checks={};before=editor['result'];changed=call('sv_set_note_pitch',{'noteIndex':1,'midiPitch':61,'guard':before['guard']});checks['pitchWrite']=changed.get('ok') is True;now=call('sv_read_editor');checks['pitchReadback']=now.get('ok') and now['result']['notes'][0]['midiPitch']==61;restored=call('sv_set_note_pitch',{'noteIndex':1,'midiPitch':60,'guard':now['result']['guard']});checks['pitchRestored']=restored.get('ok') and call('sv_read_editor')['result']['notes'][0]['midiPitch']==60
voices=call('sv_list_voicebanks')['result']['voicebanks'];voice=call('sv_read_voicebank')['result'];original=voice['voicebank']['identityKey'];other=next(v for v in voices if v['identityKey']!=original);selected=call('sv_select_voicebank',{'voicebankId':other['voicebankId'],'guard':voice['guard']});checks['voiceWrite']=selected.get('ok') is True;voice=call('sv_read_voicebank')['result'];checks['voiceReadback']=voice['voicebank']['identityKey']==other['voicebankId'];restored=call('sv_select_voicebank',{'voicebankId':original,'guard':voice['guard']});checks['voiceRestored']=restored.get('ok') is True
snapshot=call('sv_export_snapshot');assert snapshot.get('ok'),snapshot;target=run/'installed-render.wav';job=call('sv_export_wav',{'guard':snapshot['result']['guard'],'outputPath':str(target),'channels':1,'sampleRate':48000,'bitDepth':24});assert job.get('ok'),job;deadline=time.monotonic()+45;status=None
while time.monotonic()<deadline:
 status=call('sv_export_status',{'jobId':job['result']['jobId']})
 if status.get('result',{}).get('state') in ['completed','cancelled','failed']:break
 time.sleep(.15)
checks['WAVCompleted']=status.get('result',{}).get('state')=='completed';checks['WAVBaselineIdentical']=target.exists() and hashlib.sha256(target.read_bytes()).hexdigest()=='d93caf0b01d5af2d0b1cb7d936152f1a06937254cfedf94014a5f70e1a379d6e'
if target.exists():
 with wave.open(str(target)) as wav:checks['WAVFormat']=wav.getnchannels()==1 and wav.getframerate()==48000 and wav.getsampwidth()==3
snapshot=call('sv_export_snapshot');again=call('sv_export_wav',{'guard':snapshot['result']['guard'],'outputPath':str(target),'channels':1,'sampleRate':48000,'bitDepth':24});checks['noOverwrite']=again.get('error',{}).get('code')=='OUTPUT_EXISTS';os.kill(pid,0);checks['hostAlive']=True
report=dict(handle,checks=checks,passed=all(checks.values()),scope='Actual installed 0.7.0 resident MCP/official Lua provider, pitch and voice write/readback/restore plus baseline-identical native WAV, in disposable project copy.');(run/'result.json').write_text(json.dumps(report,indent=2));(base/'work/release-0.7.0/installed-editor-results.json').write_text(json.dumps(report,indent=2));print(json.dumps(report,indent=2));assert report['passed']
