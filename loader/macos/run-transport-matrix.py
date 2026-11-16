import http.server,json,os,pathlib,re,socket,subprocess,threading,time,ssl,tempfile
public_https=os.environ.get('LOADER_MATRIX_PUBLIC_HTTPS')=='1'
root=pathlib.Path(__file__).resolve().parent;output=pathlib.Path(os.environ.get('LOADER_MATRIX_OUTPUT_DIR',str(root)));output.mkdir(parents=True,exist_ok=True);hits=[];packets=[];hold=[];stop=threading.Event()
class Handler(http.server.BaseHTTPRequestHandler):
 def do_GET(self):
  hits.append(self.path);self.send_response(200);self.send_header('Content-Type','text/html');self.send_header('Cache-Control','no-store');self.end_headers()
  try:self.wfile.write(b'<!doctype html><title>gate matrix</title>ok')
  except (BrokenPipeError,ConnectionResetError):pass
 def log_message(self,*args):pass
class Server6(http.server.ThreadingHTTPServer):address_family=socket.AF_INET6
servers=[http.server.ThreadingHTTPServer(('127.0.0.1',0),Handler),Server6(('::1',0),Handler)]
tls_directory=None
if os.environ.get('LOADER_MATRIX_PROXY_TEST')=='1':
 if public_https:raise SystemExit('Choose one TLS fixture mode')
 library=pathlib.Path(os.environ.get('LOADER_PROBE_LIBRARY',str(root/'loader-probe.dylib')))
 if b'loader-audit.test' not in library.read_bytes():raise SystemExit('Reserved-name TLS fixture requires a LOADER_GATE_TESTING core with test-only DNS mapping')
 scratch=pathlib.Path.cwd()/'work/macos-loader-probe';scratch.mkdir(parents=True,exist_ok=True)
 tls_directory=tempfile.TemporaryDirectory(prefix='tls-fixture-',dir=scratch);tls=pathlib.Path(tls_directory.name)
 (tls/'request.cnf').write_text('[req]\ndistinguished_name=dn\nx509_extensions=ext\nprompt=no\n[dn]\nCN=loader-audit.test\n[ext]\nsubjectAltName=DNS:loader-audit.test\nbasicConstraints=critical,CA:FALSE\nkeyUsage=digitalSignature,keyEncipherment\nextendedKeyUsage=serverAuth\n')
 subprocess.run(['openssl','req','-x509','-newkey','rsa:2048','-nodes','-sha256','-days','2','-config',str(tls/'request.cnf'),'-keyout',str(tls/'key.pem'),'-out',str(tls/'certificate.pem')],check=True,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
 subprocess.run(['openssl','x509','-in',str(tls/'certificate.pem'),'-outform','DER','-out',str(tls/'certificate.der')],check=True)
 https=http.server.ThreadingHTTPServer(('127.0.0.1',0),Handler);context=ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER);context.load_cert_chain(tls/'certificate.pem',tls/'key.pem');https.socket=context.wrap_socket(https.socket,server_side=True);servers.append(https)
for server in servers:threading.Thread(target=server.serve_forever,daemon=True).start()
udp=socket.socket(socket.AF_INET,socket.SOCK_DGRAM);udp.bind(('127.0.0.1',0));udp.settimeout(.2)
def udp_worker():
 while not stop.is_set():
  try:packets.append(udp.recv(1024).decode())
  except socket.timeout:pass
threading.Thread(target=udp_worker,daemon=True).start()
listener=socket.socket();listener.bind(('127.0.0.1',0));listener.listen();listener.settimeout(.2)
def hold_worker():
 while not stop.is_set():
  try:
   client,_=listener.accept();client.settimeout(12)
   with client:
    hold.append(client.recv(1024).decode());client.sendall(b'ack')
    try:hold.append(client.recv(1024).decode())
    except (socket.timeout,ConnectionResetError):hold.append('closed')
   return
  except socket.timeout:pass
