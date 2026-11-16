from pathlib import Path
import http.server,threading,subprocess,time,json
root=Path(__file__).resolve().parent;base=root.parents[3];scratch=base/'work/http-cancel';scratch.mkdir(parents=True,exist_ok=True);subprocess.run(['xcrun','clang','-fobjc-arc','-Wall','-Wextra','-Werror','-I',str(root),str(root/'resident_http_cancel_test.m'),str(root/'http_capability.m'),str(root/'resident_http.m'),'-framework','Foundation','-lcurl','-o',str(scratch/'driver')],check=True)
disconnected=threading.Event()
class H(http.server.BaseHTTPRequestHandler):
 def log_message(self,*args):pass
 def do_GET(self):
  self.send_response(200);self.send_header('Content-Length','100000' if self.path=='/slow' else '2');self.end_headers()
  try:
   if self.path=='/slow':
    for i in range(1000):self.wfile.write(b'x'*100);self.wfile.flush();time.sleep(.01)
   else:self.wfile.write(b'OK')
  except (BrokenPipeError,ConnectionResetError):disconnected.set()
server=http.server.ThreadingHTTPServer(('127.0.0.1',0),H);threading.Thread(target=server.serve_forever,daemon=True).start();p=subprocess.run([str(scratch/'driver'),f'http://127.0.0.1:{server.server_port}'],capture_output=True,text=True,timeout=12);print(p.stdout);assert p.returncode==0,p.stderr;v=json.loads(p.stdout);v['checks']['serverSawCancelledConnectionClose']=disconnected.wait(2);v['passed']=all(v['checks'].values());(scratch/'results.json').write_text(json.dumps(v,indent=2));server.shutdown();assert v['passed']
