import http.server,json,os,pathlib,subprocess,threading,time
root=pathlib.Path(__file__).resolve().parent;hits=[]
class Handler(http.server.BaseHTTPRequestHandler):
 def do_GET(self):
  hits.append(self.path);self.send_response(200);self.send_header('Content-Type','text/html');self.end_headers();self.wfile.write(b'<!doctype html><title>local coverage audit</title>ok')
 def log_message(self,*args):pass
server=http.server.ThreadingHTTPServer(('127.0.0.1',0),Handler);threading.Thread(target=server.serve_forever,daemon=True).start()
exe=pathlib.Path(os.environ.get('LOADER_PROBE_DIRECT_HOST',str(root/'experimental/SV2 Loader Probe.app/Contents/MacOS/synthv-studio-original')))
env=os.environ.copy();env.update(DYLD_INSERT_LIBRARIES=str(root/'loader-probe.dylib'),LOADER_PROBE_PLUGIN=str(root/'coverage-plugin.dylib'),LOADER_PROBE_HTTP_BASE=f'http://127.0.0.1:{server.server_port}')
with (root/'coverage.stdout').open('wb') as out,(root/'coverage.stderr').open('wb') as err:
 p=subprocess.Popen([str(exe)],env=env,stdout=out,stderr=err)
 deadline=time.monotonic()+8
 while p.poll() is None and time.monotonic()<deadline:
  if '/socket-bypass' in hits and '/webkit-bypass' in hits:time.sleep(.3);break
  time.sleep(.1)
 alive=p.poll() is None
 log=(root/'coverage.stderr').read_text(errors='replace')
 if alive:
  p.terminate()
  try:p.wait(timeout=5)
  except subprocess.TimeoutExpired:p.kill();p.wait(timeout=5)
server.shutdown();server.server_close()
report={'case':'host-transports-under-URLSession-only-block','pid':p.pid,'host_alive':alive,'server_received':hits,'markers':[line for line in log.splitlines() if any(tag in line for tag in ['LOADER_PROBE','NETWORK_GATE','COVERAGE_PROBE','WEBKIT_GATE'])],'urlsession_control_blocked':'/urlsession-control' not in hits and 'urlsession-error=-999' in log,'raw_socket_bypassed_gate':'/socket-bypass' in hits,'webkit_bypassed_gate':'/webkit-bypass' in hits,'complete_host_block_verified':False,'account_switch_readiness_rejected':'quiescence-result=-2 stale-result=-3 verified=0' in log}
(root/'coverage-results.json').write_text(json.dumps(report,indent=2));print(json.dumps(report,indent=2))