threading.Thread(target=hold_worker,daemon=True).start()
assert (root/'transport-matrix-plugin.dylib').is_file(), 'Build transport-matrix fixture before starting a host'
exe=os.environ['LOADER_PROBE_DIRECT_HOST'];env=os.environ.copy();env.update(DYLD_INSERT_LIBRARIES=os.environ.get('LOADER_PROBE_LIBRARY',str(root/'loader-probe.dylib')),LOADER_PROBE_PLUGIN=str(root/'transport-matrix-plugin.dylib'),LOADER_PROBE_HTTP_BASE=f'http://127.0.0.1:{servers[0].server_port}',LOADER_MATRIX_HTTP6_PORT=str(servers[1].server_port),LOADER_MATRIX_UDP_PORT=str(udp.getsockname()[1]),LOADER_MATRIX_HOLD_PORT=str(listener.getsockname()[1]))
if public_https:env['LOADER_MATRIX_WEBKIT_BASE']='https://example.com'
if tls_directory:
 env['LOADER_MATRIX_WEBKIT_BASE']=f'https://loader-audit.test:{servers[2].server_port}';env['LOADER_MATRIX_TLS_DER']=str(pathlib.Path(tls_directory.name)/'certificate.der')
with (output/'matrix.stdout').open('wb') as out,(output/'matrix.stderr').open('wb') as err:
 p=subprocess.Popen([exe],env=env,stdout=out,stderr=err)
 (output/'live-host.json').write_text(json.dumps({'pid':p.pid,'executable':exe},indent=2))
 deadline=time.monotonic()+35
 while p.poll() is None and time.monotonic()<deadline:
  log=(output/'matrix.stderr').read_text(errors='replace')
  if 'transport-matrix-complete' in log:break
  time.sleep(.1)
 log=(output/'matrix.stderr').read_text(errors='replace')
 children=subprocess.run(['pgrep','-P',str(p.pid)],capture_output=True,text=True).stdout.split()
 if p.poll() is None and 'transport-matrix-complete' not in log:raise RuntimeError('Inspect same live matrix host PID '+str(p.pid)+'; observation incomplete')
 if p.poll() is None:
  p.terminate()
  try:p.wait(timeout=5)
  except subprocess.TimeoutExpired:raise RuntimeError('Inspect same matrix host PID '+str(p.pid)+'; termination pending')
for server in servers:server.shutdown();server.server_close()
stop.set();time.sleep(.3);udp.close();listener.close()
if tls_directory:tls_directory.cleanup()
rows=[{'phase':int(a),'case':b,'result':int(c)} for a,b,c in re.findall(r'MATRIX phase=(\d+) case=([\w-]+) result=(-?\d+)',log)]
checks={}
for row in rows:
 expected=(1 if row['case'].startswith('udp-') else 200) if row['phase']!=1 else None
 checks[f"phase{row['phase']}/{row['case']}"]=(100<=row['result']<=599) if public_https and row['phase']==2 and row['case']=='existing-page-fetch' else (row['result']==expected if expected is not None else row['result']<0)
checks['no-blocked-http-reached-server']=not any('/phase1-' in x for x in hits)
checks['no-blocked-udp-reached-server']=not any('phase1-' in x for x in packets)
checks['existing-connection-no-post-block-bytes']=hold and hold[0]=='before' and not any('after' in x for x in hold)
checks['complete']='transport-matrix-complete' in log
report={'pid':p.pid,'rows':rows,'server_http_hits':hits,'server_udp_packets':packets,'existing_connection_received':hold,'host_child_pids':children,'checks':checks,'passed':bool(checks) and all(checks.values())}
if public_https:report['scope']='Trusted public HTTPS navigation and HTTP-response reachability; zero-hit checks observe local servers only.'
(output/'transport-matrix-results.json').write_text(json.dumps(report,indent=2));print(json.dumps(report,indent=2))

assert report['passed'], "Transport regression did not complete successfully"
