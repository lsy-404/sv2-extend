"""Exercise note editing through MCP in an explicitly selected, empty test host."""
import argparse
import json
import pathlib
import queue
import subprocess
import threading

parser = argparse.ArgumentParser()
parser.add_argument('--client', type=pathlib.Path, required=True)
parser.add_argument('--endpoint', type=pathlib.Path, required=True)
parser.add_argument('--require-host-pid', type=int, required=True)
args = parser.parse_args()
endpoint = json.loads(args.endpoint.read_text(encoding='utf-8-sig'))
if endpoint['hostPid'] != args.require_host_pid:
    raise RuntimeError('The selected test host changed')
client = subprocess.Popen([str(args.client), '--endpoint', str(args.endpoint)],
                          stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                          stderr=subprocess.DEVNULL, text=True, encoding='utf-8')
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
    client.stdin.write(json.dumps({'jsonrpc': '2.0', 'id': sequence,
                                   'method': method, 'params': params}) + '\n')
    client.stdin.flush()
    response = responses.get(timeout=12)
    if response is None or response.get('id') != sequence or 'error' in response:
        raise RuntimeError('MCP request failed')
    return response['result']


def tool(name, arguments, expect_error=False):
    response = request('tools/call', {'name': name, 'arguments': arguments})
    body = response['structuredContent']
    if expect_error:
        assert response['isError'] and not body['ok']
        return body
    if response['isError'] or not body['ok']:
        raise RuntimeError('The editor rejected the test operation: ' + json.dumps(body.get('error', {})))
    return body['result']


added = False
created_group = None
try:
    request('initialize', {'protocolVersion': '2025-11-25'})
    client.stdin.write(json.dumps({'jsonrpc': '2.0', 'method': 'notifications/initialized'}) + '\n')
    client.stdin.flush()
    tools = {item['name'] for item in request('tools/list', {})['tools']}
    assert {'svext_read_editor', 'svext_add_note', 'svext_remove_note',
            'svext_set_note_pitch', 'svext_create_group', 'svext_remove_group',
            'svext_read_pitch_range', 'svext_set_pitch_curve'} <= tools, 'The real editor provider is not ready'
    initial = tool('svext_read_editor', {})
    context = json.loads(initial['guard'])
    assert initial['noteCount'] == 0 and context[1] == '', 'An empty unsaved test group is required'
    rejected = tool('svext_add_note', {'guard': initial['guard'], 'onsetBlick': 0,
                                     'durationBlick': 705600000, 'midiPitch': 60}, expect_error=True)
    assert rejected['error']['code'] == 'MAIN_GROUP_EDIT_UNSUPPORTED'
    created_group = tool('svext_create_group', {'guard': initial['guard'], 'name': 'MCP verification'})['groupUUID']
    initial = tool('svext_read_editor', {'groupUUID': created_group})
    assert not initial['isMain'] and initial['noteCount'] == 0
    group = created_group
    added_note = tool('svext_add_note', {'groupUUID': group, 'guard': initial['guard'], 'onsetBlick': 0,
                                       'durationBlick': 705600000, 'midiPitch': 60, 'lyrics': 'la'})
    added = True
    after = tool('svext_read_editor', {'groupUUID': group})
    assert after['groupUUID'] == group and after['noteCount'] == 1
    assert after['notes'][0]['midiPitch'] == 60
    stale = tool('svext_set_note_pitch', {'groupUUID': group, 'guard': initial['guard'], 'noteIndex': 1,
                                        'midiPitch': 62}, expect_error=True)
    assert 'STALE_EDIT_GUARD' in json.dumps(stale)
    tool('svext_set_note_pitch', {'groupUUID': group, 'guard': after['guard'], 'noteIndex': 1, 'midiPitch': 62})
    changed = tool('svext_read_editor', {'groupUUID': group})
    assert changed['groupUUID'] == group and changed['notes'][0]['midiPitch'] == 62
    curve = tool('svext_read_pitch_range', {'groupUUID': group, 'startBlick': 0, 'endBlick': 705600000})
    points = [[0, 0], [352800000, 25]]
    tool('svext_set_pitch_curve', {'groupUUID': group, 'startBlick': 0, 'endBlick': 705600000,
                                 'guard': curve['guard'], 'curveGuard': curve['curveGuard'], 'points': points})
    assert tool('svext_read_pitch_range', {'groupUUID': group, 'startBlick': 0, 'endBlick': 705600000})['points'] == points
    print('PASS: real MCP note creation, readback, stale rejection and pitch edit')
finally:
    try:
        if added:
            current = tool('svext_read_editor', {'groupUUID': group})
            assert current['groupUUID'] == group and current['noteCount'] == 1
            assert current['notes'][0]['onsetBlick'] == 0
            assert current['notes'][0]['durationBlick'] == 705600000
            tool('svext_remove_note', {'groupUUID': group, 'guard': current['guard'], 'noteIndex': 1})
            assert tool('svext_read_editor', {'groupUUID': group})['noteCount'] == 0
            print('PASS: the temporary note was removed')
        if created_group:
            current = tool('svext_read_editor', {'groupUUID': created_group})
            assert current['noteCount'] == 0
            tool('svext_remove_group', {'groupUUID': created_group, 'guard': current['guard']})
            assert tool('svext_read_editor', {})['noteCount'] == 0
            print('PASS: the temporary group was removed')
    finally:
        client.stdin.close()
        try:
            client.wait(timeout=5)
        except subprocess.TimeoutExpired:
            client.terminate()
            client.wait(timeout=5)
