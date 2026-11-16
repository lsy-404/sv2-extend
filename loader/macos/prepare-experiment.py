#!/usr/bin/env python3
"""Create a disposable, locally re-signed host copy. Never modifies the source."""
import argparse,pathlib,plistlib,shutil,subprocess
root=pathlib.Path(__file__).resolve().parent
parser=argparse.ArgumentParser();parser.add_argument('--source',type=pathlib.Path,default=pathlib.Path('/Applications/Synthesizer V Studio 2 Pro.app'));parser.add_argument('--destination',type=pathlib.Path,default=root/'experimental/SV2 Loader Probe.app');args=parser.parse_args()
source=args.source.resolve();app=args.destination.resolve()
if source==app or app.exists():raise SystemExit('Destination must be new and distinct from the source; preserve existing experiments.')
subprocess.run(['codesign','--verify','--strict',str(source)],check=True)
shutil.copytree(source,app)
plist=app/'Contents/Info.plist';info=plistlib.loads(plist.read_bytes());exe=app/'Contents/MacOS';original=exe/info['CFBundleExecutable'];original.rename(exe/'synthv-studio-original')
info.update(CFBundleIdentifier='com.dreamtonics.svstudio2.pro.loaderprobe',CFBundleName='SV2 Loader Probe',CFBundleExecutable='loader-launcher');plist.write_bytes(plistlib.dumps(info))
shutil.copy2(root/'loader-launcher',exe/'loader-launcher')
for folder,name,dest in [('Frameworks','loader-probe.dylib','OpenSV2Loader.dylib'),('PlugIns','network-plugin.dylib','network-plugin.dylib')]:
 target=app/'Contents'/folder;target.mkdir(exist_ok=True);shutil.copy2(root/name,target/dest)
ent=app.parent/'loader-test-entitlements.plist';ent.write_bytes(plistlib.dumps({'com.apple.security.cs.allow-jit':True,'com.apple.security.cs.allow-unsigned-executable-memory':True,'com.apple.security.cs.allow-dyld-environment-variables':True,'com.apple.security.cs.disable-library-validation':True}))
for target in [app/'Contents/Frameworks/OpenSV2Loader.dylib',app/'Contents/PlugIns/network-plugin.dylib']:
 subprocess.run(['codesign','--force','--sign','-',str(target)],check=True)
for target in [exe/'synthv-studio-original',app]:
 subprocess.run(['codesign','--force','--sign','-','--options','runtime','--entitlements',str(ent),str(target)],check=True)
subprocess.run(['codesign','--verify','--deep','--strict',str(app)],check=True)
print(app)
