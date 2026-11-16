import http.server,json,os,pathlib,subprocess,threading,time
root=pathlib.Path(__file__).resolve().parent
hits=[]
class Handler(http.server.BaseHTTPRequestHandler):
 def do_GET(self):
  hits.append(self.path)
  if self.path=='/slow':time.sleep(2)
  self.send_response(200);self.end_headers()
  try:self.wfile.write(b'ok')
  except (BrokenPipeError,ConnectionResetError):pass
 def log_message(self,*args):pass
server=http.server.ThreadingHTTPServer(('127.0.0.1',0),Handler)
threading.Thread(target=server.serve_forever,daemon=True).start()
exe=pathlib.Path(os.environ.get("LOADER_PROBE_APP",str(root/"experimental/SV2 Loader Probe.app")))/"Contents/MacOS/loader-launcher"
env=os.environ.copy();env.pop('DYLD_INSERT_LIBRARIES',None);env.pop('LOADER_PROBE_BLOCK_HOST_AT_START',None);env['LOADER_PROBE_HTTP_BASE']=f'http://127.0.0.1:{server.server_port}'
direct=os.environ.get('LOADER_PROBE_DIRECT_HOST')
if direct:
 exe=pathlib.Path(direct);env['DYLD_INSERT_LIBRARIES']=str(root/'loader-probe.dylib');env['LOADER_PROBE_PLUGIN']=str(root/'network-plugin.dylib')
with (root/'network.stdout').open('wb') as out,(root/'network.stderr').open('wb') as err:
 p=subprocess.Popen([str(exe)],cwd=exe.parent,env=env,stdout=out,stderr=err)
 deadline=time.monotonic()+12
 while p.poll() is None and time.monotonic()<deadline:
  log=(root/'network.stderr').read_text(errors='replace')
  if 'network-toggle-selftest-' in log:break
  time.sleep(.1)
 alive=p.poll() is None
 log=(root/'network.stderr').read_text(errors='replace')
 children=subprocess.run(['pgrep','-P',str(p.pid)],capture_output=True,text=True)
 vm=subprocess.run(['vmmap',str(p.pid)],capture_output=True,text=True) if alive else None
 if vm:(root/'network.vmmap').write_text(vm.stdout+vm.stderr)
 if alive:
  p.terminate()
  try:p.wait(timeout=5)
  except subprocess.TimeoutExpired:p.kill();p.wait(timeout=5)
server.shutdown();server.server_close()
markers=[line for line in log.splitlines() if any(tag in line for tag in ['LOADER_LAUNCHER','LOADER_PROBE','NETWORK_GATE','NETWORK_PROBE'])]
expected=['/allowed','/slow','/plugin-while-blocked','/restored']
report={'case':'locally-resigned-SV2-NSURLSession-gate','pid':p.pid,'host_alive_after_selftest':alive,'host_children':children.stdout.split(),'markers':markers,'server_hits':hits,'passed':'network-toggle-selftest-ok' in log and hits==expected,'final_exit':p.returncode,'scope':'NSURLSessionTask resume interception; not a system firewall; original SV2 request coverage not verified'}
(root/'network-runtime-results.json').write_text(json.dumps(report,indent=2))
print(json.dumps(report,indent=2))
