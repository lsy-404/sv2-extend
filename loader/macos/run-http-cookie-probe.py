"""Disposable native cookie namespaces; no real account/session files copied."""
from pathlib import Path
import email.utils
import http.server,http.cookies,threading,os,json,uuid,time,subprocess,plistlib,stat,hashlib
source=Path(__file__).resolve().parent;base=source.parents[3];run=Path.home()/'Library/Application Support/Dreamtonics/Open SV2 Loader/experiments/http-cookies'/uuid.uuid4().hex;run.mkdir(parents=True,mode=0o700)
roots=[run/'profileA',run/'profileB']
for root in roots:(root/'home/Library/Application Support/Dreamtonics/Synthesizer V Studio 2/license').mkdir(parents=True,mode=0o700)
usesSession=False
sourceSessionBefore=None
alias=run/'profileA-alias';alias.symlink_to(roots[0],target_is_directory=True);seen=[]
class Handler(http.server.BaseHTTPRequestHandler):
 def log_message(self,*args):pass
 def do_GET(self):
  cookies=http.cookies.SimpleCookie();cookies.load(self.headers.get('Cookie',''));item=cookies.get('svext_http_namespace');marker=item.value if item else None;seen.append({'path':self.path,'marker':marker});data=json.dumps({'marker':marker}).encode();self.send_response(200)
  if self.path in ['/set/A','/set/B']:self.send_header('Set-Cookie','svext_http_namespace='+self.path[-1]+'; Max-Age=86400; Expires='+email.utils.formatdate(time.time()+86400,usegmt=True)+'; Path=/; HttpOnly')
  self.send_header('Content-Type','application/json');self.send_header('Content-Length',str(len(data)));self.end_headers();self.wfile.write(data)
