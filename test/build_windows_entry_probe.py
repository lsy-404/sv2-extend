"""Build isolated common SV controls without changing shipped Core."""
import argparse
from pathlib import Path
import subprocess

parser=argparse.ArgumentParser()
parser.add_argument('--build-dir',type=Path,required=True)
parser.add_argument('--out',type=Path,required=True)
args=parser.parse_args()
test=Path(__file__).resolve().parent
repo=test.parent
source=repo/'loader/windows'
out=args.out.resolve()
if not out.is_relative_to(test) or out==test: raise ValueError('Use an output directory under test')
out.mkdir(parents=True,exist_ok=True)
core=(source/'core.cpp').read_text(encoding='utf-8')
core='#define NATIVE_CATEGORY_TRACE\n'+core
core=core.replace('#include "editor_autostart.h"','#include "editor_autostart.h"\n#include "windows_entry_runtime_probe.h"')
core=core.replace(' native_panel::diagnostic();',' windows_entry_probe::tick(root);\n native_panel::diagnostic();')
assert core.count('load_packages();startup_receipt();')==1

registry=(source/'native_category_registry.h').read_text(encoding='utf-8')
paint='static void bar_paint(void *bar,void *graphics) {'
assert registry.count(paint)==1
registry=registry.replace(paint,'static unsigned probe_native_paints;\n'+paint+'\n ++probe_native_paints;')
(out/'native_category_registry.h').write_text(registry,encoding='utf-8')
generated=out/'entry_probe_core.cpp'
generated.write_text(core,encoding='utf-8')
objects=[args.build_dir.resolve()/('minhook-'+name+'.o') for name in ('buffer','hook','trampoline','hde64')]
assert all(p.is_file() for p in objects)
flags=['-std=c++17','-D_WIN32_WINNT=0x0602','-O2','-Wall','-Wextra','-Werror','-Wno-cast-function-type','-Wno-misleading-indentation','-static','-static-libgcc','-static-libstdc++','-I'+str(source),'-I'+str(test),'-I'+str(args.build_dir.resolve())]
subprocess.run(['g++',*flags,'-shared',str(generated),str(source/'editor_gate.cpp'),str(source/'editor_autostart.cpp'),str(source/'javascript_gate.cpp'),str(source/'voice_adapter.cpp'),*map(str,objects),'-o',str(out/'ExtendCore.dll'),'-luser32','-lgdi32','-ladvapi32','-lshell32','-lole32','-loleaut32','-liphlpapi','-lws2_32','-luuid','-lbcrypt','-Wl,--dynamicbase,--nxcompat'],check=True)
print('Built isolated entry probe Core')
