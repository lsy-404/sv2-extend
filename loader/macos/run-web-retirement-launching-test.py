from pathlib import Path
import subprocess
source=Path(__file__).resolve().parent;work=source.parents[3]/'work/webkit-realtime';work.mkdir(parents=True,exist_ok=True)
subprocess.run(['xcrun','clang','-fobjc-arc','-Wall','-Wextra','-Werror',str(source/'web_retirement_launching_test.m'),str(source/'web_content_retirement.m'),str(source/'web_store_registry.m'),'-framework','Foundation','-framework','WebKit','-o',str(work/'launching-test')],check=True)
subprocess.run([str(work/'launching-test'),str(work/'launching-results.json')],check=True,timeout=8)
