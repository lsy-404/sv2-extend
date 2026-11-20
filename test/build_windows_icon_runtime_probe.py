"""Build a test-only Windows Core that exercises icon changes in a managed host."""
import argparse
from pathlib import Path
import subprocess

parser = argparse.ArgumentParser()
parser.add_argument("--build-dir", type=Path, required=True)
parser.add_argument("--out", type=Path, required=True)
args = parser.parse_args()
test = Path(__file__).resolve().parent
source = test.parent / "loader/windows"
build = args.build_dir.resolve()
out = args.out.resolve()
if not out.is_relative_to(test) or out == test:
    raise ValueError("Use an output directory under test")
out.mkdir(parents=True, exist_ok=True)
core = (source / "core.cpp").read_text(encoding="utf-8")
if core.count("void category_tick(){") != 1 or core.count(" native_panel::diagnostic();") != 1:
    raise ValueError("Core entry points changed")
core = core.replace("void category_tick(){",
                    '#include "windows_icon_runtime_probe.h"\nvoid category_tick(){', 1)
core = core.replace(" native_panel::diagnostic();",
                    " icon_test_tick();\n native_panel::diagnostic();", 1)
generated = out / "windows_icon_runtime_probe_core.cpp"
generated.write_text(core, encoding="utf-8")
objects = sorted(build.glob("minhook-*.o")) + sorted(build.glob("lua-*.o")) + [build / "picohttpparser.o"]
if len(list(build.glob("minhook-*.o"))) != 4 or not list(build.glob("lua-*.o")) or not objects[-1].is_file():
    raise ValueError("A complete Windows build directory is required")
flags = ["-std=c++17", "-D_WIN32_WINNT=0x0602", "-O2", "-Wall", "-Wextra", "-Werror",
         "-Wno-cast-function-type", "-Wno-misleading-indentation", "-static",
         "-static-libgcc", "-static-libstdc++", "-I" + str(build),
         "-I" + str(source), "-I" + str(test)]
sources = [generated, source / "editor_gate.cpp", source / "editor_autostart.cpp",
           source / "javascript_gate.cpp", source / "voice_adapter.cpp"]
libraries = ["-luser32", "-lgdi32", "-ladvapi32", "-lshell32", "-lole32",
             "-loleaut32", "-liphlpapi", "-lws2_32", "-luuid", "-lbcrypt", "-lwinhttp"]
subprocess.run(["g++", *flags, "-shared", *map(str, sources), *map(str, objects),
                "-o", str(out / "ExtendCore.dll"), *libraries], check=True)
