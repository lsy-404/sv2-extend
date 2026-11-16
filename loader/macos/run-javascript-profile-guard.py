import pathlib,subprocess,plistlib,struct,json,os,time,http.client,urllib.parse
base=pathlib.Path.cwd();source=base/'projects/sv2-extend/loader/macos';target=base/'work/duktape-native/unsupported/SV2 Loader Probe.app';target.parent.mkdir(parents=True,exist_ok=True)
if not target.exists():
 subprocess.run(['/usr/bin/ditto',str(base/'work/duktape-native/host/SV2 Loader Probe.app'),str(target)],check=True)
 executable=target/'Contents/MacOS/synthv-studio-original';b=bytearray(executable.read_bytes());count=struct.unpack_from('>I',b,4)[0]
 for i in range(count):
  cpu,_,off,_,_=struct.unpack_from('>IIIII',b,8+20*i)
  if cpu==0x100000c:break
 ncmd=struct.unpack_from('<I',b,off+16)[0];cursor=off+32
 for i in range(ncmd):
  cmd,size=struct.unpack_from('<II',b,cursor)
  if cmd==0x1b:b[cursor+8]^=1;break
  cursor+=size
 else:raise RuntimeError('Mach-O UUID missing')
 executable.write_bytes(b)
 # Only this disposable copy's UUID changes. Preserve runtime entitlements.
 subprocess.run(['codesign','--force','--sign','-','--preserve-metadata=entitlements,flags,runtime',str(executable)],check=True)
 p=target/'Contents/Info.plist';info=plistlib.loads(p.read_bytes());info['CFBundleIdentifier']='io.opensv2.js-unsupported-probe';p.write_bytes(plistlib.dumps(info));subprocess.run(['codesign','--force','--sign','-',str(target)],check=True)
scratch=base/'work/duktape-native/profile-guard';scratch.mkdir(exist_ok=True);env=os.environ.copy();env.update(DYLD_INSERT_LIBRARIES=str(source/'loader-probe.dylib'),OPEN_SV2_ENABLE_JS_NATIVE='1',OPEN_SV2_MCP_PLUGIN=str(source/'sv2-mcp.dylib'),OPEN_SV2_LOADER_DATA_DIR=str(scratch));env.pop('LOADER_PROBE_PLUGIN',None)
with (scratch/'stdout.log').open('wb') as out,(scratch/'stderr.log').open('wb') as err:p=subprocess.Popen([str(target/'Contents/MacOS/synthv-studio-original')],env=env,stdout=out,stderr=err)
try:
 endpoint=scratch/f'endpoints/sv2-mcp-{p.pid}.json';deadline=time.monotonic()+10
 while p.poll() is None and not endpoint.exists() and time.monotonic()<deadline:time.sleep(.05)
 d=json.loads(endpoint.read_text());c=http.client.HTTPConnection('127.0.0.1',urllib.parse.urlsplit(d['url']).port,timeout=3);c.request('POST','/mcp',json.dumps({'jsonrpc':'2.0','id':1,'method':'ping'}),{'Content-Type':'application/json','Authorization':'Bearer '+d['token']});r=c.getresponse();reply=json.loads(r.read());c.close();log=(scratch/'stderr.log').read_text(errors='replace')
 checks={'unsupported-profile-declined':'js-native-call-unavailable' in log,'no-js-table-hook-installed':'JS_GATE installed' not in log,'mcp-still-working':r.status==200 and reply.get('result')=={},'host-still-alive':p.poll() is None}
 checks['unsupported-voice-profile-declined']='voice-native-identity-unavailable' in log
 checks['no-voice-table-hook-installed']='VOICE_HOST identity-adapter-installed' not in log
 report={'pid':p.pid,'checks':checks,'passed':all(checks.values()),'only-disposable-copy-uuid-modified':True};(source/'evidence/javascript-profile-guard.json').write_text(json.dumps(report,indent=2));print(json.dumps(report,indent=2));assert report['passed']
finally:
 if p.poll() is None:
  p.terminate()
  try:p.wait(timeout=5)
  except subprocess.TimeoutExpired:p.kill();p.wait(timeout=5)
