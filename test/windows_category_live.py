"""Exercise test-only native category instrumentation in an isolated host."""
import argparse
import json
from pathlib import Path
import queue
import subprocess
import threading
import time

parser = argparse.ArgumentParser()
parser.add_argument('--client', type=Path, required=True)
parser.add_argument('--endpoint', type=Path, required=True)
parser.add_argument('--require-host-pid', type=int, required=True)
args = parser.parse_args()
deadline = time.monotonic() + 10
endpoint = None
while time.monotonic() < deadline:
    try:
        candidate = json.loads(args.endpoint.read_text(encoding='utf-8-sig'))
        if candidate.get('hostPid') == args.require_host_pid:
            endpoint = candidate
            break
    except (OSError, ValueError):
        pass
    time.sleep(0.1)
if endpoint is None:
    raise RuntimeError('The selected test host endpoint was not published')
client = subprocess.Popen([str(args.client), '--endpoint', str(args.endpoint)], stdin=subprocess.PIPE,
                          stdout=subprocess.PIPE, stderr=subprocess.DEVNULL, text=True, encoding='utf-8')
responses = queue.Queue()

def receive():
    try:
        for line in client.stdout:
            responses.put(json.loads(line))
    finally:
        responses.put(None)

threading.Thread(target=receive, daemon=True).start()
sequence = 0

def request(method, params):
    global sequence
    sequence += 1
    client.stdin.write(json.dumps({'jsonrpc': '2.0', 'id': sequence, 'method': method, 'params': params}) + '\n')
    client.stdin.flush()
    value = responses.get(timeout=15)
    if value is None or value.get('id') != sequence or 'error' in value:
        raise RuntimeError('Test MCP request failed')
    return value['result']

def tool(name, arguments):
    response = request('tools/call', {'name': name, 'arguments': arguments})
    body = response['structuredContent']
    if response['isError'] or not body.get('ok'):
        if name == 'svext_test_category_change':
            try:
                diagnostic = request('tools/call', {'name': 'svext_test_category_state', 'arguments': {}})
                snapshot = diagnostic['structuredContent']
                if snapshot.get('ok'):
                    print('Category guard state:', json.dumps(snapshot['result'], ensure_ascii=False))
            except Exception:
                print('Category guard state unavailable')
        raise RuntimeError('Test operation rejected: ' + str(body.get('error', {}).get('code', 'UNKNOWN')))
    return body['result']

def state():
    return tool('svext_test_category_state', {})

def pane(current, identifier):
    found = [item for item in current['panes'] if item['id'] == identifier]
    if len(found) != 1 or found[0]['width'] <= 0 or found[0]['height'] <= 0:
        raise AssertionError('Native pane missing or ambiguous: ' + identifier)
    return found[0]

opened = []
resized = False
try:
    request('initialize', {'protocolVersion': '2025-11-25'})
    client.stdin.write(json.dumps({'jsonrpc': '2.0', 'method': 'notifications/initialized'}) + '\n')
    client.stdin.flush()
    required = {'svext_test_category_state', 'svext_test_category_change', 'svext_test_category_pause',
                'svext_test_category_control', 'svext_test_category_resize', 'sv_read_editor',
                'sv_create_group', 'sv_remove_group', 'sv_add_note', 'sv_remove_note',
                'sv_set_note_pitch', 'sv_read_pitch_range', 'sv_set_pitch_curve'}
    deadline = time.monotonic() + 10
    while True:
        tools = {entry['name'] for entry in request('tools/list', {})['tools']}
        if required <= tools:
            break
        if time.monotonic() >= deadline:
            raise RuntimeError('Required test and editor tools were not registered')
        time.sleep(0.2)
    before = state()
    print('Host geometry:', json.dumps({key: before[key] for key in ('hostClientWidth', 'hostClientHeight', 'hostDpi', 'guards')}, ensure_ascii=False))
    assert before['nativePrepared'] and before['editorReady']
    assert {'', 'boxy.sv2', 'sv2.mcp', 'test.category.fixture'} <= {entry['id'] for entry in before['categories']}
    tool('sv_read_editor', {})
    for identifier in ('', 'boxy.sv2', 'sv2.mcp', 'test.category.fixture'):
        tool('svext_test_category_change', {'id': identifier, 'open': True})
        opened.append(identifier)
        time.sleep(0.5)
        current = state()
        category = next(item for item in current['categories'] if item['id'] == identifier)
        assert category['openKnown'] and category['open']
        print('Pane geometry:', json.dumps(pane(current, identifier), ensure_ascii=False))
    all_open = state()
    for identifier in opened:
        category = next(item for item in all_open['categories'] if item['id'] == identifier)
        assert category['openKnown'] and category['open'] and pane(all_open, identifier)['visible']
    for identifier in ('', 'boxy.sv2', 'sv2.mcp'):
        tool('svext_test_category_change', {'id': identifier, 'open': False})
        opened.remove(identifier)
        time.sleep(0.5)
        current = state()
        category = next(item for item in current['categories'] if item['id'] == identifier)
        assert category['openKnown'] and not category['open']
        assert all(not item['visible'] for item in current['panes'] if item['id'] == identifier)
    fixture_before = state()
    assert pane(fixture_before, 'test.category.fixture')['visible']
    tool('svext_test_category_control', {'kind': 'action'})
    tool('svext_test_category_control', {'kind': 'boolean'})
    tool('svext_test_category_control', {'kind': 'unchanged'})
    after = state()
    assert after['actionEvents'] == fixture_before['actionEvents'] + 1
    assert after['settingsEvents'] == fixture_before['settingsEvents'] + 1
    tool('svext_test_category_resize', {'restore': False})
    resized = True
    time.sleep(0.5)
    tool('svext_test_category_change', {'id': 'test.category.fixture', 'open': True})
    time.sleep(0.5)
    changed = state()
    old_pane = pane(fixture_before, 'test.category.fixture')
    new_pane = pane(changed, 'test.category.fixture')
    assert (old_pane['width'], old_pane['height']) != (new_pane['width'], new_pane['height'])
    tool('svext_test_category_resize', {'restore': True})
    resized = False
    tool('svext_test_category_change', {'id': 'test.category.fixture', 'open': False})
    opened.remove('test.category.fixture')
    time.sleep(0.5)
    close_state = state()
    category = next(item for item in close_state['categories'] if item['id'] == 'test.category.fixture')
    assert category['openKnown'] and not category['open']
    closed = [item for item in close_state['panes'] if item['id'] == 'test.category.fixture']
    assert not closed or all(not item['visible'] for item in closed)
    tool('svext_test_category_pause', {})
    time.sleep(1.5)
    assert state()['editorReady']
    tool('sv_read_editor', {})
    print('PASS: native category lifecycle, controls, resize and editor heartbeat recovery')
finally:
    if resized:
        try:
            tool('svext_test_category_resize', {'restore': True})
        except Exception:
            print('FAIL: test window restore failed')
    for identifier in reversed(opened):
        try:
            tool('svext_test_category_change', {'id': identifier, 'open': False})
        except Exception:
            print('FAIL: test category close failed: ' + identifier)
    client.stdin.close()
    client.terminate()
    client.wait(timeout=5)
