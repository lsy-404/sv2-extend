import socket,threading,subprocess,os,json,time,pathlib,http.server
source=pathlib.Path(__file__).resolve().parent;base=source.parents[3];root=pathlib.Path(os.environ['LOADER_PROBE_RUNTIME_DIR']) if os.environ.get('LOADER_PROBE_RUNTIME_DIR') else base/'work/network-framework/host-runtime';root.mkdir(exist_ok=True);tcp=socket.socket();tcp.bind(('127.0.0.1',0));tcp.listen();udp=socket.socket(socket.AF_INET,socket.SOCK_DGRAM);udp.bind(('127.0.0.1',0));received=[];packets=[];peers=[];httpHits=[]
class Handler(http.server.BaseHTTPRequestHandler):
 def do_GET(self):
  httpHits.append(self.path);self.send_response(200);self.send_header('Content-Length','9');self.end_headers();self.wfile.write(b'plugin-ok')
 def log_message(self,*args):pass
http=http.server.ThreadingHTTPServer(('127.0.0.1',0),Handler)
def consume(peer):
 peer.settimeout(15);buffer=b'';echoed=False
 try:
  while True:
   data=peer.recv(4096)
   if not data:break
   received.append(data.decode());buffer+=data
   if not echoed and buffer==b'restored':peer.sendall(b'ack-restored');echoed=True
 except (TimeoutError,ConnectionResetError):pass
 finally:peer.close()
def listen():
 while True:
  peer,_=tcp.accept();peers.append(1);threading.Thread(target=consume,args=(peer,),daemon=True).start()
def datagrams():
 while True:
  data,address=udp.recvfrom(4096);packets.append(data.decode())
  if data==b'udp-echo':udp.sendto(b'ack-udp',address)
for fn in [listen,datagrams,http.serve_forever]:threading.Thread(target=fn,daemon=True).start()
exe=pathlib.Path(os.environ['LOADER_PROBE_DIRECT_HOST']);env=os.environ.copy();env.update(DYLD_INSERT_LIBRARIES=os.environ.get('LOADER_PROBE_LIBRARY',str(source/'loader-probe.dylib')),LOADER_PROBE_PLUGIN=str(source/'network-framework-plugin.dylib'),LOADER_NW_TCP_PORT=str(tcp.getsockname()[1]),LOADER_NW_UDP_PORT=str(udp.getsockname()[1]),LOADER_NW_HTTP_URL=f'http://127.0.0.1:{http.server_port}/plugin-while-blocked',OPEN_SV2_LOADER_DATA_DIR=str(root));env.pop('OPEN_SV2_AUTO_EDITOR_PROVIDER',None)
with (root/'stdout.log').open('wb') as out,(root/'stderr.log').open('wb') as err:
 process=subprocess.Popen([os.environ.get('LOADER_PROBE_DIRECT_HOST',str(exe))],env=env,stdout=out,stderr=err,start_new_session=True)
 (root/'network-framework-live-host.json').write_text(json.dumps({'pid':process.pid,'executable':str(exe),'runtimeDirectory':str(root)},indent=2))
 deadline=time.monotonic()+40
 while process.poll() is None and time.monotonic()<deadline:
  if 'NW_HOST_FIXTURE_DONE' in (root/'stderr.log').read_text(errors='replace'):break
  time.sleep(.1)
 text=(root/'stdout.log').read_text(errors='replace');log=(root/'stderr.log').read_text(errors='replace');children=subprocess.run(['pgrep','-P',str(process.pid)],capture_output=True,text=True).stdout.split();alive=process.poll() is None
 if alive and 'NW_HOST_FIXTURE_DONE' not in log:raise RuntimeError('Observation timed out; preserve and inspect the same host PID '+str(process.pid))
 if alive:process.terminate();process.wait(timeout=8)
http.shutdown();time.sleep(.2)
rows=[json.loads(line) for line in text.splitlines() if line.startswith('{')];report={'pid':process.pid,'actualExecutable':str(exe),'testedCore':env['DYLD_INSERT_LIBRARIES'],'fixture':rows[-1] if rows else None,'tcpReceived':received,'udpReceived':packets,'acceptedTCPConnections':len(peers),'pluginHTTPHits':httpHits,'hostAliveAfterTests':alive,'childPids':children};print(json.dumps(report,indent=2));(root/'results.json').write_text(json.dumps(report,indent=2));assert rows and all(rows[-1]['checks'].values());assert alive and not children;assert ''.join(received)=='before-blockrestored' and packets==['udp-allowed','udp-echo'] and len(peers)==2 and httpHits==['/plugin-while-blocked']
