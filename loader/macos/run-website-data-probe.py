"""Three disposable hosts; synthetic cookies/localStorage, no real account copy."""
from pathlib import Path
import http.server,threading,os,json,uuid,time,subprocess
source=Path(__file__).resolve().parent;base=source.parents[3]
run=base/'work/host-data/runs'/uuid.uuid4().hex;run.mkdir(mode=0o700)
roots=[run/'profileA',run/'profileB']
for root in roots:
 (root/'home/Library/Application Support/Dreamtonics/Synthesizer V Studio 2/license').mkdir(parents=True,mode=0o700)
alias=run/'profileA-alias';alias.symlink_to(roots[0],target_is_directory=True)
class Handler(http.server.BaseHTTPRequestHandler):
 def log_message(self,*args):pass
 def do_GET(self):
  body=b'<!doctype html><meta charset=utf-8><title>Extend storage fixture</title>'
  self.send_response(200);self.send_header('Content-Type','text/html');self.send_header('Content-Length',str(len(body)));self.end_headers();self.wfile.write(body)
server=http.server.ThreadingHTTPServer(('127.0.0.1',0),Handler);threading.Thread(target=server.serve_forever,daemon=True).start()
exe=base/'work/host-data/host/SV2 Loader Probe.app/Contents/MacOS/synthv-studio-original';core=source/'loader-probe.dylib';native=base/'work/host-data/host-data-probe.dylib';fixture=source/'website-data-plugin.dylib'
results=[]
for phase,root,marker,reopen in [('seedA',roots[0],'A',False),('seedB',roots[1],'B',False),('reopenA',alias,'A',True)]:
 env=os.environ.copy()
 for key in ['LOADER_PROBE_BLOCK_HOST_AT_START','OPEN_SV2_AUTO_EDITOR_PROVIDER','OPEN_SV2_MCP_PLUGIN','OPEN_SV2_TEST_OWNED_PRODUCTS','OPEN_SV2_TEST_DATA_CONTROL','OPEN_SV2_TEST_RESTORE_NETWORK_AFTER_8S']:env.pop(key,None)
 env.update(DYLD_INSERT_LIBRARIES=str(core)+':'+str(native),LOADER_PROBE_PLUGIN=str(fixture),OPEN_SV2_LOADER_DATA_DIR=str(root),OPEN_SV2_TEST_HOST_HOME=str(root/'home'),OPEN_SV2_ISOLATE_WEBSITE_DATA='1',LOADER_WEBSITE_MARKER=marker,LOADER_WEBSITE_BASE=f'http://127.0.0.1:{server.server_port}')
 if reopen:env['LOADER_WEBSITE_REOPEN']='1'
 outPath=run/(phase+'.stdout.log');errPath=run/(phase+'.stderr.log')
 with outPath.open('wb') as out,errPath.open('wb') as err:
  p=subprocess.Popen([str(exe)],env=env,stdout=out,stderr=err,start_new_session=True)
  handle={'pid':p.pid,'executable':str(exe),'phase':phase,'runDirectory':str(run)};(run/'live-host.json').write_text(json.dumps(handle,indent=2));(base/'work/website-data/live-host.json').write_text(json.dumps(handle,indent=2));print(json.dumps(handle),flush=True)
  deadline=time.monotonic()+40;rows=[]
  while p.poll() is None and time.monotonic()<deadline:
   rows=[json.loads(line.split(' ',1)[1]) for line in outPath.read_text(errors='replace').splitlines() if line.startswith('WEBSITE_DATA_FIXTURE ')]
   if rows:break
   time.sleep(.1)
  if not rows:raise RuntimeError('No fixture result; inspect the same live handle before continuing. '+str(handle))
  result=rows[-1];result.update(phase=phase,hostAlive=p.poll() is None,pidMatches=result['pid']==p.pid)
  # Cookie store completion and readback have completed. Allow asynchronous disk IO,
  # then verify actual persistence by relaunching a new process in the final phase.
  time.sleep(.8)
  if p.poll() is None:p.terminate();p.wait(timeout=8)
  results.append(result)
  (run/(phase+'.json')).write_text(json.dumps(result,indent=2))
  if not result['passed']:raise AssertionError('Fixture failed: '+str(run/(phase+'.json')))
server.shutdown()
checks={'threeHostsPassed':len(results)==3 and all(r['passed'] and r['hostAlive'] and r['pidMatches'] for r in results),'differentRootsDefaultDistinct':results[0]['defaultIdentifier']!=results[1]['defaultIdentifier'],'differentRootsNamedDistinct':results[0]['namedIdentifier']!=results[1]['namedIdentifier'],'canonicalAliasDefaultStable':results[0]['defaultIdentifier']==results[2]['defaultIdentifier'],'canonicalAliasNamedStable':results[0]['namedIdentifier']==results[2]['namedIdentifier']}
report={'checks':checks,'passed':all(checks.values()),'phases':results,'runDirectory':str(run),'testedCore':str(core),'scope':'Opt-in macOS14+ public WebKit stores. Synthetic cookie/localStorage isolation and persistence only; not all website data types, native HTTP cookies, Keychain, private WK APIs, account switching or whole-host network quiescence.'}
(run/'results.json').write_text(json.dumps(report,indent=2));(base/'work/website-data/latest.json').write_text(json.dumps(report,indent=2));print(json.dumps(report,indent=2));assert report['passed']
