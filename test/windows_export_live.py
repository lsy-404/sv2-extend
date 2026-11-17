"""Verify MCP and real script exports through production native job APIs."""
import argparse,json,queue,subprocess,threading,time,wave,struct
from pathlib import Path
ap=argparse.ArgumentParser();ap.add_argument('--client',type=Path,required=True);ap.add_argument('--endpoint',type=Path,required=True);ap.add_argument('--pid',type=int,required=True);a=ap.parse_args()
assert json.loads(a.endpoint.read_text(encoding='utf-8-sig'))['hostPid']==a.pid
profile=a.endpoint.parent.parent
assert profile.is_dir() and (profile / "outputs").is_dir()
out=profile/'outputs'/('production-export-'+str(time.time_ns())+'.wav');assert not out.exists()
p=subprocess.Popen([str(a.client),'--endpoint',str(a.endpoint)],stdin=subprocess.PIPE,stdout=subprocess.PIPE,stderr=subprocess.DEVNULL,text=True,encoding='utf-8');q=queue.Queue()
def reader():
 for line in p.stdout:q.put(json.loads(line))
threading.Thread(target=reader,daemon=True).start();seq=0
def req(m,args):
 global seq
 seq+=1;p.stdin.write(json.dumps({'jsonrpc':'2.0','id':seq,'method':m,'params':args})+'\n');p.stdin.flush();r=q.get(timeout=20);assert r['id']==seq and 'error' not in r,r;return r['result']
def tool(n,args=None):
 r=req('tools/call',{'name':n,'arguments':args or {}})['structuredContent'];assert r['ok'],r;return r['result']
def render(op,**args):
 r=tool('svext_test_parity_render',dict(operation=op,**args));assert 'error' not in r,r;return r
def wait(op,key):
 end=time.monotonic()+10
 while True:
  r=render(op)
  if r.get(key):return r
  assert time.monotonic()<end,r;time.sleep(.1)
group=None;path_scope=False;format_scope=False;dispatched=False
try:
 req('initialize',{'protocolVersion':'2025-11-25'})
 initial=tool('sv_read_editor');assert initial['noteCount']==0 and json.loads(initial['guard'])[1]==''
 voice=tool('sv_read_voicebank')
 assert voice['voicebank']['assigned']
 group=tool('sv_create_group',{'guard':initial['guard'],'name':'Render verification'})['groupUUID']
 g=tool('sv_read_editor',{'groupUUID':group})
 tool('sv_add_note',{'groupUUID':group,'guard':g['guard'],'onsetBlick':0,'durationBlick':1411200000,'midiPitch':60,'lyrics':'la'})
 inherited=tool('sv_read_voicebank',{'groupUUID':group});assert inherited['voicebank']['identityKey']==voice['voicebank']['identityKey']
 print('Temporary group inherits native voice: PASS',flush=True)
 def verify_file(path):
  with wave.open(str(path),'rb') as w:
   frames=w.getnframes();data=w.readframes(frames);result={'channels':w.getnchannels(),'sampleRate':w.getframerate(),'bits':w.getsampwidth()*8,'frames':frames,'nonzero':sum(x!=0 for x in data)}
  assert frames>0 and result['nonzero']>0,result;print('Real production WAV',path.name,result,flush=True)
 snap=tool('sv_export_snapshot');assert not snap['busy'] and not snap['pending']
 job=tool('sv_export_wav',{'guard':snap['guard'],'outputPath':str(out),'channels':1,'sampleRate':48000,'bitDepth':24})
 print('Production native job created',job['state'],flush=True);end=time.monotonic()+20
 while True:
  status=tool('sv_export_status',{'jobId':job['jobId']})
  if status['state']!='running':break
  assert time.monotonic()<end,{'job':status};time.sleep(.1)
 print('Production job terminal',status,flush=True);assert status['state']=='completed' and status['configRestored'];verify_file(out)
 for language,key,filename in [('export-lua','lua','lua-native-export.wav'),('export-javascript','javascript','js-native-export.wav')]:
  target=profile/'outputs'/filename;assert not target.exists()
  tool('svext_test_parity_run',{'language':language});end=time.monotonic()+20
  while True:
   report=tool('svext_test_parity_state')['reports'].get(key)
   if report:break
   assert time.monotonic()<end;time.sleep(.1)
  print(language,report,flush=True);assert all(report.get(x) for x in ['snapshot','started','completed','audio']),report;verify_file(target)

finally:
 try:
  cfg=render('configuration')
  if any(cfg.get(k) for k in ['busy','waiting','stopping']):
   print('Rendering still active; preserve settings and group',flush=True)
  else:
   if format_scope:render('restore_format');wait('format_status','controlsRestored');render('finish_format');print('Original render format restored',flush=True)
   if path_scope:render('restore_path');wait('path_status','restored');render('finish_path');print('Original render path restored',flush=True)
   if group:
    g=tool('sv_read_editor',{'groupUUID':group})
    if g['noteCount']==1:
     tool('sv_remove_note',{'groupUUID':group,'guard':g['guard'],'noteIndex':1})
    g=tool('sv_read_editor',{'groupUUID':group});assert g['noteCount']==0
    tool('sv_remove_group',{'groupUUID':group,'guard':g['guard']});print('Temporary group removed',flush=True)
 finally:p.stdin.close();p.wait(timeout=5)
