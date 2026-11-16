from pathlib import Path
import json,os,subprocess,shutil,http.server,threading,time,uuid,hashlib
base=Path(__file__).resolve().parents[4];h=json.loads((base/'work/http-cancel/host.json').read_text());pid=h['pid'];os.kill(pid,0);root=Path(h['root']);run=Path(h['runDirectory'])/('lua-http-'+uuid.uuid4().hex);run.mkdir();modules=run/'modules';modules.mkdir();source=base/'projects/sv2-extend/plugins/lua-extend/lua';shutil.copy2(source/'sv2_extend.lua',modules/'sv2_extend_http_acceptance.lua');shutil.copy2(source/'sv2_extend_json.lua',modules/'sv2_extend_json.lua');seen=[];closed=threading.Event()
class Handler(http.server.BaseHTTPRequestHandler):
 def log_message(self,*args):pass
 def do_POST(self):
  body=self.rfile.read(int(self.headers.get('Content-Length','0'))).decode();seen.append(self.path);data=json.dumps({'body':body,'header':self.headers.get('X-Test')},ensure_ascii=False).encode();self.send_response(200);self.send_header('Content-Length',str(len(data)));self.end_headers();self.wfile.write(data)
 def do_GET(self):
  seen.append(self.path);slow=self.path=='/slow';self.send_response(200);self.send_header('Content-Length','100000' if slow else '2');self.end_headers()
  try:
   if slow:
    for i in range(1000):self.wfile.write(b'x'*100);self.wfile.flush();time.sleep(.01)
   else:self.wfile.write(b'OK')
  except (BrokenPipeError,ConnectionResetError):closed.set()
server=http.server.ThreadingHTTPServer(('127.0.0.1',0),Handler);threading.Thread(target=server.serve_forever,daemon=True).start();url=f'http://127.0.0.1:{server.server_port}'
script='''function getClientInfo() return {name='Lua Extend HTTP Request Acceptance',author='Open SV2 Loader',category='Open SV2 Loader',versionNumber=1,minEditorVersion=131840} end
function main()
 package.path=MODULEPATH..'/?.lua;'..package.path
 local E=require('sv2_extend_http_acceptance');local json=require('sv2_extend_json');local checks={};local info=E.runtimeInfo();checks.pid=info.pid;checks.mainThread=info.mainThread==true;checks.noFileOrPipe=info.usesFilesOrPipes==false
 E.http.request({url=URL..'/post',method='POST',headers={['Content-Type']='application/json',['X-Test']='lua-native'},body=json.encode({text='声库请求'})},function(response,err)
  checks.post=not err and response.status==200
  if not err then local result=json.decode(response.body);checks.header=result.header=='lua-native';checks.unicodeBody=json.decode(result.body).text=='声库请求' end
  local count=0;local handle
  handle=E.http.get(URL..'/slow',function(response,err)
   count=count+1;checks.cancelled=err and err.code=='HTTP_CANCELLED' or false
   E.http.get(URL..'/fast',function(response,err)
    checks.followup=not err and response.body=='OK'
    local early=E.http.request({url=URL..'/early'},function(response,err)
     checks.immediateCancel=err and err.code=='HTTP_CANCELLED' or false
     SV:setTimeout(200,function() checks.exactlyOneCallback=count==1;handle.cancel();checks.completedCancelSafe=true;SV:print('OPEN_SV2_LUA_HTTP_RESULT '..json.encode(checks));SV:finish() end)
    end)
    early.cancel();early.cancel()
   end)
  end)
  checks.hasCancelHandle=type(handle)=='table' and type(handle.cancel)=='function'
  SV:setTimeout(150,function() handle.cancel();handle.cancel() end)
 end)
end
'''.replace('MODULEPATH',json.dumps(str(modules))).replace('URL',json.dumps(url))
path=root/'host-home/Library/Application Support/Dreamtonics/Synthesizer V Studio 2/scripts/OpenSV2LuaHTTPRequestAcceptance.lua';assert not path.exists();path.write_text(script);log=Path(h['runDirectory'])/'stdout.log';offset=log.stat().st_size;state=dict(h,script=str(path),luaModuleSHA256=hashlib.sha256((source/'sv2_extend.lua').read_bytes()).hexdigest(),runDirectory=str(run));(base/'work/lua-http-request/host.json').write_text(json.dumps(state,indent=2));print(json.dumps(state),flush=True)
deadline=time.monotonic()+300;checks=None
while time.monotonic()<deadline:
 os.kill(pid,0)
 with log.open('rb') as f:f.seek(offset);lines=f.read().decode(errors='replace').splitlines()
 for line in lines:
  if line.startswith('OPEN_SV2_LUA_HTTP_RESULT '):checks=json.loads(line.split(' ',1)[1]);break
 if checks is not None:break
 time.sleep(.1)
if checks is None:raise RuntimeError('Inspect same live host PID '+str(pid))
checks['pidMatchesHost']=checks['pid']==pid;checks['serverSawConnectionClose']=closed.wait(2);os.kill(pid,0);checks['hostAlive']=True;report=dict(state,checks=checks,passed=all(bool(v) for v in checks.values()),scope='Official Lua script in existing managed SV2, exact source facade loaded as an isolated test module; no installed Lua update or new NW-core regression claim.');(run/'result.json').write_text(json.dumps(report,indent=2));(base/'work/lua-http-request/results.json').write_text(json.dumps(report,indent=2));print(json.dumps(report,indent=2));server.shutdown();assert report['passed']
