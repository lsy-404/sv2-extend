import http.client,json,os,pathlib,subprocess,time,urllib.parse
root=pathlib.Path(__file__).resolve().parent
scratch=pathlib.Path.cwd()/'work/lua-extend-validation/extension-registry';scratch.mkdir(parents=True,exist_ok=True)
env=os.environ.copy();env.update(DYLD_INSERT_LIBRARIES=str(root/'loader-probe.dylib'),OPEN_SV2_MCP_PLUGIN=str(root/'sv2-mcp.dylib'),LOADER_PROBE_PLUGIN=str(root/'build/capability-plugin-fixture.dylib'),OPEN_SV2_LOADER_DATA_DIR=str(scratch),LOADER_PROBE_BLOCK_HOST_AT_START='1')
with (scratch/'stdout.log').open('wb') as out,(scratch/'stderr.log').open('wb') as err:
 p=subprocess.Popen([os.environ['LOADER_PROBE_DIRECT_HOST']],env=env,stdout=out,stderr=err)
 try:
  path=scratch/f'endpoints/sv2-mcp-{p.pid}.json';deadline=time.monotonic()+10
  while p.poll() is None and not path.exists() and time.monotonic()<deadline:time.sleep(.05)
  descriptor=json.loads(path.read_text());port=urllib.parse.urlsplit(descriptor['url']).port
  def rpc(method,params):
   c=http.client.HTTPConnection('127.0.0.1',port,timeout=10);c.request('POST','/mcp',json.dumps({'jsonrpc':'2.0','id':1,'method':method,'params':params}),{'Content-Type':'application/json','Authorization':'Bearer '+descriptor['token']});r=c.getresponse();body=json.loads(r.read());c.close();assert r.status==200;return body
  def tool(name,args):return rpc('tools/call',{'name':name,'arguments':args})
  def names():return [v['name'] for v in rpc('tools/list',{})['result']['tools']]
  checks={};listing=names();checks['native-extension-tools-listed']=set(listing)=={'sv_status','svext_fixture_echo','svext_fixture_remove','svext_fixture_http_get'}
  echo=tool('svext_fixture_echo',{'unicode':'声库','value':60,'nested':{'boolean':True}})['result']['structuredContent']['result'];checks['native-extension-main-thread']=echo['mainThread'] is True;checks['native-extension-resident-pid']=echo['hostPid']==p.pid;checks['typed-arguments-roundtrip']=echo['echo']=={'unicode':'声库','value':60,'nested':{'boolean':True}}
  generation=tool('sv_status',{})['result']['structuredContent']['capabilityGeneration'];removed=tool('svext_fixture_remove',{});checks['remove-operation-completed']=removed['result']['isError'] is False
  deadline=time.monotonic()+2
  while time.monotonic()<deadline and len(names())!=1:time.sleep(.05)
  checks['removed-tools-no-longer-advertised']=names()==['sv_status'];checks['removed-tool-rejected']=tool('svext_fixture_echo',{})['error']['code']==-32602
  status=tool('sv_status',{})['result']['structuredContent'];checks['generation-increased']=status['capabilityGeneration']>generation;checks['no-provider-not-ready']=status['editorReady'] is False
  checks['private-tool-absent']=tool('account.switch',{})['error']['code']==-32602
  report={'host_pid':p.pid,'test_only_plugin':True,'checks':checks,'passed':all(checks.values())};(root/'evidence/extension-registry-runtime.json').write_text(json.dumps(report,indent=2));print(json.dumps(report,indent=2));assert report['passed']
 finally:
  if p.poll() is None:
   p.terminate()
   try:p.wait(timeout=5)
   except subprocess.TimeoutExpired:p.kill();p.wait(timeout=5)
