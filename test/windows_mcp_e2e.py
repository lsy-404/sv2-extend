import ctypes, json, pathlib, subprocess, sys, tempfile, time
from ctypes import wintypes
build=pathlib.Path(sys.argv[1]).resolve();root=pathlib.Path(tempfile.mkdtemp(prefix='sv2-mcp-canary-'));profile=root/'profile';report=profile/'outputs/status.txt'
with (root/'launch.out').open('w') as out:
 launch=subprocess.run([str(build/'InheritLauncher.exe'),str(build/'network-host/McpFixture.exe'),str(profile),str(build/'ExtendCore.dll'),str(report)],stdout=out,timeout=30)
if launch.returncode: raise RuntimeError(f'launcher exit {launch.returncode}')
name,pid=(root/'launch.out').read_text().splitlines();pid=int(pid);kernel=ctypes.WinDLL('kernel32',use_last_error=True);kernel.OpenProcess.restype=wintypes.HANDLE;process=kernel.OpenProcess(0x100001,False,pid)
if not process: raise ctypes.WinError(ctypes.get_last_error())
try:
 deadline=time.monotonic()+10
 while not report.exists() and time.monotonic()<deadline: time.sleep(.05)
 if not report.exists() or report.read_text()!='0': raise RuntimeError('plugin initialization failed')
 panel=pathlib.Path(str(report)+'.panel');update=pathlib.Path(str(panel)+'.update')
 deadline=time.monotonic()+5
 while (not update.exists() or 'Editor: ready' not in update.read_text()) and time.monotonic()<deadline: time.sleep(.05)
 descriptor=json.loads(panel.read_text());state=json.loads(update.read_text())
 assert descriptor['id']=='sv2.mcp' and descriptor['name']=='MCP' and descriptor['settings']==[]
 assert state['status']=='Listening' and any(row['text']=='Editor: ready' for row in state['sidebar']['rows'])
 rows=state['sidebar']['rows'];command=''.join(row['text'] for row in rows[2:-1])
 assert command==f'"{build / "Sv2Mcp.exe"}" --endpoint "{profile / "outputs/mcp-endpoint.json"}"'
 count=pathlib.Path(str(panel)+'.count');assert count.read_text()=='1'
 pathlib.Path(str(panel)+'.refresh').write_text('refresh');deadline=time.monotonic()+5
 while count.read_text()!='2' and time.monotonic()<deadline: time.sleep(.05)
 assert count.read_text()=='2'
 requests=[{'jsonrpc':'2.0','id':1,'method':'initialize','params':{'protocolVersion':'2025-06-18'}},{'jsonrpc':'2.0','method':'notifications/initialized'},{'jsonrpc':'2.0','id':2,'method':'tools/list'},{'jsonrpc':'2.0','id':3,'method':'tools/call','params':{'name':'read','arguments':{}}}]
 result=subprocess.run([str(build/'Sv2Mcp.exe'),'--endpoint',str(profile/'outputs/mcp-endpoint.json')],input=''.join(json.dumps(x)+'\n' for x in requests),text=True,capture_output=True,timeout=15)
 if result.returncode: raise RuntimeError(result.stderr)
 responses=[json.loads(x) for x in result.stdout.splitlines()];assert [r['id'] for r in responses]==[1,2,3];assert responses[1]['result']['tools'][0]['name']=='read';assert responses[2]['result']['structuredContent']['result']['value']==42
 print('MCP initialize, notification, tools/list and tools/call passed')
finally:
 if kernel.WaitForSingleObject(wintypes.HANDLE(process),1000)==0x102: kernel.TerminateProcess(wintypes.HANDLE(process),1);kernel.WaitForSingleObject(wintypes.HANDLE(process),5000)
 kernel.CloseHandle(wintypes.HANDLE(process))
