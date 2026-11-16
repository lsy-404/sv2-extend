import argparse, pathlib, subprocess, tempfile
p=argparse.ArgumentParser();p.add_argument('--build',type=pathlib.Path,required=True);p.add_argument('--compiler',default='g++');args=p.parse_args();repo=pathlib.Path(__file__).resolve().parents[1];build=args.build.resolve();root=pathlib.Path(tempfile.mkdtemp(prefix='sv2-path-boundary-'));official=root/'official';official.mkdir();host=official/'synthv-studio.exe';host.write_bytes(b'fixture');sentinel=official/'untouched.txt';sentinel.write_text('original');empty=root/'empty-build';empty.mkdir()
for target in [root,official,official/'nested']:
 result=subprocess.run(['pwsh','-NoProfile','-NonInteractive','-File',str(repo/'loader/windows/install.ps1'),'-BuildDirectory',str(empty),'-HostExecutable',str(host),'-InstallDirectory',str(target)],capture_output=True,text=True,encoding="utf-8",errors="replace",timeout=15)
 assert result.returncode and 'Keep the Loader separate' in (result.stdout+result.stderr),(target,result.stdout,result.stderr)
 assert sentinel.read_text()=='original' and not (official/'nested').exists()
sibling=root/'official-neighbor';result=subprocess.run(['pwsh','-NoProfile','-NonInteractive','-File',str(repo/'loader/windows/install.ps1'),'-BuildDirectory',str(empty),'-HostExecutable',str(host),'-InstallDirectory',str(sibling)],capture_output=True,text=True,encoding="utf-8",errors="replace",timeout=15)
assert result.returncode and 'Keep the Loader separate' not in (result.stdout+result.stderr) and not sibling.exists()
launcher=root/'ExtendLauncher.exe';subprocess.run([args.compiler,'-std=c++17','-D_WIN32_WINNT=0x0602','-O2','-Wall','-Wextra','-Werror','-Wno-cast-function-type','-Wno-misleading-indentation','-static','-municode','-I'+str(build),str(repo/'loader/windows/launcher.cpp'),'-o',str(launcher),'-luserenv','-ladvapi32','-lshell32','-lole32','-luuid'],check=True)
for target in [root,official,official/'nested']:
 result=subprocess.run([str(launcher),'--sandbox-fixture',str(host),str(target)],capture_output=True,text=True,encoding="utf-8",errors="replace",timeout=15)
 assert result.returncode==78,(target,result.returncode,result.stderr)
 assert sentinel.read_text()=='original' and not (official/'nested').exists()
print('installer and launcher reject equal, ancestor and descendant paths before mutation; prefix sibling remains distinct')
