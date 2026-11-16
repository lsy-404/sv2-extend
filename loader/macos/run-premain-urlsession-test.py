"""Full Core before executable constructors, independent loopback HTTP peer."""
from pathlib import Path
import http.server,json,os,subprocess,tempfile,threading,time
source=Path(__file__).resolve().parent;work=source.parents[3]/'work/premain-urlsession';work.mkdir(parents=True,exist_ok=True);binary=work/'fixture'
subprocess.run(['xcrun','clang','-fobjc-arc','-Wall','-Wextra','-Werror',str(source/'premain_urlsession_fixture.m'),'-framework','Foundation','-o',str(binary)],check=True)
hits=[]
class Handler(http.server.BaseHTTPRequestHandler):
 def do_GET(self):
  hits.append(self.path);time.sleep(.2);body=b'premain-response';self.send_response(200);self.send_header('Content-Length',str(len(body)));self.end_headers();self.wfile.write(body)
 def log_message(self,*args):pass
server=http.server.ThreadingHTTPServer(('127.0.0.1',0),Handler);thread=threading.Thread(target=server.serve_forever,daemon=True);thread.start()
try:
 with tempfile.TemporaryDirectory(prefix='extend-premain-urlsession-') as temporary:
  root=Path(temporary);root.chmod(0o700)
  env={k:v for k,v in os.environ.items() if not k.startswith(('OPEN_SV2_','LOADER_PROBE_','DYLD_','EXTEND_'))}
  env.update(DYLD_INSERT_LIBRARIES=str(source/'loader-probe.dylib'),OPEN_SV2_LOADER_DATA_DIR=str(root),EXTEND_PREMAIN_PORT=str(server.server_port))
  p=subprocess.run([str(binary)],env=env,capture_output=True,text=True,timeout=15)
  (work/'stderr.log').write_text(p.stderr);assert p.returncode==0,(p.returncode,p.stderr)
  value=json.loads(next(line[len('PREMAIN_URLSESSION '):] for line in p.stdout.splitlines() if line.startswith('PREMAIN_URLSESSION ')))
 checks={'gateInstalledBeforeExecutableConstructor':value['initialCode']==0,'premainTaskTracked':value['liveCode']==0 and value['liveTasks']>=1,'realHTTPRequestObserved':hits==['/premain'],'nativeResponsePreserved':value['gotCallback'] and value['returned'] and value['replies']==1 and value['status']==200 and value['correctBody'],'callbackCountedWhileExecuting':value['pendingDuring']>=1 and value['heldPending']>=1,'heldCallbackNotDrained':value['heldCode']==0 and value['heldDrained']==0,'callbackReturnDrains':value['pendingAfter']==0}
 report={'checks':checks,'fixture':value,'hits':hits,'passed':all(checks.values()),'scope':'Production Core and real URLSession HTTP task in an executable constructor, before host main; queued/executing callback count. Not whole-host isolation or SV2-specific lifecycle.'}
 suffix='audit' if os.environ.get('EXTEND_PREMAIN_AUDIT') else 'results';(work/(suffix+'.json')).write_text(json.dumps(report,indent=2));print(json.dumps(report,indent=2))
 if suffix!='audit':assert report['passed']
finally:server.shutdown();server.server_close();thread.join(timeout=2)
