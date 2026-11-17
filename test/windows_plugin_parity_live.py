"""Verify official script engines, permissions and native plugin panels in an owned host."""
import argparse,json,queue,subprocess,threading,time
from pathlib import Path
parser=argparse.ArgumentParser();parser.add_argument('--client',type=Path,required=True);parser.add_argument('--endpoint',type=Path,required=True);parser.add_argument('--pid',type=int,required=True);args=parser.parse_args()
assert json.loads(args.endpoint.read_text(encoding='utf-8-sig'))['hostPid']==args.pid
p=subprocess.Popen([str(args.client),'--endpoint',str(args.endpoint)],stdin=subprocess.PIPE,stdout=subprocess.PIPE,stderr=subprocess.DEVNULL,text=True,encoding='utf-8');replies=queue.Queue()
def receive():
 for line in p.stdout:replies.put(json.loads(line))
threading.Thread(target=receive,daemon=True).start();sequence=0
def request(method,arguments,expect_error=False):
 global sequence
 sequence+=1;p.stdin.write(json.dumps({'jsonrpc':'2.0','id':sequence,'method':method,'params':arguments})+'\n');p.stdin.flush();value=replies.get(timeout=15);assert value['id']==sequence,value
 if expect_error:assert 'error' in value,value;return value
 assert 'error' not in value,value;return value['result']
def tool(operation,arguments=None,allow_error=False):
 value=request('tools/call',{'name':'svext_test_parity_'+operation,'arguments':arguments or {}});body=value['structuredContent']
 if not allow_error:assert not value['isError'] and body['ok'],body
 return body
try:
 request('initialize',{'protocolVersion':'2025-11-25'})
 state=tool('state')['result'];assert state['rootReady'];print('JavaScript gateway signature matched:',state['javascriptReady'],flush=True)
 storage=tool('storage',allow_error=True);print('Scoped storage:',json.dumps(storage),flush=True)
 assert storage['ok'],storage
 for target in ['', 'opensv2.lua-extend','opensv2.javascript-extend','sv2.mcp']:tool('open',{'id':target})
 state=tool('state')['result'];assert all(pane['ready'] for pane in state['panes']);print('Native plugin panes ready:',len(state['panes']),flush=True)
 for language,identifier in [('lua','opensv2.lua-extend'),('javascript','opensv2.javascript-extend')]:
  tool('ui',{'id':identifier,'key':'enabled','value':True});tool('run',{'language':language});deadline=time.monotonic()+12
  while True:
   state=tool('state')['result'];report=state['reports'].get(language)
   if report is not None:break
   if time.monotonic()>deadline:raise AssertionError('No official script report: '+language)
   time.sleep(.1)
  checks=report['checks'];print(language,json.dumps(checks),flush=True)
  assert checks.get('runtimeInfo') and checks.get('capabilities') and checks.get('selectionAdvertised') and checks.get('reply'),checks
  if language=='javascript':assert checks.get('originalEncoding'),checks
 tool('ui',{'id':'opensv2.lua-extend','key':'enabled','value':False});tool('run',{'language':'lua'});deadline=time.monotonic()+12
 while True:
  report=tool('state')['result']['reports'].get('lua')
  if report is not None:break
  if time.monotonic()>deadline:raise AssertionError('No disabled Lua report')
  time.sleep(.1)
 assert report['checks'].get('disabled') and report['checks'].get('disabledSubmit'),report
 value=request('tools/call',{'name':'sv_read_editor','arguments':{}});assert value['structuredContent']['ok'];print('Disabled user Lua preserves internal MCP provider: PASS',flush=True)
 tool('ui',{'id':'opensv2.lua-extend','key':'enabled','value':True})
 tool('ui',{'id':'sv2.mcp','key':'tool.sv_read_editor','value':False});names={t['name'] for t in request('tools/list',{})['tools']};assert 'sv_read_editor' not in names
 denied=request('tools/call',{'name':'sv_read_editor','arguments':{}},expect_error=True);assert denied['error']['code']==-32002
 tool('ui',{'id':'sv2.mcp','key':'tool.sv_read_editor','value':True});assert 'sv_read_editor' in {t['name'] for t in request('tools/list',{})['tools']};print('MCP method filtering and restoration: PASS',flush=True)
finally:
 p.stdin.close();p.wait(timeout=5)
