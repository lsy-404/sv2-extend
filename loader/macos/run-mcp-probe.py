import concurrent.futures,http.client,json,os,pathlib,socket,subprocess,time,urllib.parse
root=pathlib.Path(__file__).resolve().parent
scratch=pathlib.Path.cwd()/'work/macos-loader-probe/mcp-validation';scratch.mkdir(parents=True,exist_ok=True)
env=os.environ.copy();env.update(DYLD_INSERT_LIBRARIES=str(root/'loader-probe.dylib'),OPEN_SV2_MCP_PLUGIN=str(root/'sv2-mcp.dylib'),OPEN_SV2_LOADER_DATA_DIR=str(scratch),LOADER_PROBE_BLOCK_HOST_AT_START='1');env.pop('LOADER_PROBE_PLUGIN',None)
with (root/'mcp.stdout').open('wb') as out,(root/'mcp.stderr').open('wb') as err:
 process=subprocess.Popen([os.environ['LOADER_PROBE_DIRECT_HOST']],env=env,stdout=out,stderr=err)
 try:
  descriptor=scratch/f'endpoints/sv2-mcp-{process.pid}.json';deadline=time.monotonic()+10
  while process.poll() is None and not descriptor.exists() and time.monotonic()<deadline:time.sleep(.1)
  if not descriptor.exists():raise RuntimeError('Resident MCP endpoint did not start; inspect mcp.stderr')
  endpoint=json.loads(descriptor.read_text());url=urllib.parse.urlsplit(endpoint['url'])
  def request(payload,extra=None,auth=True):
   client=http.client.HTTPConnection('127.0.0.1',url.port,timeout=12)
   headers={'Content-Type':'application/json','Mcp-Protocol-Version':'2025-11-25'}
   if auth:headers['Authorization']='Bearer '+endpoint['token']
   if extra:headers.update(extra)
   body=json.dumps(payload);client.request('POST','/mcp',body,headers);response=client.getresponse();data=response.read();status=response.status;client.close()
   return status,json.loads(data) if data else None
  def rpc(identifier,method,params=None):
   message={'jsonrpc':'2.0','id':identifier,'method':method}
   if params is not None:message['params']=params
   return request(message)
  checks={}
  status,initial=rpc(1,'initialize',{'protocolVersion':'2025-11-25','capabilities':{},'clientInfo':{'name':'loader-native-audit','version':'1'}});checks['initialize']=status==200 and initial['result']['serverInfo']['name']=='sv2-mcp'
  status,_=request({'jsonrpc':'2.0','method':'notifications/initialized'});checks['notification-accepted']=status==202
  status,listing=rpc(2,'tools/list',{});names=[tool['name'] for tool in listing['result']['tools']];checks['only-real-capabilities-advertised']=names==['sv_status']
  status,value=rpc(3,'tools/call',{'name':'sv_status','arguments':{}});live=value['result']['structuredContent'];checks['resident-pid']=live['hostPid']==process.pid;checks['no-product-account-required']=live['productAccountRequired'] is False;checks['native-adapter-unavailable-reported']=live['editorReady'] is False
  status,unsupported=rpc(4,'tools/call',{'name':'sv_set_note_pitch','arguments':{'noteIndex':1,'midiPitch':60,'guard':'not-a-real-guard'}});checks['unbacked-pitch-rejected']=unsupported['result']['isError'] is True and unsupported['result']['structuredContent']['error']['code']=='EDITOR_ADAPTER_UNAVAILABLE'
  status,private=rpc(5,'tools/call',{'name':'account.switch','arguments':{}});checks['account-tool-not-exposed']=private.get('error',{}).get('code')==-32602
  checks['no-token-rejected']=request({'jsonrpc':'2.0','id':6,'method':'ping'},auth=False)[0]==403
  checks['foreign-origin-rejected']=request({'jsonrpc':'2.0','id':7,'method':'ping'},{'Origin':'https://example.invalid'})[0]==403
  checks['wrong-host-rejected']=request({'jsonrpc':'2.0','id':8,'method':'ping'},{'Host':'example.invalid'})[0]==403
  checks['unsupported-protocol-rejected']=request({'jsonrpc':'2.0','id':9,'method':'ping'},{'Mcp-Protocol-Version':'bogus'})[0]==400
  checks['invalid-params-rejected']=rpc(10,'initialize',[])[1].get('error',{}).get('code')==-32602
  checks['boolean-id-rejected']=rpc(True,'ping')[1].get('error',{}).get('code')==-32600
  with concurrent.futures.ThreadPoolExecutor(max_workers=8) as pool:
   values=list(pool.map(lambda i:rpc(100+i,'tools/call',{'name':'sv_status','arguments':{}}),range(16)))
  checks['concurrent-requests']=all(code==200 and response['result']['structuredContent']['hostPid']==process.pid for code,response in values)
  # Disconnect before reading to verify a local client cannot kill the host through SIGPIPE.
  peer=socket.create_connection(('127.0.0.1',url.port));body=json.dumps({'jsonrpc':'2.0','id':999,'method':'ping'}).encode();headers=(f'POST /mcp HTTP/1.1\r\nHost: 127.0.0.1:{url.port}\r\nAuthorization: Bearer {endpoint["token"]}\r\nContent-Type: application/json\r\nContent-Length: {len(body)}\r\n\r\n').encode();peer.sendall(headers+body);peer.close();time.sleep(.2)
  checks['disconnect-does-not-kill-host']=process.poll() is None
  checks['host-network-blocked-during-test']='NETWORK_GATE pid='+str(process.pid)+' blocked=1' in (root/'mcp.stderr').read_text(errors='replace')
  children=subprocess.run(['pgrep','-P',str(process.pid)],capture_output=True,text=True).stdout.split();checks['no-manager-child-process']=not children
  report={'host_pid':process.pid,'advertised_tools':names,'checks':checks,'passed':all(checks.values()),'product_account_required':False,'native_editor_adapter_ready':False,'credentials_exported':False}
  (root/'mcp-runtime-results.json').write_text(json.dumps(report,indent=2));print(json.dumps(report,indent=2))
 finally:
  if process.poll() is None:
   process.terminate()
   try:process.wait(timeout=5)
   except subprocess.TimeoutExpired:process.kill();process.wait(timeout=5)