server=http.server.ThreadingHTTPServer(('127.0.0.1',0),Handler);threading.Thread(target=server.serve_forever,daemon=True).start()
core=source/'loader-probe.dylib';native=base/'work/host-data/host-data-probe.dylib';fixture=base/'work/http-cookies/http-cookie-fixture.dylib';results=[];executables=[];identifiers=[]
for index in range(2):
 parent=run/('app'+str(index));parent.mkdir();app=parent/'SV2 Loader Probe.app';subprocess.run(['/bin/cp','-cR',str(base/'work/host-data/host/SV2 Loader Probe.app'),str(app)],check=True)

 for input,output in [(source/'loader-probe.dylib','LoaderCoreTest.dylib'),(base/'work/host-data/host-data-probe.dylib','HostDataTest.dylib'),(base/'work/http-cookies/http-cookie-fixture.dylib','HTTPCookieTest.dylib')]:
  target=app/'Contents/Frameworks'/output;subprocess.run(['/bin/cp',str(input),str(target)],check=True);subprocess.run(['codesign','--force','--sign','-',str(target)],check=True,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
 infoPath=app/'Contents/Info.plist';info=plistlib.loads(infoPath.read_bytes());identifier='io.opensv2.cookie-probe.p'+run.name+str(index);info.update(CFBundleIdentifier=identifier,CFBundleExecutable='synthv-studio-original');infoPath.write_bytes(plistlib.dumps(info));subprocess.run(['codesign','--force','--sign','-',str(app/'Contents/MacOS/loader-launcher')],check=True,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL);subprocess.run(['codesign','--force','--sign','-','--options','runtime','--entitlements',str(base/'work/host-data/host/loader-test-entitlements.plist'),str(app)],check=True,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL);subprocess.run(['codesign','--verify','--deep','--strict',str(app)],check=True);executables.append(app/'Contents/MacOS/synthv-studio-original');identifiers.append(identifier)
for phase,root,marker,reopen in [('seedA',roots[0],'A',False),('seedB',roots[1],'B',False),('reopenA',alias,'A',True)]:
 exe=executables[1 if marker=='B' else 0];expectedIdentifier=identifiers[1 if marker=='B' else 0]
 env=os.environ.copy()
 for key in ['LOADER_PROBE_BLOCK_HOST_AT_START','OPEN_SV2_AUTO_EDITOR_PROVIDER','OPEN_SV2_MCP_PLUGIN','OPEN_SV2_TEST_OWNED_PRODUCTS','OPEN_SV2_TEST_DATA_CONTROL','OPEN_SV2_TEST_RESTORE_NETWORK_AFTER_8S','LOADER_COOKIE_REOPEN','LOADER_COOKIE_EXPLICIT_FLUSH']:env.pop(key,None)
 env.update(DYLD_INSERT_LIBRARIES=str(exe.parents[1]/'Frameworks/LoaderCoreTest.dylib')+':'+str(exe.parents[1]/'Frameworks/HostDataTest.dylib')+':'+str(exe.parents[1]/'Frameworks/HTTPCookieTest.dylib'),LOADER_PROBE_PLUGIN=str(exe.parents[1]/'Frameworks/HTTPCookieTest.dylib'),OPEN_SV2_LOADER_DATA_DIR=str(root),OPEN_SV2_TEST_HOST_HOME=str(root/'home'),OPEN_SV2_ISOLATE_WEBSITE_DATA='1',LOADER_COOKIE_MARKER=marker,LOADER_COOKIE_BASE=f'http://127.0.0.1:{server.server_port}')
 if os.environ.get('WEBSITE_NAMESPACE_IN_COOKIE_TEST')=='0':env.pop('OPEN_SV2_ISOLATE_WEBSITE_DATA',None)
 if os.environ.get('EXPECT_NATIVE_COOKIE_CLEAR')=='1':env['LOADER_COOKIE_EXPECT_NATIVE_CLEAR']='1'
 if reopen:env['LOADER_COOKIE_REOPEN']='1'
 resultPath=run/(phase+'.native.json');env['LOADER_COOKIE_RESULT']=str(resultPath);environmentPath=run/(phase+'.environment.json');environmentPath.write_text(json.dumps({k:v for k,v in env.items() if k.startswith(('DYLD_','LOADER_','OPEN_SV2_'))}));environmentPath.chmod(0o600)
 launch=json.loads(subprocess.check_output([str(base/'work/http-cookies/launch-fixture'),str(exe.parents[2]),str(environmentPath)],text=True));pid=launch['pid']
 def alive():
  value=subprocess.run(['ps','-p',str(pid),'-o','stat='],capture_output=True,text=True).stdout.strip();return bool(value and not value.startswith('Z'))
 handle={'pid':pid,'phase':phase,'executable':str(exe),'runDirectory':str(run),'launchMode':'NSWorkspace'};(run/'live-host.json').write_text(json.dumps(handle,indent=2));(base/'work/http-cookies/live-host.json').write_text(json.dumps(handle,indent=2));print(json.dumps(handle),flush=True)
 deadline=time.monotonic()+40
 while alive() and time.monotonic()<deadline and not resultPath.exists():time.sleep(.1)
 if not resultPath.exists():raise RuntimeError('No result; inspect same PID '+str(pid))
 r=json.loads(resultPath.read_text());r.update(phase=phase,hostAlive=alive(),pidMatches=r['pid']==pid,bundleMatches=r['bundleIdentifier']==expectedIdentifier)
 deadline=time.monotonic()+10
 while alive() and time.monotonic()<deadline:time.sleep(.1)
 if alive():raise RuntimeError('Normal quit still pending; inspect same PID '+str(pid))
 r['terminationObservedAfterQuitRequest']=True
 results.append(r);(run/(phase+'.json')).write_text(json.dumps(r,indent=2))
 if not r['passed'] or not r['bundleMatches']:
  failure={'usesPrivateEncryptedSessionCopy':usesSession,'websiteNamespaceEnabled':os.environ.get('WEBSITE_NAMESPACE_IN_COOKIE_TEST')!='0','passed':False,'phases':results,'failedPhase':phase,'runDirectory':str(run),'scope':'Native per-app cookie probe; failed retention must not be reported as verified isolation.'};(run/'results.json').write_text(json.dumps(failure,indent=2));(base/'work/http-cookies/latest.json').write_text(json.dumps(failure,indent=2));raise AssertionError('Native cookie fixture failed: '+str(run/(phase+'.json')))
server.shutdown()
checks={'threeHostsPassed':len(results)==3 and all(r['passed'] and r['hostAlive'] and r['pidMatches'] and r['bundleMatches'] for r in results),'differentDataRootsUseDifferentBundleIdentities':results[0]['bundleIdentifier']!=results[1]['bundleIdentifier'],'sameRootAliasKeepsBundleIdentity':results[0]['bundleIdentifier']==results[2]['bundleIdentifier'],'reopenMatchesExplicitCookiePolicy':results[2]['checks']['sharedBefore'] and (not results[2]['expectNativeStartupClear'] or results[2]['checks']['hostClearedTwoFixtureCookies']),'sessionOnlyDoesNotSurviveRelaunch':results[2]['checks']['noSessionOnlyCookieRestored']}
report={'contract':'native-startup-cookie-clear' if os.environ.get('EXPECT_NATIVE_COOKIE_CLEAR')=='1' else 'cookie-retention-through-host-startup','usesPrivateEncryptedSessionCopy':usesSession,'checks':checks,'passed':all(checks.values()),'phases':results,'observedSyntheticRequests':seen,'runDirectory':str(run),'testedCore':str(core),'sourceSessionSnapshotSHA256':sourceSessionBefore,'sourceSessionStillEqualsSnapshot':sourceSessionBefore==hashlib.sha256(original.read_bytes()).hexdigest() if usesSession else None,'scope':'System-native per-app cookie isolation using two managed application bundle identities. Shared/default URLSession, ephemeral/custom/nil preservation, session-only behavior and native persistence. Host startup cleanup is explicitly observed, not suppressed. Not app-group cookie stores, Keychain, actual account switching, or full native data-directory integration.'};(run/'results.json').write_text(json.dumps(report,indent=2));(base/'work/http-cookies/latest.json').write_text(json.dumps(report,indent=2));print(json.dumps(report,indent=2));assert report['passed']
