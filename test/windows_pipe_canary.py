import ctypes, pathlib, subprocess, sys, tempfile, time
from ctypes import wintypes
build=pathlib.Path(sys.argv[1]).resolve();root=pathlib.Path(tempfile.mkdtemp(prefix='sv2-pipe-canary-'));profile=root/'profile';report=profile/'outputs/pipe.txt'
with (root/'launcher.out').open('w') as out,(root/'launcher.err').open('w') as err:
 launch=subprocess.run([str(build/'ExtendLauncher.exe'),'--sandbox-fixture',str(build/'network-host/PipeFixture.exe'),str(profile),str(report),'unused'],stdout=out,stderr=err,timeout=30)
if launch.returncode: raise RuntimeError(f'launcher exit {launch.returncode}: '+(root/'launcher.err').read_text())
deadline=time.monotonic()+15
while not report.exists() and time.monotonic()<deadline: time.sleep(.1)
if not report.exists(): raise RuntimeError('fixture did not report')
pid,ok,error=map(int,report.read_text().split());print('server created',ok,'last error',error,flush=True)
if not ok: raise RuntimeError('server pipe creation failed')
kernel=ctypes.WinDLL('kernel32',use_last_error=True);kernel.OpenProcess.restype=wintypes.HANDLE
process=kernel.OpenProcess(0x100001,False,pid)
if not process: raise ctypes.WinError(ctypes.get_last_error())
try:
 name=rf'\\.\pipe\LOCAL\SV2Extend.MCP.{pid}'
 try:
  with open(name,'r+b',buffering=0) as pipe:
   pipe.write(b'ping\n');response=pipe.read(5)
  print('desktop echo',repr(response));assert response==b'ping\n'
 except FileNotFoundError:
  print('desktop open: ERROR_FILE_NOT_FOUND')
finally:
 if kernel.WaitForSingleObject(wintypes.HANDLE(process),2000)==0x102:
  kernel.TerminateProcess(wintypes.HANDLE(process),1);kernel.WaitForSingleObject(wintypes.HANDLE(process),5000)
 kernel.CloseHandle(wintypes.HANDLE(process))
