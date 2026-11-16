import pathlib,json,http.client,urllib.parse,time,os
base=pathlib.Path(os.environ.get('LOADER_LUA_VALIDATION_DIR',str(pathlib.Path.cwd()/'work/lua-extend-validation')));info=json.loads((base/'host.json').read_text());root=pathlib.Path(info['data']);d=json.loads((root/f'endpoints/sv2-mcp-{info["pid"]}.json').read_text());port=urllib.parse.urlsplit(d['url']).port
responses=[]
def call(name,args):
 c=http.client.HTTPConnection('127.0.0.1',port,timeout=10);start=time.monotonic();c.request('POST','/mcp',json.dumps({'jsonrpc':'2.0','id':len(responses)+1,'method':'tools/call','params':{'name':name,'arguments':args}}),{'Authorization':'Bearer '+d['token'],'Content-Type':'application/json'});r=c.getresponse();v=json.loads(r.read());c.close();value=v.get('result',{}).get('structuredContent',v);responses.append({'name':name,'elapsed_ms':round((time.monotonic()-start)*1000,2),'response':value});return value
checks={}
try:
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
finally:
 report={'pid':info['pid'],'checks':checks,'responses':responses,'passed':len(checks)==8 and all(checks.values()),'transport':'resident-native-call','no_file_or_pipe_requests':True};(root/'live-write-results.json').write_text(json.dumps(report,indent=2));print(json.dumps(report,indent=2))
