"""Verify common SV components and native notifications in an owned host."""
import argparse
import json
from pathlib import Path
import queue
import subprocess
import threading
import time

parser=argparse.ArgumentParser()
parser.add_argument('--client',type=Path,required=True)
parser.add_argument('--endpoint',type=Path,required=True)
parser.add_argument('--require-host-pid',type=int,required=True)
args=parser.parse_args()
deadline=time.monotonic()+20
while True:
    try:
        endpoint=json.loads(args.endpoint.read_text(encoding='utf-8-sig'))
        if endpoint['hostPid']==args.require_host_pid:break
    except (OSError,ValueError):pass
    if time.monotonic()>deadline:raise RuntimeError('Selected native test endpoint was not published')
    time.sleep(.1)
client=subprocess.Popen([str(args.client),'--endpoint',str(args.endpoint)],stdin=subprocess.PIPE,stdout=subprocess.PIPE,stderr=subprocess.DEVNULL,text=True,encoding='utf-8')
responses=queue.Queue()
def receive():
    for line in client.stdout:responses.put(json.loads(line))
threading.Thread(target=receive,daemon=True).start()
sequence=0
def request(method,params):
    global sequence
    sequence+=1;client.stdin.write(json.dumps({'jsonrpc':'2.0','id':sequence,'method':method,'params':params})+'\n');client.stdin.flush()
    value=responses.get(timeout=15)
    assert value['id']==sequence and 'error' not in value
    return value['result']
def tool(name,arguments):
    value=request('tools/call',{'name':'svext_nativeui_'+name,'arguments':arguments})
    body=value['structuredContent'];assert not value['isError'] and body['ok'],body
    return body['result']
try:
    request('initialize',{'protocolVersion':'2025-11-25'})
    client.stdin.write(json.dumps({'jsonrpc':'2.0','method':'notifications/initialized'})+'\n');client.stdin.flush()
    while True:
        state=tool('state',{})
        if state['categoryReady'] and state['rootReady']:break
        if time.monotonic()>deadline:raise RuntimeError('Original host root was not ready')
        time.sleep(.2)
    tool('open',{});time.sleep(.5)
    before=tool('state',{});print(json.dumps(before),flush=True)
    assert len(before['views'])==1 and before['foreignContentWindows']==0
    view=before['views'][0]
    assert view['order'].index('setting/notes') < view['order'].index('row/button/probe')
    assert view['ready'] and view['mountedToPane'] and view['toggleNative'] and view['buttonNative'] and view['toggleVisible']
    assert before['nativeStringRoundTrip'] and view['nativeLabelMeasured']
    assert view['choiceReadable'] and view['selected'] in (1,2) and view['textReadable']
    inputs=tool('inputs',{'selected':2,'text':'Native updated 中文'})
    assert inputs['views'][0]['selected']==2 and inputs['views'][0]['text']=='Native updated 中文'
    tool('set',{'checked':not view['checked']})
    settled=time.monotonic()+3
    while True:
        after=tool('state',{})
        if after['settingsEvents']>before['settingsEvents'] and after['settings']['enabled']!=view['checked']:break
        if time.monotonic()>settled:raise AssertionError(after)
        time.sleep(.1)
    assert after['views'][0]['checked']!=view['checked']
    assert after['settings']['enabled']!=view['checked']
    tool('update',{})
    for _ in range(5):
        tool('refresh',{});time.sleep(.1)
    current=tool('state',{})
    assert current['views'][0]['order']==view['order']
    assert current['views'][0]['ready'] and current['views'][0]['mountedToPane'] and current['views'][0]['toggleVisible']
    assert current['settings']['notes']=='Native updated 中文' and current['settings']['selection']=='Third choice'
    tool('clear',{})
    tool('open',{})
    rebuilt=tool('state',{})
    assert len(rebuilt['views'])==1 and rebuilt['views'][0]['ready']
    print('PASS: common native controls, value read/write, notification, five pane refreshes and destroy/recreate; GUI input remains separate',flush=True)
finally:
    client.stdin.close();client.wait(timeout=5)
