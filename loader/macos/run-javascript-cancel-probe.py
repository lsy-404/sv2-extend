from pathlib import Path
import json,os,subprocess,shutil,http.server,threading,time,plistlib
source=Path(__file__).resolve().parent;base=source.parents[3];previous=json.loads((base/'work/script-http-settings/results.json').read_text());probe=Path(previous['probeApp']);exe=probe/'Contents/MacOS/ExtendLoader';root=Path(previous['runDirectory'])/'profile';run=Path(previous['runDirectory'])/'js-cancel-v2';run.mkdir(exist_ok=False)
assets=probe/'Contents/Resources/Native';shutil.copy2(source/'loader-probe.dylib',assets/'loader-probe.dylib');shutil.copy2(base/'projects/sv2-extend/plugins/javascript-extend/sv2_extend.js',assets/'JavaScript/sv2_extend.js');subprocess.run(['codesign','--force','--sign','-',str(probe)],check=True,capture_output=True)
p=subprocess.run([str(exe),'--script-http-local','allow','--script-http-online','deny','--configure-only'],capture_output=True,text=True);assert p.returncode==0,p.stderr
p=subprocess.run([str(exe),'--experimental-host-data','--prepare-only'],capture_output=True,text=True);assert p.returncode==0,p.stderr;app=Path(p.stdout.strip());meta=plistlib.loads((app/'Contents/Info.plist').read_bytes())
seen=[];closed=threading.Event()
class Handler(http.server.BaseHTTPRequestHandler):
 def log_message(self,*args):pass
 def do_GET(self):
  seen.append(self.path);self.send_response(200);slow=self.path=='/slow';self.send_header('Content-Length','100000' if slow else '2');self.end_headers()
  try:
   if slow:
    for i in range(1000):self.wfile.write(b'x'*100);self.wfile.flush();time.sleep(.01)
   else:self.wfile.write(b'OK')
  except (BrokenPipeError,ConnectionResetError):closed.set()
server=http.server.ThreadingHTTPServer(('127.0.0.1',0),Handler);threading.Thread(target=server.serve_forever,daemon=True).start();url=f'http://127.0.0.1:{server.server_port}'
script='''function getClientInfo(){return {name:'JS Extend Cancellation Acceptance V2',author:'Open SV2 Loader',category:'Open SV2 Loader',versionNumber:1,minEditorVersion:131840};}
function main(){var E=eval(Duktape.enc('opensv2.module.v1','')),checks={},calls=0,info=E.runtimeInfo();checks.pid=info.result.pid;checks.mainThread=info.result.mainThread===true;checks.noFileOrPipe=info.result.usesFilesOrPipes===false;
 var handle=E.http.get(URL+'/slow',function(error,response){calls++;checks.cancelled=!!error&&error.code==='HTTP_CANCELLED';
  E.http.get(URL+'/fast',function(error,response){checks.followup=!error&&response.body==='OK';
   var early=E.http.request({url:URL+'/early'},function(error,response){checks.earlyCancelled=!!error&&error.code==='HTTP_CANCELLED';
    SV.setTimeout(200,function(){checks.exactlyOneCallback=calls===1;handle.cancel();checks.cancelAfterCompletionSafe=true;SV.print('OPEN_SV2_JS_CANCEL_RESULT '+JSON.stringify(checks));SV.finish();});
   });early.cancel();early.cancel();
  });
 });checks.hasCancelHandle=!!handle&&typeof handle.cancel==='function';SV.setTimeout(150,function(){handle.cancel();handle.cancel();});
}'''.replace('URL',json.dumps(url))
path=root/'host-home/Library/Application Support/Dreamtonics/Synthesizer V Studio 2/scripts/OpenSV2JSCancelAcceptanceV2.js';assert not path.exists();path.write_text(script)
env=os.environ.copy();env.pop('OPEN_SV2_LOADER_DATA_DIR',None);env.pop('OPEN_SV2_NETWORK_DIAGNOSTICS',None)
with (run/'stdout.log').open('wb') as out,(run/'stderr.log').open('wb') as err:p=subprocess.Popen([str(app/'Contents/MacOS/loader-launcher')],env=env,stdout=out,stderr=err,start_new_session=True)
handle={'pid':p.pid,'app':str(app),'bundleIdentifier':meta['CFBundleIdentifier'],'runDirectory':str(run),'root':str(root)};(base/'work/http-cancel/host.json').write_text(json.dumps(handle,indent=2));print(json.dumps(handle),flush=True)
deadline=time.monotonic()+300;checks=None
while p.poll() is None and time.monotonic()<deadline:
 for line in (run/'stdout.log').read_text(errors='replace').splitlines():
  if line.startswith('OPEN_SV2_JS_CANCEL_RESULT '):checks=json.loads(line.split(' ',1)[1]);break
 if checks is not None:break
 time.sleep(.1)
if checks is None:raise RuntimeError('Inspect same live host '+str(p.pid))
checks['pidMatchesHost']=checks['pid']==p.pid;checks['hostAlive']=p.poll() is None;checks['serverConnectionClosed']=closed.wait(2);report=dict(handle,checks=checks,passed=all(bool(x) for x in checks.values()),scope='Actual official JS cancellation and follow-up request in SV2 2.3.0 ARM; no project edits.');(run/'result.json').write_text(json.dumps(report,indent=2));(base/'work/http-cancel/host-result.json').write_text(json.dumps(report,indent=2));print(json.dumps(report,indent=2));server.shutdown();assert report['passed']
