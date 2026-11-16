import base64,http.client,http.server,json,os,pathlib,ssl,subprocess,tempfile,threading,time,urllib.parse
root=pathlib.Path(__file__).resolve().parent;scratch=pathlib.Path.cwd()/'work/lua-extend-validation/script-http';scratch.mkdir(parents=True,exist_ok=True)
seen=[]
class Handler(http.server.BaseHTTPRequestHandler):
 def log_message(self,*args):pass
 def do_GET(self):
  seen.append(self.path);status=404 if self.path=='/missing' else 302 if self.path=='/redirect' else 200
  body=b'\x00\xff\xfe' if self.path=='/binary' else b'x'*524289 if self.path=='/large' else b'local script HTTP response'
  self.send_response(status)
  if status==302:self.send_header('Location','/redirect-target')
  self.send_header('Content-Length',str(len(body)));self.end_headers();self.wfile.write(body)
httpd=http.server.ThreadingHTTPServer(('127.0.0.1',0),Handler);threading.Thread(target=httpd.serve_forever,daemon=True).start()
with tempfile.TemporaryDirectory(prefix='sv2-http-tls-') as tmp:
 cert=pathlib.Path(tmp)/'cert.pem';key=pathlib.Path(tmp)/'key.pem'
 subprocess.run(['openssl','req','-x509','-newkey','rsa:2048','-nodes','-keyout',str(key),'-out',str(cert),'-days','1','-subj','/CN=localhost'],stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL,check=True)
 tls=http.server.ThreadingHTTPServer(('127.0.0.1',0),Handler);context=ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER);context.load_cert_chain(cert,key);tls.socket=context.wrap_socket(tls.socket,server_side=True);threading.Thread(target=tls.serve_forever,daemon=True).start()
 env=os.environ.copy();env.update(DYLD_INSERT_LIBRARIES=str(root/'loader-probe.dylib'),OPEN_SV2_MCP_PLUGIN=str(root/'sv2-mcp.dylib'),LOADER_PROBE_PLUGIN=str(root/'build/capability-plugin-fixture.dylib'),OPEN_SV2_LOADER_DATA_DIR=str(scratch),LOADER_PROBE_BLOCK_HOST_AT_START='1')
 with (scratch/'stdout.log').open('wb') as out,(scratch/'stderr.log').open('wb') as err:
  p=subprocess.Popen([os.environ['LOADER_PROBE_DIRECT_HOST']],env=env,stdout=out,stderr=err)
  try:
   descriptor=scratch/f'endpoints/sv2-mcp-{p.pid}.json';deadline=time.monotonic()+10
   while p.poll() is None and not descriptor.exists() and time.monotonic()<deadline:time.sleep(.05)
   d=json.loads(descriptor.read_text());port=urllib.parse.urlsplit(d['url']).port
   def call(args):
    c=http.client.HTTPConnection('127.0.0.1',port,timeout=12);c.request('POST','/mcp',json.dumps({'jsonrpc':'2.0','id':1,'method':'tools/call','params':{'name':'svext_fixture_http_get','arguments':args}}),{'Content-Type':'application/json','Authorization':'Bearer '+d['token']});r=c.getresponse();v=json.loads(r.read());c.close();return v.get('result',{}).get('structuredContent',v)
   local=f'http://127.0.0.1:{httpd.server_port}';checks={}
   value=call({'url':local+'/ok'});checks['local-get-while-host-blocked']=value.get('ok') and value['result']['body']=='local script HTTP response';checks['http-status']=value.get('ok') and value['result']['status']==200
   value=call({'url':local+'/missing'});checks['http-errors-retain-status']=value.get('ok') and value['result']['status']==404
   value=call({'url':local+'/redirect'});checks['redirect-not-followed']=value.get('ok') and value['result']['status']==302 and '/redirect-target' not in seen
   value=call({'url':local+'/binary'});checks['binary-base64-roundtrip']=value.get('ok') and value['result']['bodyEncoding']=='base64' and base64.b64decode(value['result']['body'])==b'\x00\xff\xfe'
   value=call({'url':local+'/large'});checks['bounded-response']=value.get('error',{}).get('code')=='HTTP_RESPONSE_TOO_LARGE'
   checks['file-scheme-rejected']=call({'url':'file:///no-such-test-file'}).get('error',{}).get('code')=='INVALID_HTTP_REQUEST'
   checks['url-credentials-rejected']=call({'url':f'http://test:placeholder@127.0.0.1:{httpd.server_port}/credentials'}).get('error',{}).get('code')=='INVALID_HTTP_REQUEST'
   checks['unknown-options-rejected']=call({'url':local+'/ok','headers':{}}).get('error',{}).get('code')=='INVALID_HTTP_REQUEST'
   value=call({'url':f'https://127.0.0.1:{tls.server_port}/tls'});checks['untrusted-tls-rejected']=value.get('error',{}).get('code')=='HTTP_TRANSPORT_ERROR'
   value=call({'url':'https://example.com/'});checks['online-https-with-tls-verification']=value.get('ok') and value['result']['status']==200 and 'Example Domain' in value['result']['body']
   checks['host-remained-blocked']='blocked=1' in (scratch/'stderr.log').read_text(errors='replace')
   report={'host_pid':p.pid,'checks':checks,'passed':all(checks.values()),'shared_native_http_tested':True,'javascript_frontend_implemented':False,'test_only_mcp_forwarder':True};(root/'evidence/script-http-runtime.json').write_text(json.dumps(report,indent=2));print(json.dumps(report,indent=2));assert report['passed']
  finally:
   if p.poll() is None:
    p.terminate()
    try:p.wait(timeout=5)
    except subprocess.TimeoutExpired:p.kill();p.wait(timeout=5)
   httpd.shutdown();tls.shutdown()
