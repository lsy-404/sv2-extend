"""Run transport regressions in a fresh managed SV2 data namespace.

Never use the official executable directly: the fixture's host data binding is
prepared by a separately identified copy of the installed launcher.
"""
from pathlib import Path
import json, os, plistlib, shutil, subprocess, sys, uuid
source=Path(__file__).resolve().parent;base=source.parents[3]
work=base/'work/policy-transports';work.mkdir(parents=True,exist_ok=True)
run=Path.home()/'Library/Application Support/Dreamtonics/Open SV2 Loader/experiments/policy-transports'/uuid.uuid4().hex
run.mkdir(parents=True,mode=0o700);root=run/'profile';probe=work/('Extend Policy '+run.name+'.app')
subprocess.run(['ditto',str(Path.home()/'Applications/Extend Loader.app'),str(probe)],check=True)
shutil.copy2(source/'loader-probe.dylib',probe/'Contents/Resources/Native/loader-probe.dylib')
domain='io.opensv2.policy-probe.p'+run.name;infoPath=probe/'Contents/Info.plist';info=plistlib.loads(infoPath.read_bytes());info['CFBundleIdentifier']=domain;infoPath.write_bytes(plistlib.dumps(info))
subprocess.run(['codesign','--force','--sign','-',str(probe)],check=True,capture_output=True)
empty=run/'empty';empty.mkdir();subprocess.run(['defaults','write',domain,'loaderDataDirectory',str(empty)],check=True)
prepared=subprocess.run([str(probe/'Contents/MacOS/ExtendLoader'),'--data-dir',str(root),'--isolate-host-data','--prepare-only'],check=True,capture_output=True,text=True)
managed=Path(prepared.stdout.strip());host=managed/'Contents/MacOS/synthv-studio-original'
env={k:v for k,v in os.environ.items() if not k.startswith(('OPEN_SV2_','LOADER_PROBE_','DYLD_'))}
env.update(LOADER_PROBE_DIRECT_HOST=str(host),LOADER_PROBE_LIBRARY=str(source/'loader-probe.dylib'),OPEN_SV2_LOADER_DATA_DIR=str(root),OPEN_SV2_HOST_DATA_HOME=str(root/'host-home'),OPEN_SV2_ISOLATE_HOST_DATA='1',OPEN_SV2_ISOLATE_WEBSITE_DATA='1',LOADER_PROBE_RUNTIME_DIR=str(root),LOADER_MATRIX_OUTPUT_DIR=str(run))
handle={'root':str(root),'managedApplication':str(managed),'executable':str(host),'runDirectory':str(run)}
(work/'prepared-host.json').write_text(json.dumps(handle,indent=2));print(json.dumps(handle),flush=True)
for driver,name in [('run-transport-matrix.py','matrix'),('run-urlsession-callback-probe.py','callbacks')]:
 with (run/(name+'.driver.log')).open('wb') as output:
  result=subprocess.run([sys.executable,str(source/driver)],env=env,stdout=output,stderr=subprocess.STDOUT)
 if result.returncode:raise RuntimeError(f'{driver} returned {result.returncode}; inspect its precise host handle/log before any restart: {run}')
report={'handle':handle,'matrix':json.loads((run/'transport-matrix-results.json').read_text()),'callbacks':json.loads((root/'results.json').read_text())}
report['passed']=bool(report['matrix']['passed'] and report['callbacks']['passed']);report['scope']='Actual SV2 in unique isolated data namespace, production candidate Core; independently observed socket/NW/WebKit and URLSession callback regressions, not whole-host isolation.'
(work/'results.json').write_text(json.dumps(report,indent=2));print(json.dumps({'passed':report['passed'],'runDirectory':str(run)},indent=2));assert report['passed']
