"""Isolated test-host preparation. No credential values or vendor binaries ship."""
from pathlib import Path
import json, os, plistlib, shutil, subprocess, uuid

def prepare_host(source, work, purpose):
    source=Path(source);work=Path(work);work.mkdir(parents=True,exist_ok=True)
    run=Path.home()/'Library/Application Support/Dreamtonics/Open SV2 Loader/experiments'/purpose/uuid.uuid4().hex
    run.mkdir(parents=True,mode=0o700);root=run/'profile';probe=work/('Extend Probe '+run.name+'.app')
    subprocess.run(['ditto',str(Path.home()/'Applications/Extend Loader.app'),str(probe)],check=True)
    shutil.copy2(source/'loader-probe.dylib',probe/'Contents/Resources/Native/loader-probe.dylib')
    domain='io.opensv2.isolated-probe.p'+run.name;infoPath=probe/'Contents/Info.plist'
    info=plistlib.loads(infoPath.read_bytes());info['CFBundleIdentifier']=domain;infoPath.write_bytes(plistlib.dumps(info))
    subprocess.run(['codesign','--force','--sign','-',str(probe)],check=True,capture_output=True)
    empty=run/'empty';empty.mkdir();subprocess.run(['defaults','write',domain,'loaderDataDirectory',str(empty)],check=True)
    prepared=subprocess.run([str(probe/'Contents/MacOS/ExtendLoader'),'--data-dir',str(root),'--isolate-host-data','--prepare-only'],check=True,capture_output=True,text=True)
    managed=Path(prepared.stdout.strip());host=managed/'Contents/MacOS/synthv-studio-original'
    env={k:v for k,v in os.environ.items() if not k.startswith(('OPEN_SV2_','LOADER_PROBE_','DYLD_'))}
    env.update(LOADER_PROBE_DIRECT_HOST=str(host),LOADER_PROBE_LIBRARY=str(source/'loader-probe.dylib'),DYLD_INSERT_LIBRARIES=str(source/'loader-probe.dylib'),OPEN_SV2_LOADER_DATA_DIR=str(root),OPEN_SV2_HOST_DATA_HOME=str(root/'host-home'),OPEN_SV2_ISOLATE_HOST_DATA='1',OPEN_SV2_ISOLATE_WEBSITE_DATA='1')
    handle={'root':str(root),'managedApplication':str(managed),'executable':str(host),'runDirectory':str(run),'probeApplication':str(probe)}
    (work/'prepared-host.json').write_text(json.dumps(handle,indent=2))
    return handle,env
