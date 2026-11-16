from pathlib import Path
import json,os,time,threading,http.server,ssl,subprocess,hashlib
base=Path(__file__).resolve().parents[4];h=json.loads((base/'work/release-0.7.0/live-host.json').read_text());pid=h['pid'];os.kill(pid,0);root=Path(h['dataDirectory']);run=base/'work/release-0.7.0/http';run.mkdir(exist_ok=False);seen=[];closed=set()
class Handler(http.server.BaseHTTPRequestHandler):
 def log_message(self,*args):pass
 def do_POST(self):
  body=self.rfile.read(int(self.headers.get('Content-Length','0'))).decode();seen.append(self.path);data=json.dumps({'body':body,'header':self.headers.get('X-Test')},ensure_ascii=False).encode();self.send_response(200);self.send_header('Content-Length',str(len(data)));self.end_headers();self.wfile.write(data)
 def do_GET(self):
  seen.append(self.path);slow=self.path.endswith('/slow');self.send_response(200);self.send_header('Content-Length','100000' if slow else '2');self.end_headers()
  try:
   if slow:
    for i in range(1000):self.wfile.write(b'x'*100);self.wfile.flush();time.sleep(.01)
   else:self.wfile.write(b'OK')
  except (BrokenPipeError,ConnectionResetError):closed.add(self.path)
server=http.server.ThreadingHTTPServer(('127.0.0.1',0),Handler);threading.Thread(target=server.serve_forever,daemon=True).start();cert=run/'cert.pem';key=run/'key.pem';subprocess.run(['openssl','req','-x509','-newkey','rsa:2048','-nodes','-keyout',str(key),'-out',str(cert),'-days','1','-subj','/CN=localhost'],check=True,capture_output=True);tls=http.server.ThreadingHTTPServer(('127.0.0.1',0),Handler);ctx=ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER);ctx.load_cert_chain(cert,key);tls.socket=ctx.wrap_socket(tls.socket,server_side=True);threading.Thread(target=tls.serve_forever,daemon=True).start();url=f'http://127.0.0.1:{server.server_port}';tlsURL=f'https://127.0.0.1:{tls.server_port}'
js='''function getClientInfo(){return {name:'Installed 0.7.0 JS HTTP',author:'Open SV2 Loader',category:'Open SV2 Loader',versionNumber:1,minEditorVersion:131840};}
function main(){var E=eval(Duktape.enc('opensv2.module.v1','')),checks={},info=E.runtimeInfo();checks.pid=info.result.pid;checks.mainThread=info.result.mainThread===true;checks.noFileOrPipe=info.result.usesFilesOrPipes===false;
 E.http.request({url:LOCAL+'/js/post',method:'POST',headers:{'Content-Type':'application/json','X-Test':'installed-js'},body:JSON.stringify({text:'声库请求'})},function(e,r){checks.post=!e&&r.status===200;if(!e){var v=JSON.parse(r.body);checks.header=v.header==='installed-js';checks.unicode=JSON.parse(v.body).text==='声库请求';}
 E.http.get('https://example.com/',function(e,r){checks.onlineHTTPS=!e&&r.status===200&&r.body.indexOf('Example Domain')>=0;
 E.http.get(TLS+'/tls',function(e,r){checks.untrustedTlsRejected=!!e&&e.code==='HTTP_TRANSPORT_ERROR';var count=0;
 var handle=E.http.get(LOCAL+'/js/slow',function(e,r){count++;checks.cancelled=!!e&&e.code==='HTTP_CANCELLED';
 E.http.get(LOCAL+'/js/fast',function(e,r){checks.followup=!e&&r.body==='OK';var early=E.http.request({url:LOCAL+'/js/early'},function(e,r){checks.immediateCancel=!!e&&e.code==='HTTP_CANCELLED';SV.setTimeout(200,function(){checks.singleCallback=count===1;handle.cancel();SV.print('INSTALLED_070_JS_HTTP '+JSON.stringify(checks));SV.finish();});});early.cancel();early.cancel();});});SV.setTimeout(150,function(){handle.cancel();handle.cancel();});
 });});});}
'''.replace('LOCAL',json.dumps(url)).replace('TLS',json.dumps(tlsURL))
lua='''function getClientInfo() return {name='Installed 0.7.0 Lua HTTP',author='Open SV2 Loader',category='Open SV2 Loader',versionNumber=1,minEditorVersion=131840} end
function main()
 package.path=MODULEPATH..'/?.lua;'..package.path
 local E=require('sv2_extend');local json=require('sv2_extend_json');local checks={};local info=E.runtimeInfo();checks.pid=info.pid;checks.mainThread=info.mainThread==true;checks.noFileOrPipe=info.usesFilesOrPipes==false
 E.http.request({url=LOCAL..'/lua/post',method='POST',headers={['Content-Type']='application/json',['X-Test']='installed-lua'},body=json.encode({text='声库请求'})},function(r,e)
 checks.post=not e and r.status==200;if not e then local v=json.decode(r.body);checks.header=v.header=='installed-lua';checks.unicode=json.decode(v.body).text=='声库请求' end
 E.http.get('https://example.com/',function(r,e) checks.onlineHTTPS=not e and r.status==200 and r.body:find('Example Domain',1,true)~=nil
 E.http.get(TLS..'/tls',function(r,e) checks.untrustedTlsRejected=e and e.code=='HTTP_TRANSPORT_ERROR' or false;local count=0;local handle
 handle=E.http.get(LOCAL..'/lua/slow',function(r,e) count=count+1;checks.cancelled=e and e.code=='HTTP_CANCELLED' or false
 E.http.get(LOCAL..'/lua/fast',function(r,e) checks.followup=not e and r.body=='OK'
 local early=E.http.request({url=LOCAL..'/lua/early'},function(r,e) checks.immediateCancel=e and e.code=='HTTP_CANCELLED' or false
 SV:setTimeout(200,function() checks.singleCallback=count==1;handle.cancel();SV:print('INSTALLED_070_LUA_HTTP '..json.encode(checks));SV:finish() end)
 end);early.cancel();early.cancel()
 end) end);SV:setTimeout(150,function() handle.cancel();handle.cancel() end)
 end) end) end)
end
'''.replace('MODULEPATH',json.dumps(str(root/'plugins/lua-extend/lua'))).replace('LOCAL',json.dumps(url)).replace('TLS',json.dumps(tlsURL))
scripts=root/'host-home/Library/Application Support/Dreamtonics/Synthesizer V Studio 2/scripts'
for name,body in [('OpenSV2Installed070JSHTTP.js',js),('OpenSV2Installed070LuaHTTP.lua',lua)]:
 path=scripts/name;assert not path.exists();path.write_text(body)
