"""Production Core + opt-in plugin before independent executable initializers."""
from pathlib import Path
import json,os,subprocess,tempfile
source=Path(__file__).resolve().parent;work=source.parents[3]/'work/bootstrap-order';work.mkdir(parents=True,exist_ok=True)
for name,args in [('plugin.dylib',['-dynamiclib',str(source/'bootstrap_order_plugin.m')]),('host',[str(source/'bootstrap_order_host.m')])]:
 subprocess.run(['xcrun','clang','-fobjc-arc','-Wall','-Wextra','-Werror',*args,'-framework','Foundation','-o',str(work/name)],check=True)
checks={};observations={}
with tempfile.TemporaryDirectory(prefix='extend-bootstrap-order-') as temporary:
 for mode in ['plain','required','missing','disabled','failed','duplicate-owner','missing-entry','malformed-policy']:
  root=Path(temporary)/mode;root.mkdir(mode=0o700);preferences=root/'preferences';preferences.mkdir(mode=0o700)
  package=root/'plugins/test';package.mkdir(parents=True,mode=0o700);library=package/'plugin.dylib';library.write_bytes((work/'plugin.dylib').read_bytes());library.chmod(0o700)
  manifest={'schema':1,'id':'test.early','name':'Bootstrap test','version':'1.0.0','nativeLibrary':'plugin.dylib','bootstrap':{'abi':1,'guardOwners':['test.early']}}
  if mode=='disabled':manifest['enabled']=False
  if mode=='missing':manifest.pop('bootstrap')
  if mode=='duplicate-owner':manifest['bootstrap']['guardOwners']=['test.early','test.early']
  if mode=='missing-entry':library.write_bytes((work/'host').read_bytes())
  (package/'plugin.json').write_text(json.dumps(manifest))
  guarded=mode not in ['plain','failed','duplicate-owner','missing-entry']
  policy={'schema':2,'generation':1,'guards':{'test.early:recover':True} if guarded else {},'bootstrapGuards':{'test.early:recover':True} if guarded else {}}
  policy_path=preferences/'startup-network-policy.json';policy_path.write_text('malformed' if mode=='malformed-policy' else json.dumps(policy));policy_path.chmod(0o600)
  env={k:v for k,v in os.environ.items() if not k.startswith(('OPEN_SV2_','LOADER_PROBE_','DYLD_','EXTEND_TEST_'))}
  env.update(DYLD_INSERT_LIBRARIES=str(source/'loader-probe.dylib'),OPEN_SV2_LOADER_DATA_DIR=str(root),EXTEND_TEST_BOOTSTRAP_LIBRARY=str(library))
  if mode=='failed':env['EXTEND_TEST_BOOTSTRAP_FAIL']='1'
  result=subprocess.run([str(work/'host')],env=env,capture_output=True,text=True,timeout=20)
  (work/(mode+'.stderr')).write_text(result.stderr)
  events=[json.loads(line[len('BOOTSTRAP_TEST '):]) for line in result.stdout.splitlines() if line.startswith('BOOTSTRAP_TEST ')]
  observations[mode]={'exitCode':result.returncode,'events':events,'hostMain':'HOST_MAIN' in result.stdout}
  if mode in ['plain','required']:
   checks[mode+'HostAdmitted']=result.returncode==0 and 'HOST_MAIN' in result.stdout
   checks[mode+'EarlyBeforeExecutableConstructor']=[e['phase'] for e in events]==['early','executable-constructor','normal']
   states={e['phase']:e for e in events}
   checks[mode+'SameResidentModule']=all(states.get(p,{}).get('constructions')==1 for p in ['executable-constructor','normal']) and states.get('normal',{}).get('bootCalls')==1 and states.get('normal',{}).get('normalCalls')==1
   checks[mode+'BorrowedMetadataSurvives']=states.get('normal',{}).get('metadataRetained') is True
   early=states.get('early',{});checks[mode+'PolicyBeforeEntry']=early.get('ownerCount')==int(guarded) and early.get('blocked')==int(guarded) and early.get('policyValid')==1 and early.get('applied')==int(guarded)
  else:
   checks[mode+'RefusesBeforeHostInitialization']=result.returncode==78 and 'HOST_MAIN' not in result.stdout and not any(e['phase']=='executable-constructor' for e in events)
report={'checks':checks,'observations':observations,'passed':all(checks.values()),'scope':'Production Core, real dlopen and independent executable initializer order, required-owner failure closure, same-module normal init. Does not prove SV2 account lifecycle or full network isolation.'}
(work/'results.json').write_text(json.dumps(report,indent=2));print(json.dumps(report,indent=2));assert report['passed']
