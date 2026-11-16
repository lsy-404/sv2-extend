from pathlib import Path
import json,os,plistlib,subprocess,time,uuid
from host_probe_support import prepare_host
source=Path(__file__).resolve().parent;work=source.parents[3]/'work/web-context';work.mkdir(parents=True,exist_ok=True);fixture=work/'fixture.dylib'
subprocess.run(['xcrun','clang','-dynamiclib','-fobjc-arc','-Wall','-Wextra','-Werror',str(source/'web_context_fixture.m'),'-framework','AppKit','-framework','WebKit','-o',str(fixture)],check=True)
if os.environ.get('EXTEND_WEB_CONTEXT_STANDALONE'):
 run=work/('standalone-'+uuid.uuid4().hex);run.mkdir(mode=0o700);root=run/'profile';root.mkdir(mode=0o700);app=run/'Fixture.app';(app/'Contents/MacOS').mkdir(parents=True);exe=app/'Contents/MacOS/fixture';(app/'Contents/Info.plist').write_bytes(plistlib.dumps({'CFBundleIdentifier':'io.opensv2.web-context.p'+run.name.split('-')[-1],'CFBundleExecutable':'fixture','CFBundlePackageType':'APPL','LSUIElement':True}));subprocess.run(['xcrun','clang','-fobjc-arc',str(source/'webkit_fixture_host.m'),'-framework','AppKit','-o',str(exe)],check=True);handle={'root':str(root),'executable':str(exe),'runDirectory':str(run),'standalone':True};env={k:v for k,v in os.environ.items() if not k.startswith(('DYLD_','OPEN_SV2_','LOADER_PROBE_'))};env.update(DYLD_INSERT_LIBRARIES=str(source/'loader-probe.dylib'),OPEN_SV2_LOADER_DATA_DIR=str(root),OPEN_SV2_ISOLATE_WEBSITE_DATA='1')
else:handle,env=prepare_host(source,work,'web-context')
run=Path(handle['runDirectory']);output=run/'result.json';env.update(LOADER_PROBE_PLUGIN=str(fixture),EXTEND_WEB_CONTEXT_RESULT=str(output))
with (run/'stdout.log').open('wb') as out,(run/'stderr.log').open('wb') as err:p=subprocess.Popen([handle['executable']],env=env,stdout=out,stderr=err,start_new_session=True)
handle['pid']=p.pid;(work/'live-host.json').write_text(json.dumps(handle,indent=2));print(json.dumps(handle),flush=True);deadline=time.monotonic()+30
while p.poll() is None and not output.exists() and time.monotonic()<deadline:time.sleep(.1)
if not output.exists():raise RuntimeError('Inspect same live host PID '+str(p.pid)+'; incomplete context observation')
value=json.loads(output.read_text());report={'handle':handle,'fixture':value,'passed':all(value['checks'].values()),'scope':'Native renderer-only scope control, combined renderer/network process retirement, kernel PID exits, pending native JS completion and persistent test cookie preservation. Not whole-host network isolation.'};name='standalone-results.json' if handle.get('standalone') else 'host-results.json';(work/name).write_text(json.dumps(report,indent=2));print(json.dumps(report,indent=2))
if p.poll() is None:p.terminate();p.wait(timeout=8)
assert report['passed']
