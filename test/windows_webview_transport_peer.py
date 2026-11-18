import base64
import hashlib
import json
import os
from pathlib import Path
import socket
import subprocess
import sys
import threading
import time
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from urllib.parse import parse_qs, urlparse

if len(sys.argv) != 4:
    raise SystemExit('usage: windows_webview_transport_peer.py FIXTURE RUNTIME_DLL PROFILE')
binary, runtime, profile = map(Path, sys.argv[1:4])
mode = '--transport'
counts = {'http': {}, 'websocket': {}, 'udp': [0, 0, 0]}
lock = threading.Lock()
stop = threading.Event()

class Peer(BaseHTTPRequestHandler):
    protocol_version = 'HTTP/1.1'

    def log_message(self, *_):
        pass

    def do_GET(self):
        request = urlparse(self.path)
        if request.path == '/http':
            phase = parse_qs(request.query).get('phase', [''])[0]
            if phase not in ('1', '2', '3'):
                self.send_error(400)
                return
            with lock:
                counts['http'][phase] = counts['http'].get(phase, 0) + 1
            self.send_response(204)
            self.send_header('Content-Length', '0')
            self.send_header('Access-Control-Allow-Origin', '*')
            self.end_headers()
            return
        if request.path != '/ws' or self.headers.get('Upgrade', '').lower() != 'websocket':
            self.send_error(400)
            return
        key = self.headers.get('Sec-WebSocket-Key', '')
        accept = base64.b64encode(hashlib.sha1((key + '258EAFA5-E914-47DA-95CA-C5AB0DC85B11').encode()).digest()).decode()
        self.send_response(101)
        self.send_header('Upgrade', 'websocket')
        self.send_header('Connection', 'Upgrade')
        self.send_header('Sec-WebSocket-Accept', accept)
        self.end_headers()
        self.connection.settimeout(30)
        while not stop.is_set():
            try:
                header = self.rfile.read(2)
                if len(header) != 2:
                    break
                length = header[1] & 127
                if not header[1] & 128 or length >= 126:
                    break
                mask = self.rfile.read(4)
                payload = self.rfile.read(length)
                if len(mask) != 4 or len(payload) != length:
                    break
                payload = bytes(value ^ mask[i % 4] for i, value in enumerate(payload))
                if header[0] & 15 == 8:
                    break
                if payload not in (b'phase1', b'phase2', b'phase3'):
                    break
                marker = payload.decode()
                with lock:
                    counts['websocket'][marker] = counts['websocket'].get(marker, 0) + 1
                self.connection.sendall(bytes([0x81, len(payload)]) + payload)
            except (OSError, TimeoutError):
                break

server = ThreadingHTTPServer(('127.0.0.1', 0), Peer)
server.daemon_threads = True
threading.Thread(target=server.serve_forever, daemon=True).start()
udp_sockets = []
for phase in range(3):
    udp = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    udp.bind(('127.0.0.1', 0))
    udp.settimeout(.2)
    udp_sockets.append(udp)
    def receive(sock=udp, index=phase):
        while not stop.is_set():
            try:
                data, _ = sock.recvfrom(2048)
                if data:
                    with lock:
                        counts['udp'][index] += 1
            except TimeoutError:
                pass
            except OSError:
                break
    threading.Thread(target=receive, daemon=True).start()
ports = [server.server_port] + [item.getsockname()[1] for item in udp_sockets]
environment = os.environ.copy()
environment['SV2_CDP_LOCAL_PORTS'] = ','.join(map(str, ports))
output = Path(__file__).resolve().parent / '.outputs'
result = None
try:
    result = subprocess.run([str(binary), str(runtime), str(profile), str(output / 'transport-peer-preview.png'), 'interactive'], input='\n\n', capture_output=True, text=True, encoding='utf-8', errors='replace', env=environment, timeout=60)
    time.sleep(.5)
finally:
    stop.set()
    server.shutdown()
    server.server_close()
    for item in udp_sockets:
        item.close()
observations = [line for line in result.stdout.splitlines() if line.startswith(('network_phase=', 'cdp_', 'target_type=', 'existing_targets=', 'session_error', 'dom='))]
phases = {}
for line in observations:
    if line.startswith('network_phase='):
        label, value = line.split(' result=', 1)
        phases[int(label.split('=', 1)[1])] = json.loads(json.loads(value))
checks = {'fixture_completed': result.returncode == 0}
if len(phases) == 3:
    checks.update({
        'baseline_peer': counts['http'].get('1', 0) > 0 and counts['websocket'].get('phase1', 0) >= 2 and counts['udp'][0] == 0,
        'blocked_http_and_udp_peer': counts['http'].get('2', 0) == 0 and counts['udp'][1] == 0,
        'blocked_existing_websocket_peer': counts['websocket'].get('phase2', 0) == 0,
        'blocked_existing_worker': phases[2]['workerExistingHttp'] != 'success' and phases[2]['workerExistingWs'] != 'echo',
        'blocked_new_worker': phases[2]['workerNewHttp'] != 'success' and phases[2]['workerNewWs'] != 'echo',
        'restored_peer': counts['http'].get('3', 0) > 0 and counts['websocket'].get('phase3', 0) >= 1 and counts['udp'][2] == 0,
    })
passed = bool(checks) and all(checks.values())
receipt = {'mode': mode, 'fixture_exit': result.returncode, 'test_passed': passed, 'peer_counts': counts, 'checks': checks, 'observations': observations}
path = output / ('transport-peer-' + mode[2:] + '.json')
path.write_text(json.dumps(receipt, indent=2), encoding='utf-8')
print(json.dumps(receipt, indent=2))
raise SystemExit(0 if passed else 1)
