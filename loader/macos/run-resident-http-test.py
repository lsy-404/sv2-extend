import http.server,threading,json,subprocess,os,pathlib
source=pathlib.Path(__file__).resolve().parent
scratch=source.parents[3]/'work/js-http-request'
scratch.mkdir(parents=True,exist_ok=True)
driver=scratch/'driver'
subprocess.run(['xcrun','clang','-fobjc-arc','-Wall','-Wextra','-Werror','-I',str(source),str(source/'resident_http_test_driver.m'),str(source/'http_capability.m'),str(source/'resident_http.m'),'-framework','Foundation','-lcurl','-o',str(driver)],check=True)
seen=[]
class H(http.server.BaseHTTPRequestHandler):
 def log_message(self,*args):pass
 def go(self):
  data=self.rfile.read(int(self.headers.get('Content-Length','0')));seen.append((self.command,self.path,data))
  body=json.dumps({'method':self.command,'body':data.decode(),'header':self.headers.get('X-Test')},ensure_ascii=False).encode();self.send_response(302 if self.path=='/redirect' else 200)
  if self.path=='/redirect':self.send_header('Location','/unwanted')
  self.send_header('Content-Length',str(len(body)));self.end_headers()
  if self.command!='HEAD':self.wfile.write(body)
 do_GET=do_POST=do_PUT=do_PATCH=do_DELETE=do_OPTIONS=do_HEAD=go
server=http.server.ThreadingHTTPServer(('127.0.0.1',0),H);threading.Thread(target=server.serve_forever,daemon=True).start();url=f'http://127.0.0.1:{server.server_port}';checks={}
def call(args,method='http.request',**env):
 p=subprocess.run([str(driver),method,json.dumps(args,ensure_ascii=False)],env=dict(os.environ,**env),capture_output=True,text=True,timeout=14);assert p.returncode==0,p.stderr;return json.loads(p.stdout)
for method in ['GET','HEAD','POST','PUT','PATCH','DELETE','OPTIONS']:
 args={'url':url+'/echo','method':method,'headers':{'X-Test':'hello'}}
 if method not in ['GET','HEAD']:args['body']='本地请求\u0000data'
 v=call(args);checks[method]=v.get('ok') and v['result']['status']==200 and (v['result']['body']=='' if method=='HEAD' else json.loads(v['result']['body'])=={'method':method,'body':args.get('body',''),'header':'hello'})
checks['legacyGET']=call({'url':url+'/legacy'},'http.get').get('ok') is True
for label,extra in {'invalidMethod':{'method':'CONNECT'},'badHeaderName':{'headers':{'Bad Name':'x'}},'headerInjection':{'headers':{'X-Test':'x\r\ny: z'}},'hostOverride':{'headers':{'Host':'x'}},'lengthOverride':{'headers':{'Content-Length':'0'}},'invalidBody':{'method':'POST','body':[]},'getBody':{'body':'x'},'largeBody':{'method':'POST','body':'x'*65537},'unknown':{'surprise':True}}.items():
 before=len(seen);v=call(dict(url=url+'/invalid',**extra));checks[label]=v.get('error',{}).get('code')=='INVALID_HTTP_REQUEST' and len(seen)==before
before=len(seen);checks['localDenied']=call({'url':url+'/denied'},OPEN_SV2_SCRIPT_HTTP_LOCAL='0').get('error',{}).get('code')=='HTTP_ACCESS_DENIED' and len(seen)==before
checks['onlineDenied']=call({'url':'https://example.com/'},OPEN_SV2_SCRIPT_HTTP_ONLINE='0').get('error',{}).get('code')=='HTTP_ACCESS_DENIED'
checks['onlineOffLocalAllowed']=call({'url':url+'/allowed'},OPEN_SV2_SCRIPT_HTTP_ONLINE='0').get('ok') is True
checks['127PrefixDNSNotLocal']=call({'url':'http://127.example.com/'},OPEN_SV2_SCRIPT_HTTP_ONLINE='0').get('error',{}).get('code')=='HTTP_ACCESS_DENIED'
v=call({'url':url+'/redirect','method':'POST','body':'payload'});checks['noRedirect']=v.get('ok') and v['result']['status']==302 and not any(x[1]=='/unwanted' for x in seen)
report={'checks':checks,'passed':all(checks.values()),'scope':'Real libcurl transport and shared capability in isolated native test process, not SV2/JS frontend acceptance.'};(scratch/'results.json').write_text(json.dumps(report,indent=2));print(json.dumps(report,indent=2));server.shutdown();assert report['passed']
