"""Build one C source into a multi-platform .sv2ext, without host binaries."""
from pathlib import Path
import argparse, hashlib, json, shutil, subprocess, tempfile, zipfile
parser=argparse.ArgumentParser();parser.add_argument('--platform',action='append',choices=['macos-arm64','macos-x86_64','windows-x86_64'],required=True);parser.add_argument('--out',type=Path,required=True);parser.add_argument('--version',default='1.0.0');args=parser.parse_args()
source=Path(__file__).resolve().parent;sdk=source.parent;target=args.out.resolve();target.parent.mkdir(parents=True,exist_ok=True)
if target.exists():raise SystemExit('Preserve existing package: choose a new output path.')
libraries={}
with tempfile.TemporaryDirectory(prefix='extend-hello-build-') as temporary:
 root=Path(temporary)
 for platform in sorted(set(args.platform)):
  suffix='dll' if platform.startswith('windows') else 'dylib';relative=f'native/{platform}/plugin.{suffix}';binary=root/relative;binary.parent.mkdir(parents=True)
  if platform.startswith('macos'):
   architecture='arm64' if platform.endswith('arm64') else 'x86_64';command=['xcrun','clang','-arch',architecture,'-dynamiclib','-O2','-Wall','-Wextra','-Werror']
  else:command=['x86_64-w64-mingw32-gcc','-shared','-static-libgcc','-O2','-Wall','-Wextra','-Werror']
  subprocess.run([*command,'-I'+str(sdk/'include'),str(source/'hello_plugin.c'),'-o',str(binary)],check=True);libraries[platform]=relative
 manifest={'schema':1,'loaderAPI':1,'id':'example.hello','name':'Hello','version':args.version,'enabled':True,'nativeLibraries':libraries};(root/'plugin.json').write_text(json.dumps(manifest,indent=2))
 # ZIP writing is just packaging; runtime extraction stays inside Loader.
 with zipfile.ZipFile(target,'x',compression=zipfile.ZIP_DEFLATED) as archive:
  for path in sorted(root.rglob('*')):
   if path.is_file():archive.write(path,path.relative_to(root))
print(json.dumps({'package':str(target),'platforms':list(libraries),'sha256':hashlib.sha256(target.read_bytes()).hexdigest()},indent=2))
