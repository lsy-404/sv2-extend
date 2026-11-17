"""Exercise a resident owned fixture and the external stdio client."""
import ctypes
from ctypes import wintypes
import json
import os
from pathlib import Path
import subprocess
import sys
import time
import uuid

build=Path(sys.argv[1]).resolve()
root=Path(__file__).resolve().parent/'.outputs'/('mcp-fulltrust-'+uuid.uuid4().hex)
root.mkdir(parents=True)
profile=root/'profile';report=profile/'outputs/status.txt';endpoint=profile/'outputs/mcp-endpoint.json'
with (root/'launcher.out').open('w') as out,(root/'launcher.err').open('w') as err:
    launch=subprocess.run([str(build/'ExtendLauncher.exe'),'--managed-fixture',str(build/'network-host/McpFixture.exe'),str(profile),str(report),'unused'],stdout=out,stderr=err,timeout=20)
assert launch.returncode==0, 'fixture launch failed'
receipt=json.loads((root/'launcher.out').read_text());assert receipt['residentCore'] and not receipt['appContainer']
for _ in range(100):
    if endpoint.exists() and report.exists(): break
    time.sleep(.05)
assert endpoint.exists() and report.read_text()=='0'
value=json.loads(endpoint.read_text());assert value['hostPid']==receipt['hostPid'] and isinstance(value['hostStartTime'],int)
client=build/'Sv2Mcp.exe'
def invoke(request=None):
    return subprocess.run([str(client),'--endpoint',str(endpoint)],input='' if request is None else ''.join(json.dumps(item)+'\n' for item in (request if isinstance(request,list) else [request])),text=True,capture_output=True,timeout=10)
def set_value(item): endpoint.write_text(json.dumps(item),encoding='utf-8')
request={'jsonrpc':'2.0','id':1,'method':'initialize','params':{'protocolVersion':'2025-06-18'}}
requests=[request,{'jsonrpc':'2.0','method':'notifications/initialized'},{'jsonrpc':'2.0','id':2,'method':'tools/list'},{'jsonrpc':'2.0','id':3,'method':'tools/call','params':{'name':'read','arguments':{}}}]
result=invoke(requests);responses=[json.loads(line) for line in result.stdout.splitlines()];assert result.returncode==0 and [item['id'] for item in responses]==[1,2,3] and responses[2]['result']['structuredContent']['result']['value']==42
malformed=dict(value);malformed['schema']=[];set_value(malformed);result=invoke();assert result.returncode!=0 and 'Invalid or stale' in result.stderr
stale=dict(value);stale['hostStartTime']+=1;set_value(stale);result=invoke();assert result.returncode!=0 and 'Invalid or stale' in result.stderr
kernel=ctypes.WinDLL('kernel32',use_last_error=True)
class FileTime(ctypes.Structure): _fields_=[('low',wintypes.DWORD),('high',wintypes.DWORD)]
times=[FileTime() for _ in range(4)];kernel.GetCurrentProcess.restype=wintypes.HANDLE;kernel.GetProcessTimes.argtypes=[wintypes.HANDLE]+[ctypes.POINTER(FileTime)]*4
assert kernel.GetProcessTimes(kernel.GetCurrentProcess(),*[ctypes.byref(t) for t in times])
forged=dict(value);forged['hostPid']=os.getpid();forged['hostStartTime']=(times[0].high<<32)|times[0].low;set_value(forged)
result=invoke();assert result.returncode!=0 and 'Invalid MCP server process' in result.stderr
set_value(value)
permission=subprocess.run(['icacls',str(endpoint),'/grant','*S-1-1-0:(R)'],capture_output=True,text=True,timeout=10);assert permission.returncode==0
result=invoke();assert result.returncode!=0 and 'Invalid or stale' in result.stderr
permission=subprocess.run(['icacls',str(endpoint),'/remove','*S-1-1-0'],capture_output=True,text=True,timeout=10);assert permission.returncode==0
foreign=Path(os.environ['WINDIR'])/'System32'/'drivers'/'etc'/'hosts'
adv=ctypes.WinDLL('advapi32',use_last_error=True)
kernel.CreateFileW.restype=wintypes.HANDLE;kernel.CreateFileW.argtypes=[wintypes.LPCWSTR,wintypes.DWORD,wintypes.DWORD,wintypes.LPVOID,wintypes.DWORD,wintypes.DWORD,wintypes.HANDLE]
file=kernel.CreateFileW(str(foreign),0x20000,7,None,3,0x00200000,None);assert file!=wintypes.HANDLE(-1).value
owner=wintypes.LPVOID();descriptor=wintypes.LPVOID();adv.GetSecurityInfo.argtypes=[wintypes.HANDLE,wintypes.DWORD,wintypes.DWORD,ctypes.POINTER(wintypes.LPVOID),wintypes.LPVOID,wintypes.LPVOID,wintypes.LPVOID,ctypes.POINTER(wintypes.LPVOID)]
assert adv.GetSecurityInfo(file,1,1,ctypes.byref(owner),None,None,None,ctypes.byref(descriptor))==0
adv.OpenProcessToken.argtypes=[wintypes.HANDLE,wintypes.DWORD,ctypes.POINTER(wintypes.HANDLE)];token=wintypes.HANDLE();assert adv.OpenProcessToken(kernel.GetCurrentProcess(),8,ctypes.byref(token))
size=wintypes.DWORD();adv.GetTokenInformation(token,1,None,0,ctypes.byref(size));data=ctypes.create_string_buffer(size.value);assert adv.GetTokenInformation(token,1,data,size,ctypes.byref(size))
user=ctypes.cast(data,ctypes.POINTER(wintypes.LPVOID))[0];adv.EqualSid.argtypes=[wintypes.LPVOID,wintypes.LPVOID];assert not adv.EqualSid(owner,user)
result=subprocess.run([str(client),'--endpoint',str(foreign)],input='',text=True,capture_output=True,timeout=10);assert result.returncode!=0 and 'Invalid or stale' in result.stderr
kernel.CloseHandle.argtypes=[wintypes.HANDLE];kernel.CloseHandle(token);kernel.CloseHandle(file);kernel.LocalFree.argtypes=[wintypes.LPVOID];kernel.LocalFree(descriptor)
kernel.OpenProcess.argtypes=[wintypes.DWORD,wintypes.BOOL,wintypes.DWORD];kernel.OpenProcess.restype=wintypes.HANDLE;process=kernel.OpenProcess(0x100000,False,receipt['hostPid']);assert process and kernel.WaitForSingleObject(wintypes.HANDLE(process),10000)==0;kernel.CloseHandle(process)
result=invoke();assert result.returncode!=0 and 'Invalid or stale' in result.stderr
print('Host-owned pipe, external initialize, stale process, wrong server PID, foreign owner and foreign allowed SID rejected')
