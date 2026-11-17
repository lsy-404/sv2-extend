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
core=core.replace('#include "editor_autostart.h"','#include "editor_autostart.h"\n#include "native_common_runtime_probe.h"')
core=core.replace(' native_panel::diagnostic();',' native_panel::diagnostic();')
assert core.count('load_packages();startup_receipt();')==1
core=core.replace('load_packages();startup_receipt();','load_packages();if(!native_ui_probe::install(api,dispatcher))return LOADER_ERR_INVALID;startup_receipt();')
generated=out/'native_control_core.cpp'
generated.write_text(core,encoding='utf-8')
objects=[args.build_dir.resolve()/('minhook-'+name+'.o') for name in ('buffer','hook','trampoline','hde64')]
assert all(p.is_file() for p in objects)
flags=['-std=c++17','-D_WIN32_WINNT=0x0602','-O2','-Wall','-Wextra','-Werror','-Wno-cast-function-type','-Wno-misleading-indentation','-static','-static-libgcc','-static-libstdc++','-I'+str(source),'-I'+str(test)]
subprocess.run(['g++',*flags,'-shared',str(generated),str(source/'editor_gate.cpp'),str(source/'editor_autostart.cpp'),*map(str,objects),'-o',str(out/'ExtendCore.dll'),'-luser32','-lgdi32','-ladvapi32','-lshell32','-lole32','-luuid','-lbcrypt','-Wl,--dynamicbase,--nxcompat'],check=True)
print('Built isolated common SV control Core')
