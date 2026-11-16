import ctypes, pathlib, subprocess, sys, tempfile, time
from ctypes import wintypes
build=pathlib.Path(sys.argv[1]).resolve();root=pathlib.Path(tempfile.mkdtemp(prefix='sv2-pipe-inherit-'));profile=root/'profile';report=profile/'outputs/pipe.txt'
with (root/'launch.out').open('w') as out:
 launch=subprocess.run([str(build/'InheritLauncher.exe'),str(build/'network-host/InheritFixture.exe'),str(profile),str(build/'ExtendCore.dll'),str(report)],stdout=out,timeout=30)
if launch.returncode: raise RuntimeError(f'launcher exit {launch.returncode}')
name,pid=(root/'launch.out').read_text().splitlines();pid=int(pid);kernel=ctypes.WinDLL('kernel32',use_last_error=True);kernel.OpenProcess.restype=wintypes.HANDLE;process=kernel.OpenProcess(0x100001,False,pid)
if not process: raise ctypes.WinError(ctypes.get_last_error())
try:
 deadline=time.monotonic()+10
 while not report.exists() and time.monotonic()<deadline: time.sleep(.05)
 if not report.exists() or report.read_text()!='1': raise RuntimeError('child did not validate server handle')
 with open(name,'r+b',buffering=0) as pipe:
  pipe.write(b'ping\n');response=pipe.read(5)
 print('inherited AppContainer server and desktop client echo',repr(response));assert response==b'ping\n'
finally:
 if kernel.WaitForSingleObject(wintypes.HANDLE(process),3000)==0x102:
  kernel.TerminateProcess(wintypes.HANDLE(process),1);kernel.WaitForSingleObject(wintypes.HANDLE(process),5000)
 kernel.CloseHandle(wintypes.HANDLE(process))
