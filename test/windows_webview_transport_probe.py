import json, socket, threading, select, subprocess, os, tempfile, queue, time, pathlib, sys
listener=socket.socket();listener.bind(('127.0.0.1',0));listener.listen(); port=listener.getsockname()[1]
lock=threading.Lock(); clients=set(); blocked=False; accepted=0
def readn(s,n):
 b=b''
 while len(b)<n:
  x=s.recv(n-len(b))
  if not x: raise OSError('eof')
  b+=x
 return b
def serve(c):
 global accepted
 remote=None
 try:
  c.settimeout(8); h=readn(c,2); methods=readn(c,h[1]); assert h[0]==5 and 0 in methods; c.sendall(b'\x05\x00')
  h=readn(c,4); assert h[:3]==b'\x05\x01\x00'
  if h[3]==1: host=socket.inet_ntop(socket.AF_INET,readn(c,4))
  elif h[3]==4: host=socket.inet_ntop(socket.AF_INET6,readn(c,16))
  elif h[3]==3: host=readn(c,readn(c,1)[0]).decode('idna')
  else: raise OSError('bad address')
  port=int.from_bytes(readn(c,2),'big')
  with lock:
   if blocked: raise OSError('offline')
  remote=socket.create_connection((host,port),timeout=5)
  with lock:
   if blocked: raise OSError('offline')
   clients.add(remote); accepted+=1
  c.sendall(b'\x05\x00\x00\x01\x00\x00\x00\x00\x00\x00');c.settimeout(None);remote.settimeout(None)
  while True:
   r,_,_=select.select([c,remote],[],[],1)
   with lock:
    if blocked: break
   for s in r:
    b=s.recv(16384)
    if not b: return
    (remote if s is c else c).sendall(b)
 except (OSError,AssertionError,UnicodeError): pass
 finally:
  with lock:
   clients.discard(c)
   if remote: clients.discard(remote)
  c.close()
  if remote: remote.close()
def accept():
 while True:
  try: c,_=listener.accept()
  except OSError: return
  with lock: clients.add(c)
  threading.Thread(target=serve,args=(c,),daemon=True).start()
threading.Thread(target=accept,daemon=True).start()
env=os.environ.copy(); env['WEBVIEW2_ADDITIONAL_BROWSER_ARGUMENTS']=f'--proxy-server=socks5://127.0.0.1:{port} --proxy-bypass-list=<-loopback> --disable-quic --force-webrtc-ip-handling-policy=disable_non_proxied_udp'
if len(sys.argv) != 3: raise SystemExit('usage: python windows_webview_transport_probe.py FIXTURE_EXE SYSTEM_RUNTIME_DLL')
root=pathlib.Path(tempfile.mkdtemp(prefix='webview-proxy-fixture-')); binary=pathlib.Path(sys.argv[1]);dll=sys.argv[2]
results={}
profile=root/'profile'/'EBWebView'/'Default';profile.mkdir(parents=True)
(profile/'Preferences').write_text(json.dumps({'webrtc':{'ip_handling_policy':'disable_non_proxied_udp'}}),encoding='utf8')
p=subprocess.Popen([str(binary),dll,str(root/'profile'),str(root/'preview.png'),'interactive'],stdin=subprocess.PIPE,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,encoding='utf8',errors='replace',env=env,bufsize=1)
q=queue.Queue()
def reader():
 for line in p.stdout: q.put(line.rstrip())
 q.put(None)
threading.Thread(target=reader,daemon=True).start()
try:
 while True:
  line=q.get(timeout=90)
  if line is None: break
  if line.startswith(('network_phase=','dom=','ready=','job_samples=')): print(line,flush=True)
  if line.startswith('dom=1 '):
   cmd="Get-CimInstance Win32_Process -Filter \"Name = 'msedgewebview2.exe'\" | Where-Object { $_.CommandLine -like '*webview-proxy-fixture-*' } | ForEach-Object { 'role=' + $(if ($_.CommandLine -match '--type=([^ ]+)') { $Matches[1] } else { 'browser' }) + ' proxy=' + ($_.CommandLine -match '--proxy-server=') + ' webrtc=' + ($_.CommandLine -match '--force-webrtc-ip-handling-policy=disable_non_proxied_udp') }"
   print(subprocess.run(['powershell','-NoProfile','-Command',cmd],capture_output=True,text=True).stdout,flush=True)
  if line.startswith('network_phase='):
   phase=int(line.split(' ',1)[0].split('=')[1]);results[phase]=json.loads(json.loads(line.split('result=',1)[1]))
  if line=='ready=2':
   with lock:
    blocked=True
    for s in list(clients):
     try: s.shutdown(socket.SHUT_RDWR)
     except OSError: pass
   print('proxy_offline accepted=',accepted,flush=True);p.stdin.write('\n');p.stdin.flush()
  if line=='ready=3':
   with lock: blocked=False
   print('proxy_online accepted=',accepted,flush=True);p.stdin.write('\n');p.stdin.flush()
 p.wait(timeout=10);print('exit=',p.returncode,'total_tunnels=',accepted,flush=True)
 offline=results.get(2,{})
 if p.returncode or offline.get('srflx4') != 0 or offline.get('srflx6') != 0 or offline.get('http') != 'failed' or offline.get('workerNewWs') == 'echo':
  raise SystemExit('FAIL: proxy does not enforce complete offline network isolation')
finally:
 if p.poll() is None: p.kill()
 listener.close()
