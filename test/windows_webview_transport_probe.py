import json
import os
from pathlib import Path
import queue
import subprocess
import sys
import tempfile
import threading

if len(sys.argv) != 3:
    raise SystemExit('usage: probe.py FIXTURE_EXE SYSTEM_RUNTIME_DLL')
root = Path(tempfile.mkdtemp(prefix='webview-transport-fixture-'))
profile = root / 'profile' / 'EBWebView' / 'Default'
profile.mkdir(parents=True)
(profile / 'Preferences').write_text(json.dumps({'fixture': {'preserved': True}, 'webrtc': {'other': 'preserved'}}), encoding='utf8')
process = subprocess.Popen([sys.argv[1], sys.argv[2], str(root / 'profile'), str(root / 'preview.png'), 'interactive'], stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, encoding='utf8', errors='replace', bufsize=1)
lines = queue.Queue()
def read():
    for line in process.stdout:
        lines.put(line.rstrip())
    lines.put(None)
threading.Thread(target=read, daemon=True).start()
results = {}
try:
    while True:
        line = lines.get(timeout=90)
        if line is None:
            break
        if line.startswith(('network_phase=', 'dom=', 'ready=', 'job_samples=')):
            print(line, flush=True)
        if line.startswith('network_phase='):
            phase = int(line.split(' ', 1)[0].split('=')[1])
            results[phase] = json.loads(json.loads(line.split('result=', 1)[1]))
        if line in ('ready=2', 'ready=3'):
            process.stdin.write('\n')
            process.stdin.flush()
    process.wait(timeout=10)
    prefs = json.loads((profile / 'Preferences').read_text(encoding='utf8'))
    online = lambda x: x.get('http') == 'success' and x.get('workerNewHttp') == 'success' and x.get('workerNewWs') == 'echo'
    offline = results.get(2, {})
    blocked = offline.get('http') == 'failed' and offline.get('http6') == 'failed' and offline.get('existingTcp') != 'echo' and offline.get('newTcp') != 'echo' and offline.get('workerExistingHttp') == 'failed' and offline.get('workerNewHttp') == 'failed' and offline.get('workerExistingWs') != 'echo' and offline.get('workerNewWs') != 'echo' and offline.get('srflx4') == 0 and offline.get('srflx6') == 0
    if process.returncode or not online(results.get(1, {})) or not blocked or not online(results.get(3, {})) or prefs.get('fixture', {}).get('preserved') is not True or prefs.get('webrtc', {}).get('other') != 'preserved' or prefs.get('webrtc', {}).get('ip_handling_policy') != 'disable_non_proxied_udp':
        raise SystemExit('FAIL: transport or preference verification failed')
    print('PASS: transport state and preserved preferences', flush=True)
finally:
    if process.poll() is None:
        process.kill()
