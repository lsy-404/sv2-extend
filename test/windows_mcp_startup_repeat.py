"""Restart an owned empty SV2 fixture and verify its actual MCP endpoint."""
import argparse
import ctypes
import ctypes.wintypes as wt
import json
from pathlib import Path
import queue
import subprocess
import threading
import time

parser = argparse.ArgumentParser()
for name in ('launcher', 'host', 'profile', 'client'):
    parser.add_argument('--' + name, type=Path, required=True)
args = parser.parse_args()
kernel = ctypes.WinDLL('kernel32', use_last_error=True)
kernel.OpenProcess.argtypes = [wt.DWORD, wt.BOOL, wt.DWORD]
kernel.OpenProcess.restype = wt.HANDLE
kernel.QueryFullProcessImageNameW.argtypes = [wt.HANDLE, wt.DWORD, wt.LPWSTR, ctypes.POINTER(wt.DWORD)]
kernel.GetExitCodeProcess.argtypes = [wt.HANDLE, ctypes.POINTER(wt.DWORD)]
kernel.TerminateProcess.argtypes = [wt.HANDLE, wt.UINT]
kernel.WaitForSingleObject.argtypes = [wt.HANDLE, wt.DWORD]
kernel.CloseHandle.argtypes = [wt.HANDLE]
expected = args.profile / 'runtime' / args.host.parent.name / args.host.name
endpoint = args.profile / 'outputs' / 'mcp-endpoint.json'
abort_file = args.profile / 'outputs' / 'abort-frames.json'

for attempt in range(1, 4):
    result = subprocess.run([str(args.launcher), '--host', str(args.host), str(args.profile)],
                            capture_output=True, text=True, encoding='utf-8', timeout=90, check=True)
    launch = json.loads(result.stdout)
    assert launch['residentCore'] and launch['appContainer'] and not launch['fixtureOnly']
    pid = launch['hostPid']
    process = kernel.OpenProcess(0x100001 | 0x1000, False, pid)
    assert process
    path, size = ctypes.create_unicode_buffer(32768), wt.DWORD(32768)
    assert kernel.QueryFullProcessImageNameW(process, 0, path, ctypes.byref(size))
    assert Path(path.value).resolve() == expected.resolve(), 'Unexpected host path'
    client = None
    verified = False
    try:
        deadline = time.monotonic() + 15
        while True:
            status = wt.DWORD()
            assert kernel.GetExitCodeProcess(process, ctypes.byref(status)) and status.value == 259, 'Host exited'
            try:
                published = json.loads(endpoint.read_text(encoding='utf-8-sig'))
                if published['hostPid'] == pid:
                    break
            except (OSError, ValueError, KeyError):
                pass
            assert time.monotonic() < deadline, 'No current endpoint'
            time.sleep(.1)
        client = subprocess.Popen([str(args.client), '--endpoint', str(endpoint)], stdin=subprocess.PIPE,
                                  stdout=subprocess.PIPE, stderr=subprocess.DEVNULL, text=True, encoding='utf-8')
        responses = queue.Queue()

        def receive():
            for line in client.stdout:
                responses.put(json.loads(line))
            responses.put(None)

        reader = threading.Thread(target=receive, daemon=True)
        reader.start()
        sequence = 0

        def request(method, params):
            global sequence
            sequence += 1
            client.stdin.write(json.dumps({'jsonrpc': '2.0', 'id': sequence, 'method': method, 'params': params}) + '\n')
            client.stdin.flush()
            reply = responses.get(timeout=12)
            assert reply and reply.get('id') == sequence and 'error' not in reply
            return reply['result']

        request('initialize', {'protocolVersion': '2025-11-25'})
        client.stdin.write(json.dumps({'jsonrpc': '2.0', 'method': 'notifications/initialized'}) + '\n')
        client.stdin.flush()
        while True:
            tools = request('tools/list', {})['tools']
            if any(tool['name'] == 'sv_read_editor' for tool in tools):
                break
            assert time.monotonic() < deadline, 'Editor did not become ready'
            time.sleep(.1)
        editor = request('tools/call', {'name': 'sv_read_editor', 'arguments': {}})['structuredContent']
        assert editor['ok'] and editor['result']['noteCount'] == 0
        assert json.loads(editor['result']['guard'])[1] == '', 'Only an unsaved fixture may be stopped'
        if abort_file.exists():
            assert json.loads(abort_file.read_text())['hostPid'] != pid, 'Current host recorded an abort'
        verified = True
        print(json.dumps({'attempt': attempt, 'hostPid': pid, 'tools': len(tools), 'editorRead': True, 'abort': False}), flush=True)
    finally:
        if client:
            client.stdin.close()
            client.wait(timeout=5)
            reader.join(timeout=5)
        if verified:
            assert kernel.TerminateProcess(process, 0)
            assert kernel.WaitForSingleObject(process, 5000) == 0
        kernel.CloseHandle(process)
