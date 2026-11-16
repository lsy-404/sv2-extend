from pathlib import Path
import subprocess
source=Path(__file__).resolve().parent;work=source.parents[3]/'work/network-transition';work.mkdir(parents=True,exist_ok=True)
subprocess.run(['xcrun','clang','-fobjc-arc','-Wall','-Wextra','-Werror',str(source/'network_transition_test.m'),str(source/'network_gate.m'),'-framework','Foundation','-o',str(work/'test')],check=True)
subprocess.run([str(work/'test'),str(work/'results.json')],check=True,timeout=12)
