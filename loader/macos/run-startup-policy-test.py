from pathlib import Path
import subprocess,tempfile,json,os,concurrent.futures
source=Path(__file__).resolve().parent;base=source.parents[3];work=base/'work/startup-policy';work.mkdir(exist_ok=True);binary=work/'driver'
subprocess.run(['xcrun','clang','-fobjc-arc','-Wall','-Wextra','-Werror',str(source/'startup_policy_test_driver.m'),str(source/'startup_policy.m'),'-framework','Foundation','-o',str(binary)],check=True)
checks={}
class Host:
 def __init__(self,root):
  self.p=subprocess.Popen([str(binary),str(root)],stdin=subprocess.PIPE,stdout=subprocess.PIPE,stderr=subprocess.PIPE,text=True);self.boot=json.loads(self.p.stdout.readline())
 def call(self,op,**args):
  self.p.stdin.write(json.dumps(dict(op=op,**args))+'\n');self.p.stdin.flush();return json.loads(self.p.stdout.readline())
 def close(self):
  if self.p.poll() is None:self.p.stdin.write('{"op":"quit"}\n');self.p.stdin.flush()
  self.p.wait(timeout=5)
with tempfile.TemporaryDirectory(prefix='extend-startup-policy-') as directory:
 root=Path(directory);a=Host(root);b=Host(root);assert a.boot['bootstrap']==b.boot['bootstrap']==0
 checks['twoSharedHostsAdmitted']=a.call('state')['leaseMode']==b.call('state')['leaseMode']==1
 checks['upgradeRefusesOtherHost']=a.call('acquire')['code']==-5
 checks['failedUpgradeKeepsSharedLease']=b.call('acquire')['code']==-5
 a.call('close-other-fd');checks['closingOtherFDDoesNotDropLease']=b.call('acquire')['code']==-5
 b.close();r=a.call('acquire');checks['soleHostCanUpgrade']=r['code']==0 and r['leaseMode']==2
 checks['sameOperationAcquireIdempotent']=a.call('acquire')['leaseGeneration']==r['leaseGeneration']
 rejected=Host(root);rejected.p.wait(timeout=5);checks['exclusiveRejectsNewHost']=rejected.boot['bootstrap']==-5 and rejected.p.returncode==73
 checks['wrongOwnerCannotRelease']=a.call('release',owner='other.owner')['code']==-1 and a.call('state')['leaseMode']==2
 checks['differentOperationCannotAcquire']=a.call('acquire',id='other')['code']==-5
 checks['samePIDReceipt']=a.call('state')['leasePid']==a.boot['pid']
 a.call('release');b=Host(root);checks['downgradeReadmitsHost']=b.boot['bootstrap']==0
 with concurrent.futures.ThreadPoolExecutor() as pool:
  results=list(pool.map(lambda item:item[0].call('guard',owner=item[1],id='transaction',enabled=True),[(a,'owner.a'),(b,'owner.b')]))
 checks['concurrentGuardsPersisted']=all(r['code']==0 for r in results) and a.call('state')['guards']==2
 checks['bootstrapCountIsOwnerScoped']=a.call('owner-count',owner='owner.a')['code']==1 and a.call('owner-count',owner='owner')['code']==0 and a.call('owner-count',owner='absent.owner')['code']==0
 checks['bootstrapCountRejectsInvalidOwner']=a.call('owner-count',owner='owner.a:transaction')['code']==-1
 r=a.call('guard',owner='owner.a',id='transaction',enabled=True);checks['guardAcquireIdempotent']=a.call('guard',owner='owner.a',id='transaction',enabled=True)['policyGeneration']==r['policyGeneration']
 checks['guardListOwnerScoped']=a.call('list',owner='owner.a')['listed']['result']['guards']==['transaction'] and a.call('list',owner='absent.owner')['listed']['result']['guards']==[]
 a.call('guard',owner='owner.a',id='transaction',enabled=False);r=b.call('state');checks['oneReleasePreservesOtherGuard']=r['guards']==1 and r['blocked']==1
 b.call('guard',owner='owner.b',id='transaction',enabled=False);checks['lastGuardReleaseAllowsNextStartup']=a.call('state')['blocked']==0
 a.call('guard',owner='owner.a',id='crash-intent',enabled=True);a.close();b.close();a=Host(root);r=a.call('state');checks['guardSurvivesProcessExit']=r['blocked']==1 and r['guards']==1
 checks['orphanGuardDiscoverable']=a.call('list',owner='owner.a')['listed']['result']['guards']==['crash-intent'];a.call('guard',owner='owner.a',id='crash-intent',enabled=False)
 file=root/'preferences/startup-network-policy.json';saved=file.read_bytes();file.write_bytes(b'not json');r=a.call('state');checks['malformedPolicyFailsClosed']=r['blocked']==1 and r['policyValid']==0
 checks['bootstrapCountDoesNotTreatInvalidPolicyAsEmpty']=a.call('owner-count',owner='owner.a')['code']==-6
 checks['malformedPolicyNotOverwritten']=a.call('guard',enabled=False)['code']==-6 and file.read_bytes()==b'not json'
 file.write_bytes(saved);os.chmod(file,0o644);checks['publicModeFailsClosed']=a.call('state')['policyValid']==0;os.chmod(file,0o600)
 linked=root/'other-policy';os.link(file,linked);checks['hardlinkFailsClosed']=a.call('state')['blocked']==1 and a.call('guard',enabled=True)['code']==-6;linked.unlink()
 target=root/'target';target.write_bytes(saved);file.unlink();file.symlink_to(target);checks['symlinkFailsClosed']=a.call('state')['blocked']==1 and a.call('guard',enabled=True)['code']==-6 and target.read_bytes()==saved;file.unlink();file.write_bytes(saved);os.chmod(file,0o600)
 checks['validPolicyCanResume']=a.call('state')['policyValid']==1 and a.call('state')['blocked']==0
 a.call('guard',owner='early.owner',id='recovery',enabled=True)
 checks['plainGuardsDoNotRequireBootstrap']=a.call('required-bootstrap')['listed']['owners']==[]
 a.call('register-bootstrap',owner='early.owner');prior=a.call('state')['policyGeneration'];a.call('guard',owner='early.owner',id='recovery',enabled=True)
 checks['existingGuardPromotedDurably']=a.call('state')['policyGeneration']==prior+1 and a.call('required-bootstrap')['listed']['owners']==['early.owner']
 a.close();a=Host(root)
 checks['requiredOwnerSurvivesModuleAbsence']=a.call('required-bootstrap')['listed']['owners']==['early.owner']
 a.call('guard',owner='plain.owner',id='plain',enabled=True);a.call('guard',owner='early.owner',id='recovery',enabled=False)
 checks['releaseRemovesOnlyItsBootstrapRequirement']=a.call('required-bootstrap')['listed']['owners']==[] and a.call('state')['guards']==1
 policy=json.loads(file.read_text());policy['bootstrapGuards']={'missing.owner:no-guard':True};file.write_text(json.dumps(policy));checks['requiredGuardMustExist']=a.call('state')['policyValid']==0 and a.call('required-bootstrap')['listed'].get('invalid') is True
 policy['bootstrapGuards']={'plain.owner:plain':1};file.write_text(json.dumps(policy));checks['requiredGuardMustBeBoolean']=a.call('state')['policyValid']==0
 policy['schema']=1;policy['bootstrapGuards']='invalid';file.write_text(json.dumps(policy));checks['legacySchemaCannotSmuggleBootstrapField']=a.call('state')['policyValid']==0 and a.call('required-bootstrap')['listed'].get('invalid') is True
 a.close()
report={'checks':checks,'passed':all(checks.values()),'scope':'Actual macOS OFD kernel locks across processes, failed atomic upgrades, late admission refusal, owner-operation guards, fsync persistence and malformed-file failure closure. Not yet a real SV2 pre-main observation.'};(work/'results.json').write_text(json.dumps(report,indent=2));print(json.dumps(report,indent=2));assert report['passed']
