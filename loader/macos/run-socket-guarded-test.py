from pathlib import Path
import json,os,selectors,socket,subprocess,uuid
source=Path(__file__).resolve().parent;work=source.parents[3]/'work/socket-guarded';work.mkdir(parents=True,exist_ok=True);run=work/uuid.uuid4().hex;run.mkdir(mode=0o700);library=run/'gate.dylib';binary=run/'fixture'
subprocess.run(['xcrun','clang','-dynamiclib','-Wall','-Wextra','-Werror',str(source/'socket_gate.c'),'-o',str(library)],check=True)
subprocess.run(['xcrun','clang','-Wall','-Wextra','-Werror',str(source/'socket_guarded_fixture.c'),'-o',str(binary)],check=True)
env={k:v for k,v in os.environ.items() if not k.startswith(('DYLD_','OPEN_SV2_','LOADER_PROBE_'))};env['DYLD_INSERT_LIBRARIES']=str(library)
with (run/'stderr.log').open('wb') as err:p=subprocess.Popen([str(binary)],stdout=subprocess.PIPE,stderr=err,text=True,env=env)
(run/'handle.json').write_text(json.dumps({'pid':p.pid,'binary':str(binary)}));selector=selectors.DefaultSelector();selector.register(p.stdout,selectors.EVENT_READ)
if not selector.select(5):raise RuntimeError('Inspect same guarded fixture PID '+str(p.pid))
line=p.stdout.readline();assert line.startswith('PORT '),line
with socket.create_connection(('127.0.0.1',int(line.split()[1])),timeout=3):
 if not selector.select(5):raise RuntimeError('Inspect same guarded fixture PID '+str(p.pid))
 line=p.stdout.readline();assert line.startswith('RESULT '),line;value=json.loads(line[len('RESULT '):]);p.wait(timeout=5);assert p.returncode==0
checks={'guardPreserved':bool(value['guardPreserved']),'gapExplicit':bool(value['rawCoverageUnverified']),'guardedSocketRemainsActive':value['activeWhileGuarded']>=1,'ownerCloseDrains':value['activeAfterOwnerClose']==0};report={'checks':checks,'fixture':value,'passed':all(checks.values()),'scope':'Owned loopback TCP socket with real kernel DUP guard; skipped revocation remains counted until actual owner close. No system-owned guard is changed.'};suffix='audit' if os.environ.get('EXTEND_GUARDED_AUDIT') else 'results';(work/(suffix+'.json')).write_text(json.dumps(report,indent=2));print(json.dumps(report,indent=2))
if suffix!='audit':assert report['passed']
