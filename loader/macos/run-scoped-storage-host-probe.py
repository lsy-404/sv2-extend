from pathlib import Path
import json,subprocess,time
from host_probe_support import prepare_host
source=Path(__file__).resolve().parent;work=source.parents[3]/'work/scoped-storage';fixture=work/'fixture.dylib'
subprocess.run(['xcrun','clang','-dynamiclib','-Wall','-Wextra','-Werror',str(source/'scoped_storage_fixture.c'),'-o',str(fixture)],check=True)
handle,env=prepare_host(source,work,'scoped-storage');run=Path(handle['runDirectory']);output=run/'result.json';env.update(LOADER_PROBE_PLUGIN=str(fixture),EXTEND_STORAGE_RESULT=str(output))
with (run/'stdout.log').open('wb') as out,(run/'stderr.log').open('wb') as err:p=subprocess.Popen([handle['executable']],env=env,stdout=out,stderr=err,start_new_session=True)
handle['pid']=p.pid;(work/'live-host.json').write_text(json.dumps(handle,indent=2));print(json.dumps(handle),flush=True);deadline=time.monotonic()+25
while p.poll() is None and not output.exists() and time.monotonic()<deadline:time.sleep(.1)
if not output.exists():raise RuntimeError('Inspect same host PID '+str(p.pid)+'; storage observation incomplete')
value=json.loads(output.read_text());checks={'sameHost':value['pid']==p.pid,'appendedABIAvailable':value['apiSize']>=264,'writesAccepted':value['writeA']==value['writeB']==0,'firstPluginRoundTrip':bool(value['readA']),'secondPluginRoundTrip':bool(value['readB']),'smallBufferUnchanged':bool(value['smallBufferUnchanged']),'sizeQuery':bool(value['sizeQuery']),'hostAlive':p.poll() is None};report={'handle':handle,'checks':checks,'passed':all(checks.values()),'scope':'Actual isolated SV2 native SDK table, two plugin namespaces, bounded binary read/write and buffer semantics. No account transaction or whole-host isolation claim.'};(work/'host-results.json').write_text(json.dumps(report,indent=2));print(json.dumps(report,indent=2))
if p.poll() is None:p.terminate();p.wait(timeout=8)
assert report['passed']
