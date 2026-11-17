from pathlib import Path
import os,subprocess,tempfile
root=Path(__file__).resolve().parents[1]
out=root/'test'/'.outputs'/'translation-fixture'
out.mkdir(parents=True,exist_ok=True)
inputs=out/'input';inputs.mkdir(exist_ok=True)
minhook=root/'loader'/'windows'/'vendor'/'minhook'/'src'
objects=[]
for source in ['buffer.c','hook.c','trampoline.c','hde/hde64.c']:
 obj=out/(Path(source).stem+'.o');subprocess.run(['gcc','-O2','-c',str(minhook/source),'-o',str(obj)],check=True);objects.append(str(obj))
flags=['g++','-std=c++17','-D_WIN32_WINNT=0x0602','-municode','-static']
subprocess.run([*flags,str(root/'test/windows_translation_probe_child.cpp'),*objects,'-o',str(inputs/'child.exe'),'-lshell32','-lole32','-luuid'],check=True)
subprocess.run([*flags,str(root/'test/windows_translation_probe_launcher.cpp'),'-o',str(out/'launcher.exe'),'-luserenv','-ladvapi32','-lshell32','-lole32','-luuid'],check=True)
core=root/'test/.outputs/language-core-build/ExtendCore.dll'
if not core.exists():raise SystemExit('Build the Windows core in test/.outputs/language-core-build first.')
source=Path(os.environ['PROGRAMDATA'])/'Dreamtonics/Synthesizer V Studio 2/translations'
expected_count=len(list(source.glob('*.txt')))
assert expected_count and (source/'zh-cn.txt').is_file()
with tempfile.TemporaryDirectory(prefix='sv2-translation-') as parent:
 profile=Path(parent)/'profile'
 def launch():
  subprocess.run([str(out/'launcher.exe'),str(inputs/'child.exe'),str(profile),str(core)],check=True)
  return (profile/'outputs/translation-probe.txt').read_text()
 first=launch()
 for expected in ('known=1','folder=1','resolved=1',f'directCount={expected_count}','findError=0','chineseReadable=1','chineseContent=1'):assert expected in first,first
 target=profile/'host-data/ProgramData/Dreamtonics/Synthesizer V Studio 2/translations/zh-cn.txt'
 target.write_bytes(b'preserve-owned-existing-file')
 launch()
 assert target.read_bytes()==b'preserve-owned-existing-file'
print('translation namespace and existing destination checks passed')
