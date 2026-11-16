"""Actual resident MCP remains reachable while host network is blocked."""
from pathlib import Path
import json,os,subprocess,shutil,time,urllib.request
from host_probe_support import prepare_host
source=Path(__file__).resolve().parent;work=source.parents[3]/'work/network-control-mcp';work.mkdir(parents=True,exist_ok=True)
subprocess.run(['xcrun','clang','-dynamiclib','-fobjc-arc','-Wall','-Wextra','-Werror',str(source/'network_control_mcp_fixture.m'),'-framework','Foundation','-o',str(work/'fixture.dylib')],check=True)
handle,_=prepare_host(source,work,'network-control-mcp');root=Path(handle['root']);run=Path(handle['runDirectory']);package=root/'plugins/network-control-test';package.mkdir(mode=0o700);shutil.copy2(work/'fixture.dylib',package/'fixture.dylib');(package/'plugin.json').write_text(json.dumps({'schema':1,'id':'test.network-control','name':'Network control diagnostic','version':'1','nativeLibrary':'fixture.dylib'}))
env={k:v for k,v in os.environ.items() if not k.startswith(('OPEN_SV2_','LOADER_PROBE_','DYLD_','EXTEND_'))};exe=Path(handle['probeApplication'])/'Contents/MacOS/ExtendLoader'
with (run/'stdout.log').open('wb') as out,(run/'stderr.log').open('wb') as err:p=subprocess.Popen([str(exe),'--data-dir',str(root),'--isolate-host-data'],env=env,stdout=out,stderr=err,start_new_session=True)
handle['pid']=p.pid;(work/'live-host.json').write_text(json.dumps(handle,indent=2));print(json.dumps(handle),flush=True)
responses=[];failures=0;deadline=time.monotonic()+45;last=None
while p.poll() is None and time.monotonic()<deadline:
 try:
  state=json.loads((root/'network-control-probe.json').read_text());endpoint=json.loads((root/f'endpoints/sv2-mcp-{p.pid}.json').read_text())
  request=urllib.request.Request(endpoint['url'],data=json.dumps({'jsonrpc':'2.0','id':1,'method':'tools/call','params':{'name':'sv_status','arguments':{}}}).encode(),headers={'Content-Type':'application/json','Authorization':'Bearer '+endpoint['token']})
  reply=json.load(urllib.request.urlopen(request,timeout=2))['result']['structuredContent'];responses.append({'phase':state['phase'],'hostPid':reply['hostPid'],'editorReady':reply['editorReady']});last=state
  if state['phase']=='restored':break
 except FileNotFoundError:pass
 except (OSError,KeyError,json.JSONDecodeError):failures+=1
 time.sleep(.1)
if not last or last['phase']!='restored':raise RuntimeError('Inspect same host PID '+str(p.pid)+'; probe incomplete')
checks={'sameHostResponds':all(r['hostPid']==p.pid for r in responses),'multipleBlockedMCPCalls':sum(r['phase']=='blocked' for r in responses)>=3,'restoredMCPWorks':responses[-1]['phase']=='restored','editorReady':responses[-1]['editorReady'],'policyRestoreSucceeded':last['code']==last['snapshot']==last['blocked']==0,'wholeHostStillUnverified':last['wholeHostVerified']==0,'noControlErrors':failures==0,'hostAlive':p.poll() is None}
report={'checks':checks,'passed':all(checks.values()),'responses':responses,'handle':handle,'scope':'Actual isolated normal Loader entry, resident MCP authenticated calls during host block and after restore; does not prove whole-host network isolation.'};(work/'results.json').write_text(json.dumps(report,indent=2));print(json.dumps({'checks':checks,'passed':report['passed']},indent=2))
if p.poll() is None:p.terminate();p.wait(timeout=8)
assert report['passed']
