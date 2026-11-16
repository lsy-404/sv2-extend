from pathlib import Path
import subprocess
source=Path(__file__).resolve().parent;work=source.parents[3]/'work/webkit-realtime';work.mkdir(parents=True,exist_ok=True)
subprocess.run(['xcrun','clang','-fobjc-arc','-Wall','-Wextra','-Werror',str(source/'webkit_rtc_preferences_test.m'),str(source/'webkit_rtc_gate.m'),'-framework','Foundation','-framework','WebKit','-o',str(work/'preferences-test')],check=True)
subprocess.run([str(work/'preferences-test'),str(work/'preferences-results.json')],check=True,timeout=10)
