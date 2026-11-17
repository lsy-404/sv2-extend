import argparse, ctypes, json, os, pathlib, shutil, subprocess, tempfile, time
from ctypes import wintypes
p=argparse.ArgumentParser();p.add_argument('--build',type=pathlib.Path,required=True);p.add_argument('--compiler',default='g++');p.add_argument('--delay-launcher-source',type=pathlib.Path);args=p.parse_args()
build=args.build.resolve();tests=pathlib.Path(__file__).resolve().parent;root=pathlib.Path(tempfile.mkdtemp(prefix='sv2-fatal-startup-'));hostdir=root/'host';hostdir.mkdir()
def compile_file(*arguments):
 subprocess.run([args.compiler,'-std=c++17','-D_WIN32_WINNT=0x0602','-O2','-Wall','-Wextra','-Werror','-static',*map(str,arguments)],check=True)
if args.delay_launcher_source:
 source=args.delay_launcher_source.resolve();text=source.read_text(encoding='utf-8');needle='const ULONGLONG deadline=GetTickCount64()+15000;'
 if text.count(needle)!=1: raise RuntimeError('launcher polling point not found')
 instrumented=root/'launcher.cpp';instrumented.write_text(text.replace(needle,'Sleep(250);'+needle),encoding='utf-8')
 original_build=build;build=root/'delayed-build';build.mkdir();shutil.copyfile(original_build/'ExtendCore.dll',build/'ExtendCore.dll')
 compile_file('-Wno-cast-function-type','-Wno-misleading-indentation','-I'+str(source.parent),'-I'+str(original_build),'-municode',instrumented,'-o',build/'ExtendLauncher.exe','-luserenv','-ladvapi32','-lshell32','-lole32','-loleaut32','-luuid')
compile_file('-municode',tests/'windows_fatal_startup_host.cpp','-o',hostdir/'FatalStartupHost.exe','-lshell32','-lole32','-luuid')
for fatal in (False,True):
 dll=root/('fatal.dll' if fatal else 'control.dll');flags=['-DFATAL_STARTUP'] if fatal else []
 compile_file('-shared',*flags,tests/'windows_fatal_startup_plugin.cpp','-o',dll)
 profile=root/('fatal-profile' if fatal else 'control-profile');package=profile/'plugins/test.startup';library=package/'native/windows-x86_64/plugin.dll';library.parent.mkdir(parents=True);library.write_bytes(dll.read_bytes())
 (package/'plugin.json').write_text(json.dumps({'schema':1,'id':'test.startup','name':'Startup fixture','version':'1.0.0','nativeLibraries':{'windows-x86_64':'native/windows-x86_64/plugin.dll'}}),encoding='utf-8')
 marker=profile/'outputs/main.marker';launch_out=root/('fatal.out' if fatal else 'control.out');launch_err=root/('fatal.err' if fatal else 'control.err')
 with launch_out.open('w') as stdout,launch_err.open('w') as stderr:
  env=os.environ.copy();env['SV2_TEST_MAIN_MARKER']=str(marker)
  result=subprocess.run([str(build/'ExtendLauncher.exe'),'--managed-fixture',str(hostdir/'FatalStartupHost.exe'),str(profile),str(marker)],stdout=stdout,stderr=stderr,timeout=30,env=env)
 receipt_path=profile/'outputs/windows-runtime.json';deadline=time.monotonic()+5
 while not receipt_path.exists() and time.monotonic()<deadline: time.sleep(.05)
 if not receipt_path.exists(): raise AssertionError('missing runtime receipt; launcher exit '+str(result.returncode))
 receipt=json.loads(receipt_path.read_text());assert not receipt['appContainer'] and receipt['managed'];entries=[x for x in receipt['packages'] if x.get('id')=='test.startup'];assert len(entries)==1 and entries[0]['loaded'] and entries[0]['initResult']==(-10 if fatal else 0)
 if fatal:
  assert result.returncode != 0, 'fatal plugin did not abort startup'
  time.sleep(.25);assert marker.exists() and marker.read_text()=='armed', 'host main ran after fatal plugin failure'
 else:
  assert result.returncode==0, 'control launcher failed: '+str(result.returncode)
  deadline=time.monotonic()+5
  while (not marker.exists() or marker.read_text()!='armedmain entered') and time.monotonic()<deadline: time.sleep(.05)
  assert marker.exists() and marker.read_text()=='armedmain entered', 'control host main did not run'
  kernel=ctypes.WinDLL('kernel32',use_last_error=True);kernel.OpenProcess.restype=wintypes.HANDLE
  process=kernel.OpenProcess(0x100000,False,receipt['hostPid'])
  if process:
   try:
    assert kernel.WaitForSingleObject(wintypes.HANDLE(process),5000)==0, 'control host did not exit'
   finally: kernel.CloseHandle(wintypes.HANDLE(process))
 print(('fatal: plugin -10, host main not entered' if fatal else 'control: plugin 0, host main entered')+'; managed full-trust receipt verified')
