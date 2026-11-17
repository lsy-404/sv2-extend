"""Build a test-only Core using existing production MinHook objects."""
import argparse
from pathlib import Path
import subprocess
import sys

parser = argparse.ArgumentParser()
parser.add_argument('--build-dir', type=Path, required=True)
parser.add_argument('--out', type=Path, required=True)
parser.add_argument('--compiler', default='g++')
args = parser.parse_args()
test_root = Path(__file__).resolve().parent
repo = test_root.parent
source = repo / 'loader' / 'windows'
build = args.build_dir.resolve()
out = args.out.resolve()
if not out.is_relative_to(test_root) or out == test_root:
    raise ValueError('The output directory must be inside the repository test directory')
objects = [build / ('minhook-' + name + '.o') for name in ('buffer', 'hook', 'trampoline', 'hde64')]
if not all(path.is_file() for path in objects):
    raise ValueError('The production build directory must contain all four MinHook objects')
out.mkdir(parents=True, exist_ok=True)
generated = out / 'category_probe_core.cpp'
binding = out / 'category_probe_binding.h'
binding.write_text(
    '#define SV2_TEST_CATEGORY_CHANGE(id, opening) '
    '((opening ? native_category_registry::open_category(id) : native_category_registry::close_category(id)) == 0)\n'
    '#define SV2_TEST_CATEGORY_OPEN_STATE(id, result) (native_category_registry::category_open(id, result) == 0)\n',
    encoding='utf-8', newline='\n')
subprocess.run([sys.executable, str(test_root / 'windows_category_probe_generate.py'),
                '--core', str(source / 'core.cpp'), '--output', str(generated)], check=True)
flags = ['-std=c++17', '-D_WIN32_WINNT=0x0602', '-O2', '-Wall', '-Wextra', '-Werror',
         '-Wno-cast-function-type', '-Wno-misleading-indentation', '-static', '-static-libgcc',
         '-static-libstdc++', '-I' + str(source), '-I' + str(test_root), '-include', str(binding)]
command = [args.compiler, *flags, '-shared', str(generated), str(source / 'editor_gate.cpp'),
           str(source / 'editor_autostart.cpp'), *(str(path) for path in objects),
           '-o', str(out / 'ExtendCore.dll'), '-luser32', '-lgdi32', '-ladvapi32', '-lshell32',
           '-lole32', '-luuid', '-lbcrypt', '-Wl,--dynamicbase,--nxcompat']
subprocess.run(command, check=True)
print('Built test-only ExtendCore.dll')
