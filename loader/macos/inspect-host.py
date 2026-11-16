#!/usr/bin/env python3
"""Read-only SV2 bundle inspection. Does not launch or modify the target."""
import hashlib,json,pathlib,plistlib,subprocess,sys
if len(sys.argv)!=2: raise SystemExit('usage: inspect-host.py /path/to/Synthesizer V Studio 2.app')
bundle=pathlib.Path(sys.argv[1]).resolve()
with (bundle/'Contents/Info.plist').open('rb') as f: info=plistlib.load(f)
exe=bundle/'Contents/MacOS'/info['CFBundleExecutable']
def run(args):
 p=subprocess.run(args,text=True,capture_output=True)
 return {'exit_code':p.returncode,'stdout':p.stdout,'stderr':p.stderr}
report={'bundle':str(bundle),'executable':str(exe),'version':info.get('CFBundleShortVersionString'),
 'bundle_id':info.get('CFBundleIdentifier'),'sha256':hashlib.file_digest(exe.open('rb'),'sha256').hexdigest(),
 'architecture':run(['file',str(exe)]),'signature':run(['codesign','-dv','--verbose=4',str(bundle)]),
 'entitlements':run(['codesign','-d','--entitlements',':-',str(bundle)]),
 'verification':run(['codesign','--verify','--strict','--verbose=2',str(bundle)]),
 'linked_libraries':run(['otool','-L',str(exe)])}
print(json.dumps(report,indent=2,ensure_ascii=False))
