from pathlib import Path
import http.server,threading,subprocess,os,time,json
source=Path(__file__).resolve().parent;base=source.parents[3];root=Path(os.environ['LOADER_PROBE_RUNTIME_DIR']) if os.environ.get('LOADER_PROBE_RUNTIME_DIR') else base/'work/background-session/runtime';root.mkdir(exist_ok=True);hits=[];liveBytes=[];liveClosed=[]
class Handler(http.server.BaseHTTPRequestHandler):
 def do_GET(self):
  hits.append(self.path)
  if self.path=='/background-live':
   self.send_response(200);self.send_header('Content-Length',str(8*1024*1024));self.end_headers();sent=0
   try:
    for _ in range(256):self.wfile.write(b'x'*32768);self.wfile.flush();sent+=32768;time.sleep(.05)
   except (BrokenPipeError,ConnectionResetError):liveClosed.append(True)
   liveBytes.append(sent)
  else:
   data=b'background-test';self.send_response(200);self.send_header('Content-Length',str(len(data)));self.end_headers();self.wfile.write(data)
 def log_message(self,*args):pass
server=http.server.ThreadingHTTPServer(('127.0.0.1',0),Handler);threading.Thread(target=server.serve_forever,daemon=True).start();exe=Path(os.environ['LOADER_PROBE_DIRECT_HOST']);env=os.environ.copy();env.update(DYLD_INSERT_LIBRARIES=os.environ.get('LOADER_PROBE_LIBRARY',str((Path.home()/'Applications/Extend Loader.app/Contents/Resources/Native/loader-probe.dylib'))),LOADER_PROBE_PLUGIN=str(source/'urlsession-callback-plugin.dylib'),OPEN_SV2_LOADER_DATA_DIR=str(root),LOADER_BACKGROUND_BASE=f'http://127.0.0.1:{server.server_port}')
with (root/'stdout.log').open('wb') as out,(root/'stderr.log').open('wb') as err:
 p=subprocess.Popen([str(exe)],env=env,stdout=out,stderr=err,start_new_session=True);deadline=time.monotonic()+65
 while p.poll() is None and time.monotonic()<deadline:
  if 'BACKGROUND_PROBE_DONE' in (root/'stderr.log').read_text(errors='replace'):break
  time.sleep(.1)
 text=(root/'stdout.log').read_text(errors='replace');alive=p.poll() is None
 if alive and 'BACKGROUND_PROBE_DONE' not in (root/'stderr.log').read_text(errors='replace'):
  (root/'urlsession-live-host.json').write_text(json.dumps({'pid':p.pid,'executable':str(exe),'runtimeDirectory':str(root)},indent=2));raise RuntimeError('Observation timed out; preserve and inspect the same host PID '+str(p.pid))
 if alive:p.terminate();p.wait(timeout=6)
server.shutdown();values=[json.loads(line.split(' ',1)[1]) for line in text.splitlines() if line.startswith('BACKGROUND_PROBE ')];report={'pid':p.pid,'testedCore':env['DYLD_INSERT_LIBRARIES'],'hits':hits,'hostAlive':alive,'liveBytesWritten':liveBytes,'livePeerClosed':liveClosed,'result':values[-1] if values else None};from urlsession_callback_checks import validate;report['checks']=validate(report);report['passed']=all(report['checks'].values());(root/'results.json').write_text(json.dumps(report,indent=2));print(json.dumps(report,indent=2));assert report['passed']
