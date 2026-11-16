import argparse, ctypes, pathlib, shutil, subprocess, tempfile, time
from ctypes import wintypes
p=argparse.ArgumentParser();p.add_argument('--build',type=pathlib.Path,required=True);p.add_argument('--launcher-source',type=pathlib.Path,required=True);p.add_argument('--compiler',default='g++');args=p.parse_args();repo=pathlib.Path(__file__).resolve().parents[1];build=args.build.resolve();root=pathlib.Path(tempfile.mkdtemp(prefix='sv2-runtime-acl-'));binary=root/'binary';binary.mkdir();hostdir=root/'host';hostdir.mkdir();profile=root/'profile'
def run(*args_): subprocess.run([args.compiler,'-std=c++17','-D_WIN32_WINNT=0x0602','-O2','-Wall','-Wextra','-Werror','-Wno-cast-function-type','-Wno-misleading-indentation','-static','-municode',*map(str,args_)],check=True)
source=root/'launcher.cpp';shutil.copyfile(args.launcher_source,source);shutil.copyfile(build/'ExtendCore.dll',binary/'ExtendCore.dll')
run('-I'+str(repo/'loader/windows'),'-I'+str(build),source,'-o',binary/'ExtendLauncher.exe','-luserenv','-ladvapi32','-lshell32','-lole32','-luuid')
run(repo/'test/windows_runtime_acl_host.cpp','-o',hostdir/'Probe.exe','-lshell32','-lole32','-luuid')
(hostdir/'asset.txt').write_text('one');(hostdir/'delete.txt').write_text('preserve');(profile/'plugins').mkdir(parents=True);(profile/'plugins/delete.txt').write_text('preserve')
class FileTime(ctypes.Structure): _fields_=[('low',wintypes.DWORD),('high',wintypes.DWORD)]
class FileAttributes(ctypes.Structure): _fields_=[('attributes',wintypes.DWORD),('created',FileTime),('accessed',FileTime),('modified',FileTime),('size_high',wintypes.DWORD),('size_low',wintypes.DWORD)]
kernel=ctypes.WinDLL('kernel32',use_last_error=True);kernel.CreateFileW.restype=wintypes.HANDLE
kernel.GetFileAttributesExW.argtypes=[wintypes.LPCWSTR,ctypes.c_int,ctypes.c_void_p]
kernel.SetFileTime.argtypes=[wintypes.HANDLE,ctypes.c_void_p,ctypes.c_void_p,ctypes.POINTER(FileTime)]
def raw_time(path):
 data=FileAttributes()
 if not kernel.GetFileAttributesExW(str(path),0,ctypes.byref(data)): raise ctypes.WinError(ctypes.get_last_error())
 return (data.modified.high<<32)|data.modified.low
def odd_tick(path):
 value=raw_time(path)|1;stamp=FileTime(value&0xffffffff,value>>32);handle=kernel.CreateFileW(str(path),0x100,7,None,3,0,None)
 if handle==wintypes.HANDLE(-1).value: raise ctypes.WinError(ctypes.get_last_error())
 try:
  if not kernel.SetFileTime(wintypes.HANDLE(handle),None,None,ctypes.byref(stamp)): raise ctypes.WinError(ctypes.get_last_error())
 finally: kernel.CloseHandle(wintypes.HANDLE(handle))
 assert raw_time(path)==value
for name in ('Probe.exe','asset.txt','delete.txt'): odd_tick(hostdir/name)
def assert_times():
 for name in ('Probe.exe','asset.txt','delete.txt'):
  assert raw_time(hostdir/name)==raw_time(profile/'runtime/Synthesizer V Studio 2 Pro'/name),f'FILETIME mismatch: {name}'
def launch(index):
 report=profile/'outputs'/f'result-{index}.txt';out=root/f'launcher-{index}.out';err=root/f'launcher-{index}.err'
 with out.open('w') as stdout,err.open('w') as stderr: result=subprocess.run([str(binary/'ExtendLauncher.exe'),'--sandbox-fixture',str(hostdir/'Probe.exe'),str(profile),str(report)],stdout=stdout,stderr=stderr,timeout=30)
 if result.returncode: raise RuntimeError(f'launcher exit {result.returncode}: '+err.read_text())
 deadline=time.monotonic()+5
 while not report.exists() and time.monotonic()<deadline:time.sleep(.05)
 if not report.exists():raise RuntimeError('missing probe result')
 fields=report.read_text().split();assert fields[:9]==['0','0','0','0','0','1','1','1','1'],fields
 time.sleep(1.2);return fields[9]
assert launch(1)=='one'
assert_times()
# Recreate the previous all-access profile ACL before checking upgrade convergence.
userenv=ctypes.WinDLL('userenv',use_last_error=True);sid=wintypes.LPVOID();name=(profile/'container-name.txt').read_text().strip();userenv.DeriveAppContainerSidFromAppContainerName.argtypes=[wintypes.LPCWSTR,ctypes.POINTER(wintypes.LPVOID)];assert userenv.DeriveAppContainerSidFromAppContainerName(name,ctypes.byref(sid))==0
advapi=ctypes.WinDLL('advapi32',use_last_error=True);sidtext=wintypes.LPWSTR();assert advapi.ConvertSidToStringSidW(sid,ctypes.byref(sidtext));sidvalue=sidtext.value;ctypes.windll.kernel32.LocalFree(sidtext);ctypes.windll.advapi32.FreeSid(sid)
subprocess.run(['icacls',str(profile),'/grant',f'*{sidvalue}:(OI)(CI)F','/T','/Q'],check=True,capture_output=True)
(hostdir/'asset.txt').write_text('two-changed')
odd_tick(hostdir/'asset.txt')
assert launch(2)=='two-changed'
assert_times()
assert (profile/'runtime/Synthesizer V Studio 2 Pro/delete.txt').exists()
before={p.name for p in (profile/'runtime').iterdir() if p.name.startswith('.previous-')}
assert launch(3)=='two-changed'
assert_times()
assert before=={p.name for p in (profile/'runtime').iterdir() if p.name.startswith('.previous-')},'unchanged runtime was recopied'
print('runtime and profile root deny writes/deletes; data directories allow read/write/delete; old full ACL converges; changed resource copied')
