import http.server,json,os,pathlib,ssl,subprocess,tempfile,threading,time
base=pathlib.Path.cwd();source=base/'projects/sv2-extend/loader/macos';data=base/'work/duktape-native/runtime';data.mkdir(parents=True,exist_ok=True)
seen=[]
class Handler(http.server.BaseHTTPRequestHandler):
 def log_message(self,*args):pass
 def do_GET(self):
  seen.append(self.path);status=404 if self.path=='/missing' else 302 if self.path=='/redirect' else 200
  body=b'\x00\xff\xfe' if self.path=='/binary' else 'JS本地网络响应'.encode()
  self.send_response(status)
  if status==302:self.send_header('Location','/redirect-target')
  self.send_header('Content-Length',str(len(body)));self.end_headers();self.wfile.write(body)
server=http.server.ThreadingHTTPServer(('127.0.0.1',0),Handler);threading.Thread(target=server.serve_forever,daemon=True).start()
with tempfile.TemporaryDirectory(prefix='js-native-tls-') as tmp:
 cert=pathlib.Path(tmp)/'cert.pem';key=pathlib.Path(tmp)/'key.pem';subprocess.run(['openssl','req','-x509','-newkey','rsa:2048','-nodes','-keyout',str(key),'-out',str(cert),'-days','1','-subj','/CN=localhost'],stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL,check=True)
 tls=http.server.ThreadingHTTPServer(('127.0.0.1',0),Handler);ctx=ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER);ctx.load_cert_chain(cert,key);tls.socket=ctx.wrap_socket(tls.socket,server_side=True);threading.Thread(target=tls.serve_forever,daemon=True).start()
 script=r'''// Open SV2 JS native-call integration test. No editor changes.
function getClientInfo(){return {name:'JS Extend Native Call Test',author:'Open SV2 Loader',category:'Open SV2 Loader',versionNumber:1,minEditorVersion:131330};}
function main(){
 var checks={}, finished=false;
 function finish(error){if(finished)return;finished=true;SV.print('OPEN_SV2_JS_NATIVE_RESULT '+JSON.stringify({checks:checks,error:error?String(error):null}));SV.finish();}
 try {
  var Extend=eval(Duktape.enc('opensv2.module.v1',''));
  var info=Extend.runtimeInfo();checks.nativePid=info.ok&&info.result.pid;checks.mainThread=info.ok&&info.result.mainThread===true;checks.noFileOrPipe=info.ok&&info.result.usesFilesOrPipes===false;
  checks.normalHex=Duktape.enc('hex','foo')==='666f6f';checks.normalBase64=Duktape.enc('base64','foo')==='Zm9v';checks.normalHexCoercion=Duktape.enc('hex',123)==='313233';
  try{Duktape.enc('not-a-real-format','foo');checks.normalError=false;}catch(e){checks.normalError=e instanceof TypeError;}
  var invalid=JSON.parse(Duktape.enc('opensv2.v1','not json'));checks.invalidJson=invalid.ok===false&&invalid.error.code==='INVALID_ARGUMENT';
  Extend.call('fixture.echo',{unicode:'声库\uD83D\uDE00',boolean:true,number:60,array:[1,2]},function(error,result){
   if(error){finish(error.code);return;}
   checks.unicodeRoundtrip=result.echo.unicode==='声库\uD83D\uDE00';checks.typedRoundtrip=result.echo.boolean===true&&result.echo.number===60&&result.echo.array[1]===2;
   Extend.call('fixture.network_block',{},function(error,result){
    if(error){finish(error.code);return;}checks.hostBlocked=result.blocked===1;
    var tests=[
     {name:'localGet',url:LOCAL+'/ok',verify:function(e,r){return !e&&r.status===200&&r.body==='JS本地网络响应';}},
     {name:'onlineHttps',url:'https://example.com/',verify:function(e,r){return !e&&r.status===200&&r.body.indexOf('Example Domain')>=0;}},
     {name:'http404',url:LOCAL+'/missing',verify:function(e,r){return !e&&r.status===404;}},
     {name:'noRedirect',url:LOCAL+'/redirect',verify:function(e,r){return !e&&r.status===302&&r.redirectsFollowed===false;}},
     {name:'binary',url:LOCAL+'/binary',verify:function(e,r){return !e&&r.bodyEncoding==='base64'&&r.body==='AP/+';}},
     {name:'noFileScheme',url:'file:///no-such-js-test-file',verify:function(e,r){return !!e&&e.code==='INVALID_HTTP_REQUEST';}},
     {name:'tlsVerification',url:TLS+'/tls',verify:function(e,r){return !!e&&e.code==='HTTP_TRANSPORT_ERROR';}}
    ];var index=0;
    function next(){
     if(index===tests.length){Extend.call('account.switch',{},function(error,result){checks.privateMethodDenied=!!error&&error.code==='METHOD_NOT_PUBLIC';Extend.call('fixture.network_allow',{},function(error,result){checks.hostRestored=!error&&result.blocked===0;finish(error&&error.code);});});return;}
     var test=tests[index++];Extend.http.get(test.url,function(error,result){checks[test.name]=!!test.verify(error,result);if(error)SV.print('JS_TEST '+test.name+' '+error.code);next();});
    }next();
   });
  });
 }catch(error){finish(error);}
}
'''.replace('LOCAL',json.dumps(f'http://127.0.0.1:{server.server_port}')).replace('TLS',json.dumps(f'https://127.0.0.1:{tls.server_port}'))
 scriptPath=pathlib.Path.home()/'Library/Application Support/Dreamtonics/Synthesizer V Studio 2/scripts/OpenSV2JSNativeCallTest.js';scriptPath.write_text(script)
 env=os.environ.copy();env.update(DYLD_INSERT_LIBRARIES=str(source/'loader-probe.dylib'),OPEN_SV2_ENABLE_JS_NATIVE='1',OPEN_SV2_MCP_PLUGIN=str(source/'sv2-mcp.dylib'),LOADER_PROBE_PLUGIN=str(source/'build/capability-plugin-fixture.dylib'),OPEN_SV2_LOADER_DATA_DIR=str(data));env.pop('LOADER_PROBE_BLOCK_HOST_AT_START',None)
 host=base/'work/duktape-native/host/SV2 Loader Probe.app/Contents/MacOS/synthv-studio-original'
 with (data/'stdout.log').open('wb') as out,(data/'stderr.log').open('wb') as err:p=subprocess.Popen([str(host)],env=env,stdout=out,stderr=err,start_new_session=True)
 (data/'host.json').write_text(json.dumps({'pid':p.pid,'executable':str(host)},indent=2));print(json.dumps({'pid':p.pid,'waiting_for':'JS Extend Native Call Test official script'}),flush=True)
 deadline=time.monotonic()+240;report=None
 try:
  while p.poll() is None and time.monotonic()<deadline:
   lines=(data/'stdout.log').read_text(errors='replace').splitlines()
   for line in lines:
    if line.startswith('OPEN_SV2_JS_NATIVE_RESULT '):report=json.loads(line.split(' ',1)[1]);break
   if report is not None:break
   time.sleep(.1)
  if report is None:raise RuntimeError('JS did not finish; inspect logs and live host before retrying')
  checks=report['checks'];checks['nativePidMatchesHost']=checks.get('nativePid')==p.pid;checks['redirectTargetNotRequested']='/redirect-target' not in seen;checks['hostSurvived']=p.poll() is None
  report.update(host_pid=p.pid,passed=not report.get('error') and len(checks)>=20 and all(bool(v) for v in checks.values()),seen_test_paths=seen,architecture='arm64',profile='SV2 2.3.0 / Duktape 2.5.0')
  (source/'evidence/javascript-native-http-runtime.json').write_text(json.dumps(report,indent=2,ensure_ascii=False));print(json.dumps(report,indent=2,ensure_ascii=False),flush=True)
 finally:
  server.shutdown();tls.shutdown()
 # Leave the owned test host alive for follow-up script/negative tests.
