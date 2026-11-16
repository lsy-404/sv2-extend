"""Full resident Core injected before an independent executable's constructors."""
from pathlib import Path
import json, os, socket, subprocess, tempfile
source=Path(__file__).resolve().parent
work=source.parents[3]/'work/startup-constructor';work.mkdir(parents=True,exist_ok=True)
binary=work/'fixture'
subprocess.run(['xcrun','clang','-Wall','-Wextra','-Werror',str(source/'startup_constructor_fixture.c'),'-o',str(binary)],check=True)
checks={};observations={}
with tempfile.TemporaryDirectory(prefix='extend-premain-') as temporary:
 for mode in ('allow','guard','malformed'):
  root=Path(temporary)/mode;root.mkdir(mode=0o700);preferences=root/'preferences';preferences.mkdir(mode=0o700)
  policy=preferences/'startup-network-policy.json'
  if mode!='allow':
   policy.write_text(json.dumps({'schema':1,'generation':1,'guards':{'test.owner:operation':True}}) if mode=='guard' else 'malformed')
   policy.chmod(0o600)
  with socket.socket() as server:
   server.bind(('127.0.0.1',0));server.listen();server.settimeout(.3)
   env=os.environ.copy()
   for key in list(env):
    if key.startswith(('OPEN_SV2_','LOADER_PROBE_','DYLD_')):env.pop(key)
   env.update(DYLD_INSERT_LIBRARIES=str(source/'loader-probe.dylib'),OPEN_SV2_LOADER_DATA_DIR=str(root),OPEN_SV2_ISOLATE_WEBSITE_DATA='1',EXTEND_TEST_PORT=str(server.getsockname()[1]))
   result=subprocess.run([str(binary)],env=env,capture_output=True,text=True,timeout=20)
   (work/f'{mode}.stderr').write_text(result.stderr)
   if mode=='malformed':
    checks['malformedRefusedBeforeHostInitialization']=result.returncode==78 and 'HOST_MAIN' not in result.stdout
    observations[mode]={'exitCode':result.returncode,'hostMainReached':'HOST_MAIN' in result.stdout}
    continue
   assert result.returncode==0,(mode,result.returncode,result.stderr)
   state=json.loads(result.stdout.splitlines()[0]);payload=b''
   try:
    connection,_=server.accept()
    with connection:payload=connection.recv(100)
   except socket.timeout:pass
   observations[mode]=state
   checks[mode+'HostMainReached']='HOST_MAIN' in result.stdout
   checks[mode+'SharedLeaseBeforeConstructor']=state['leaseMode']==1 and state['leaseCode']==0
   if mode=='allow':checks['defaultAllowSendsActualPayload']=state['connect']==0 and payload==b'constructor-payload'
   else:
    checks[mode+'PolicyAppliedBeforeHostConstructor']=state['blocked']==1 and state['applied']==1
    checks[mode+'ActualSocketDenied']=state['connect']==-1 and payload==b''
    checks[mode+'ValidityTruthful']=state['valid']==int(mode=='guard')
report={'passed':all(checks.values()),'checks':checks,'observations':observations,'scope':'Full production Core before independent host constructors and main, real TCP peer. Does not prove all SV2 transports or whole-host isolation.'}
(work/'results.json').write_text(json.dumps(report,indent=2));print(json.dumps(report,indent=2));assert report['passed']