log=base/'work/release-0.7.0/launch.stdout';offset=log.stat().st_size;print(json.dumps({'pid':pid,'waitingFor':['Installed 0.7.0 Lua HTTP','Installed 0.7.0 JS HTTP']}),flush=True);reports={};deadline=time.monotonic()+420
while time.monotonic()<deadline:
 os.kill(pid,0)
 with log.open('rb') as f:f.seek(offset);lines=f.read().decode(errors='replace').splitlines()
 for line in lines:
  for name,marker in [('lua','INSTALLED_070_LUA_HTTP '),('js','INSTALLED_070_JS_HTTP ')]:
   if line.startswith(marker):reports[name]=json.loads(line[len(marker):])
 if len(reports)==2:break
 time.sleep(.1)
if len(reports)!=2:raise RuntimeError('Inspect same installed host PID '+str(pid))
for language,checks in reports.items():checks['pidMatchesInstalledHost']=checks.get('pid')==pid;checks['serverSawCancelledConnection']=('/'+language+'/slow') in closed
installedModule=Path(h['installedApplication'])/'Contents/Resources/Native/Lua/sv2_extend.lua';shared={'installedLuaModuleMatchesRuntime':hashlib.sha256(installedModule.read_bytes()).digest()==hashlib.sha256((root/'plugins/lua-extend/lua/sv2_extend.lua').read_bytes()).digest(),'hostAlive':True};passed=all(bool(v) for c in reports.values() for v in c.values()) and all(shared.values());report={'pid':pid,'frontends':reports,'sharedChecks':shared,'passed':passed,'scope':'Actual installed 0.7.0 managed host, embedded JS module and installed Lua module, official scripting frontends. Anonymous online HTTPS, local POST/headers/Unicode, TLS rejection, cancellation and follow-up requests.'};(base/'work/release-0.7.0/installed-http-results.json').write_text(json.dumps(report,indent=2));print(json.dumps(report,indent=2));server.shutdown();tls.shutdown();assert passed
