from pathlib import Path
import os,json,time,threading,http.server,subprocess,shutil,hashlib
source=Path(__file__).resolve().parent;base=source.parents[3];handle=json.loads((base/'work/host-data-integration/public-live-host.json').read_text());app=Path(handle['managedApp']);root=Path(handle['root']);run=Path(handle['runDirectory'])/'js-request';
if (run/'result.json').exists():raise RuntimeError('Completed run already exists; select a fresh owned prepared profile')
run.mkdir(exist_ok=True)
executable=str(app/'Contents/MacOS/synthv-studio-original')
for row in subprocess.check_output(['ps','-axo','pid=,comm='],text=True).splitlines():
 fields=row.strip().split(None,1)
 if len(fields)==2 and fields[1]==executable:raise RuntimeError('Managed app already has a live process; preserve it')
# Require the previous owned host to be terminal before changing its disposable app.
try:os.kill(handle['pid'],0)
except ProcessLookupError:pass
else:raise RuntimeError('Previous host still live; do not mutate its app')
core=app/'Contents/Frameworks/OpenSV2Loader.dylib';shutil.copy2(core,run/'previous-loader-probe.dylib');shutil.copy2(source/'loader-probe.dylib',core);subprocess.run(['codesign','--force','--deep','--sign','-',str(app)],check=True,capture_output=True)
seen=[]
class Handler(http.server.BaseHTTPRequestHandler):
 def log_message(self,*args):pass
 def do_POST(self):
  body=self.rfile.read(int(self.headers.get('Content-Length','0'))).decode();seen.append({'method':self.command,'path':self.path,'body':body,'header':self.headers.get('X-Test')});data=json.dumps(seen[-1],ensure_ascii=False).encode();self.send_response(200);self.send_header('Content-Length',str(len(data)));self.end_headers();self.wfile.write(data)
 def do_GET(self):
  seen.append({'method':'GET','path':self.path});self.send_response(200);self.send_header('Content-Length','2');self.end_headers();self.wfile.write(b'OK')
server=http.server.ThreadingHTTPServer(('127.0.0.1',0),Handler);threading.Thread(target=server.serve_forever,daemon=True).start();url=f'http://127.0.0.1:{server.server_port}'
script='''function getClientInfo(){return {name:'JS Extend Request Acceptance',author:'Open SV2 Loader',category:'Open SV2 Loader',versionNumber:1,minEditorVersion:131840};}
function main(){
 var E=eval(Duktape.enc('opensv2.module.v1','')),checks={},info=E.runtimeInfo();checks.nativePid=info.result.pid;checks.mainThread=info.result.mainThread===true;checks.noFileOrPipe=info.result.usesFilesOrPipes===false;
 E.http.request({url:URL+'/post',method:'POST',headers:{'Content-Type':'application/json','X-Test':'native-js'},body:JSON.stringify({text:'声库请求'})},function(error,response){
  checks.post=!error&&response.status===200;
  if(!error){var result=JSON.parse(response.body);checks.method=result.method==='POST';checks.header=result.header==='native-js';checks.unicodeBody=JSON.parse(result.body).text==='声库请求';}
  E.http.get(URL+'/get',function(error,response){checks.legacyGet=!error&&response.body==='OK';
   E.http.request({url:'https://example.com/'},function(error,response){checks.onlineSwitch=!!error&&error.code==='HTTP_ACCESS_DENIED';
    E.http.request({url:URL+'/invalid',headers:{'X-Test':'bad\\r\\nInjected: true'}},function(error,response){checks.invalidHeaders=!!error&&error.code==='INVALID_HTTP_REQUEST';SV.print('OPEN_SV2_JS_REQUEST_RESULT '+JSON.stringify(checks));SV.finish();});
   });
  });
 });
}'''.replace('URL',json.dumps(url))
scripts=root/'host-home/Library/Application Support/Dreamtonics/Synthesizer V Studio 2/scripts';path=scripts/'OpenSV2JSRequestAcceptance.js';assert not path.exists();path.write_text(script);os.chmod(path,0o600)
env=os.environ.copy();env.pop('OPEN_SV2_LOADER_DATA_DIR',None);env.pop('OPEN_SV2_NETWORK_DIAGNOSTICS',None);env['OPEN_SV2_SCRIPT_HTTP_ONLINE']='0'
with (run/'stdout.log').open('wb') as out,(run/'stderr.log').open('wb') as err:p=subprocess.Popen([str(app/'Contents/MacOS/loader-launcher')],env=env,stdout=out,stderr=err,start_new_session=True)
state={'pid':p.pid,'app':str(app),'script':str(path),'runDirectory':str(run),'coreSHA256':hashlib.sha256(core.read_bytes()).hexdigest()};(base/'work/js-http-request/host.json').write_text(json.dumps(state,indent=2));print(json.dumps(state),flush=True)
deadline=time.monotonic()+300;checks=None
while p.poll() is None and time.monotonic()<deadline:
 for line in (run/'stdout.log').read_text(errors='replace').splitlines():
  if line.startswith('OPEN_SV2_JS_REQUEST_RESULT '):checks=json.loads(line.split(' ',1)[1]);break
 if checks is not None:break
 time.sleep(.2)
if checks is None:raise RuntimeError('No official JS result; inspect same live host')
checks['nativePidMatchesHost']=checks.get('nativePid')==p.pid;checks['hostAlive']=p.poll() is None;checks['onlyExpectedLocalRequests']=[x['path'] for x in seen]==['/post','/get'];report=dict(state,checks=checks,passed=all(bool(x) for x in checks.values()),scope='Official Duktape JS in real managed SV2 2.3.0 arm64; local request + online permission denial. No online success claim for this run.')
(run/'result.json').write_text(json.dumps(report,indent=2));(base/'work/js-http-request/host-result.json').write_text(json.dumps(report,indent=2));print(json.dumps(report,indent=2),flush=True);server.shutdown();assert report['passed']
# Retain the owned blank host for follow-up lifecycle checks.
