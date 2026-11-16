from pathlib import Path
import json,os,selectors,socket,subprocess,uuid
source=Path(__file__).resolve().parent;work=source.parents[3]/'work/socket-accept';work.mkdir(parents=True,exist_ok=True);run=work/uuid.uuid4().hex;run.mkdir(mode=0o700);library=run/'gate.dylib';binary=run/'fixture'
subprocess.run(['xcrun','clang','-DSOCKET_GATE_DIAGNOSTICS','-dynamiclib','-Wall','-Wextra','-Werror',str(source/'socket_gate.c'),'-o',str(library)],check=True)
subprocess.run(['xcrun','clang','-Wall','-Wextra','-Werror',str(source/'socket_accept_fixture.c'),'-o',str(binary)],check=True)
env={k:v for k,v in os.environ.items() if not k.startswith(('DYLD_','OPEN_SV2_','LOADER_PROBE_'))};env['DYLD_INSERT_LIBRARIES']=str(library)
checks={};observations={}
for mode,bypass,control,ipv6 in [(name+('IPv6' if ipv6 else 'IPv4'),bypass,control,ipv6) for ipv6 in [0,1] for name,bypass,control in [('host',0,0),('privateHost',1,0),('control',0,1),('privateControl',1,1)]]:
 with (run/(mode+'.stderr.log')).open('wb') as err:p=subprocess.Popen([str(binary),str(bypass),str(control),str(ipv6)],stdin=subprocess.PIPE,stdout=subprocess.PIPE,stderr=err,text=True,env=env)
 (run/(mode+'.handle.json')).write_text(json.dumps({'pid':p.pid,'binary':str(binary)}));selector=selectors.DefaultSelector();selector.register(p.stdout,selectors.EVENT_READ)
 def line():
  if not selector.select(5):raise RuntimeError('Inspect same fixture PID '+str(p.pid))
  return p.stdout.readline().strip()
 first=line();assert first.startswith('PORT '),first
 with socket.create_connection(('::1' if ipv6 else '127.0.0.1',int(first.split()[1])),timeout=3) as peer:
  assert line()=='BLOCKED';peer.settimeout(.2)
  try:closed=peer.recv(100)==b''
  except socket.timeout:closed=False
  p.stdin.write('\n');p.stdin.flush();result=line();assert result.startswith('RESULT '),result;value=json.loads(result[len('RESULT '):]);p.wait(timeout=5);assert p.returncode==0
 checks[mode+'PeerPolicy']=closed==bool(not control)
 checks[mode+'OldAcceptedPolicy']=value['oldAcceptedSendAfterRestore']==(16 if control else -1)
 if not bypass:checks[mode+'AcceptedAccounting']=value['liveBeforeBlock']==(0 if control else 1)
 observations[mode]=dict(value,peerClosedAtBlock=closed)
 selector.close()
report={'checks':checks,'fixture':observations,'passed':all(checks.values()),'scope':'Real IPv4/IPv6 inbound TCP accept and accept$NOCANCEL, independent shutdown observer, permanent host revocation after restore, listener-inherited control exemption. Standalone raw gate, not all host networking.'};suffix='audit' if os.environ.get('EXTEND_SOCKET_ACCEPT_AUDIT') else 'results';(work/(suffix+'.json')).write_text(json.dumps(report,indent=2));print(json.dumps(report,indent=2))
if suffix!='audit':assert report['passed']
