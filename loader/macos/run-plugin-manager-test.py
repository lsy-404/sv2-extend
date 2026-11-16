from pathlib import Path
import os, subprocess, tempfile, json, zipfile, io, stat, hashlib, http.server, threading
source=Path(__file__).resolve().parent;base=source.parents[3];work=base/'work/plugin-manager';work.mkdir(exist_ok=True)
binary=work/'driver'
subprocess.run(['xcrun','clang','-DLOADER_PLUGIN_MANAGER_TESTING','-fobjc-arc','-Wall','-Wextra','-Werror',*[str(source/x) for x in ['plugin_manager_test_driver.m','plugin_manager.m','startup_policy.m','resident_http.m','socket_gate.c']],'-framework','Foundation','-larchive','-lcurl','-o',str(binary)],check=True)
fixture=work/'fixture.dylib';subprocess.run(['xcrun','clang','-dynamiclib','-Wall','-Wextra','-Werror',str(source/'plugin.c'),'-o',str(fixture)],check=True)
checks={};responses={}
class Handler(http.server.BaseHTTPRequestHandler):
 def do_GET(self):
  body=responses.get(self.path)
  self.send_response(200 if body is not None else 404);self.end_headers()
  if body is not None:self.wfile.write(body)
 def log_message(self,*a):pass
server=http.server.ThreadingHTTPServer(('127.0.0.1',0),Handler);threading.Thread(target=server.serve_forever,daemon=True).start();url=f'http://127.0.0.1:{server.server_port}'
def package(version='1.0.0',id='example.manager',extra=None,entries=None,wrapper=False,cross=False):
 manifest={'schema':1,'id':id,'name':'Manager Fixture','version':version,'loaderAPI':1,'nativeLibrary':'plugin.dylib'}
 if extra:manifest.update(extra)
 if cross:manifest.pop('nativeLibrary');manifest['nativeLibraries']={'macos-arm64':'native/macos-arm64/plugin.dylib','macos-x86_64':'native/macos-x86_64/plugin.dylib','windows-x86_64':'native/windows-x86_64/plugin.dll'}
 data=io.BytesIO()
 with zipfile.ZipFile(data,'w',zipfile.ZIP_DEFLATED) as archive:
  prefix='package/' if wrapper else ''
  archive.writestr(prefix+'plugin.json',json.dumps(manifest));archive.writestr(prefix+'marker.txt',version)
  if cross:
   archive.writestr('native/macos-arm64/plugin.dylib',fixture.read_bytes());archive.writestr('native/windows-x86_64/plugin.dll',b'not selected on macOS')
  else:archive.writestr(prefix+'plugin.dylib',fixture.read_bytes())
  if entries:
   for name,value in entries:archive.writestr(name,value)
 return data.getvalue()
def run(root,method,args=None,env=None,code=0):
 p=subprocess.run([str(binary),str(root),method,json.dumps(args or {})],env=dict(os.environ,OPEN_SV2_ALLOW_LOCAL_PLUGIN_SOURCE='1',**(env or {})),capture_output=True,text=True)
 assert p.returncode==code,(method,p.returncode,p.stdout,p.stderr)
 return json.loads(p.stdout) if code==0 else None
with tempfile.TemporaryDirectory(prefix='extend-plugin-manager-') as temporary:
 t=Path(temporary)
 def profile(name):r=t/name;r.mkdir(mode=0o700);return r
 def local(root,data,name='package.zip'):
  p=t/name;p.write_bytes(data);return run(root,'manager.install_local',{'path':str(p)})
 root=profile('main');blob=package(extra={'updateURL':url+'/release.json'})
 r=local(root,blob);checks['localQueued']=r['reply']['ok'] and len(r['snapshot']['pending'])==1;checks['notActivatedInRunningProcess']=not (root/'plugins/example.manager').exists()
 r=run(root,'apply');checks['activatedAtStartup']=(root/'plugins/example.manager/marker.txt').read_text()=='1.0.0' and not r['snapshot']['pending']
 checks['duplicateVersionRejected']=not local(root,blob,'same.zip')['reply']['ok']
 new=package('2.0.0',extra={'updateURL':url+'/release.json'});responses['/plugin.zip']=new;responses['/release.json']=json.dumps({'id':'example.manager','version':'2.0.0','downloadURL':url+'/plugin.zip','sha256':hashlib.sha256(new).hexdigest()}).encode()
 r=run(root,'sequence',[{'method':'manager.check_updates'},{'method':'manager.install_update','arguments':{'id':'example.manager'}}]);checks['realHTTPUpdateFound']=len(r['steps'][0]['snapshot']['updates'])==1;checks['updateQueued']=len(r['snapshot']['pending'])==1;checks['oldVersionRunsUntilRestart']=(root/'plugins/example.manager/marker.txt').read_text()=='1.0.0'
 run(root,'apply');checks['updateApplied']=(root/'plugins/example.manager/marker.txt').read_text()=='2.0.0';backups=list((root/'plugin-backups').iterdir());checks['oldPackagePreserved']=any((p/'marker.txt').read_text()=='1.0.0' for p in backups)
 third=package('3.0.0',extra={'updateURL':url+'/release.json'});assert local(root,third,'third.zip')['reply']['ok'];run(root,'apply',env={'LOADER_MANAGER_TEST_CRASH_AFTER_BACKUP':'1'},code=86);checks['crashJournalPresent']=bool(list((root/'plugin-transactions').glob('*.json')));run(root,'apply');checks['recoveredThenApplied']=(root/'plugins/example.manager/marker.txt').read_text()=='3.0.0';checks['recoveredOldVersionPreserved']=any((p/'marker.txt').read_text()=='2.0.0' for p in (root/'plugin-backups').iterdir());checks['journalRetired']=not list((root/'plugin-transactions').glob('*.json'))
 assert local(root,package('4.0.0'),'fourth.zip')['reply']['ok'];manifest=root/'plugins/example.manager/plugin.json';m=json.loads(manifest.read_text());m['name']='Externally changed';manifest.write_text(json.dumps(m));run(root,'apply');checks['externalChangeNotOverwritten']=(root/'plugins/example.manager/marker.txt').read_text()=='3.0.0' and (root/'plugin-pending/example.manager/package').exists()
 r=local(profile('wrapper'),package(wrapper=True),'wrapper.zip');checks['singleWrapperSupported']=r['reply']['ok']
 r=local(profile('cross'),package(cross=True),'cross.zip');checks['platformMapSelected']=r['reply']['ok']
 lifecycle=profile('lifecycle');assert local(lifecycle,package(),'lifecycle.zip')['reply']['ok'];run(lifecycle,'apply')
 r=run(lifecycle,'manager.set_enabled',{'id':'example.manager','enabled':False,'generation':1});checks['packageDisableSaved']=r['reply']['ok'] and r['snapshot']['installed'][0]['enabled'] is False
 checks['packageDisableDoesNotDeleteCode']=(lifecycle/'plugins/example.manager/plugin.dylib').exists()
 r=run(lifecycle,'manager.set_enabled',{'id':'example.manager','enabled':True,'generation':1});checks['packageEnableSaved']=r['reply']['ok'] and r['snapshot']['installed'][0]['enabled'] is True
 checks['stalePackageToggleRefused']=not run(lifecycle,'manager.set_enabled',{'id':'example.manager','enabled':False,'generation':999})['reply']['ok']
 holderBinary=work/'lease-holder';subprocess.run(['xcrun','clang','-fobjc-arc','-Wall','-Wextra','-Werror',str(source/'startup_policy_test_driver.m'),str(source/'startup_policy.m'),'-framework','Foundation','-o',str(holderBinary)],check=True)
 holder=subprocess.Popen([str(holderBinary),str(lifecycle)],stdin=subprocess.PIPE,stdout=subprocess.PIPE,text=True)
 try:
  assert json.loads(holder.stdout.readline())['bootstrap']==0
  checks['otherHostPreventsStartupToggle']=not run(lifecycle,'manager.set_enabled',{'id':'example.manager','enabled':False,'generation':1})['reply']['ok']
  assert local(lifecycle,package('2.0.0'),'lifecycle-update.zip')['reply']['ok'];r=run(lifecycle,'apply');checks['otherHostDefersPackageReplacement']=(lifecycle/'plugins/example.manager/marker.txt').read_text()=='1.0.0' and len(r['snapshot']['pending'])==1
 finally:holder.stdin.write('{"op":"quit"}\n');holder.stdin.flush();holder.wait(timeout=5)
 run(lifecycle,'apply');checks['nextStartupAppliesAfterOtherHostExit']=(lifecycle/'plugins/example.manager/marker.txt').read_text()=='2.0.0'
 legacy=profile('legacy');(legacy/'plugins').mkdir(mode=0o755);assert local(legacy,package(),'legacy.zip')['reply']['ok'];run(legacy,'apply');checks['legacyLauncher0755DirectoryMigrated']=(legacy/'plugins/example.manager/plugin.json').exists() and stat.S_IMODE((legacy/'plugins').stat().st_mode)==0o700
 unsafe=profile('unsafe');(unsafe/'plugins').mkdir(mode=0o777);(unsafe/'plugins').chmod(0o777);assert local(unsafe,package(),'unsafe.zip')['reply']['ok'];run(unsafe,'apply');checks['worldWritableDirectoryNeverAdopted']=not(unsafe/'plugins/example.manager').exists() and stat.S_IMODE((unsafe/'plugins').stat().st_mode)==0o777
 deletion=profile('delete');assert local(deletion,package(),'delete.zip')['reply']['ok'];run(deletion,'apply');before=(deletion/'plugins/example.manager/plugin.json').read_bytes()
 r=run(deletion,'manager.remove',{'id':'example.manager','generation':1});checks['removalQueuedNotHotUnloaded']=r['reply']['ok'] and r['snapshot']['pending'][0]['operation']=='remove' and (deletion/'plugins/example.manager').exists()
 r=run(deletion,'manager.cancel_remove',{'id':'example.manager','generation':1});checks['removalCanBeCancelled']=r['reply']['ok'] and not r['snapshot']['pending'] and (deletion/'plugins/example.manager').exists()
 assert run(deletion,'manager.remove',{'id':'example.manager','generation':1})['reply']['ok'];checks['removalConflictsWithUpdate']=not local(deletion,package('2.0.0'),'delete-update.zip')['reply']['ok']
 run(deletion,'apply',env={'LOADER_MANAGER_TEST_CRASH_AFTER_REMOVAL':'1'},code=86);checks['removalCrashPreservesPackage']=not(deletion/'plugins/example.manager').exists() and any((p/'plugin.json').read_bytes()==before for p in (deletion/'plugin-backups').iterdir())
 r=run(deletion,'apply');checks['removalCrashRecoveryFinishes']=not r['snapshot']['pending'] and not r['snapshot']['installed'] and len(list((deletion/'plugin-backups').iterdir()))==1
 changed=profile('delete-changed');assert local(changed,package(),'changed-delete.zip')['reply']['ok'];run(changed,'apply');assert run(changed,'manager.remove',{'id':'example.manager','generation':1})['reply']['ok'];f=changed/'plugins/example.manager/plugin.json';m=json.loads(f.read_text());m['name']='preserve external change';f.write_text(json.dumps(m));r=run(changed,'apply');checks['removalPreservesExternalChanges']=f.exists() and len(r['snapshot']['pending'])==1
 checks['builtinsCannotBeDeleted']=not run(changed,'manager.remove',{'id':'opensv2.mcp','generation':1})['reply']['ok']
 bad=profile('bad');outside=t/'outside';outside.write_text('preserve')
 checks['traversalRejected']=not local(bad,package(entries=[('../outside','replace')]),'traversal.zip')['reply']['ok'] and outside.read_text()=='preserve'
 checks['absoluteRejected']=not local(bad,package(entries=[('/absolute-file','replace')]),'absolute.zip')['reply']['ok']
 data=io.BytesIO()
 with zipfile.ZipFile(data,'w') as archive:
  info=zipfile.ZipInfo('link');info.create_system=3;info.external_attr=(stat.S_IFLNK|0o777)<<16;archive.writestr(info,'../outside')
 checks['symlinkRejected']=not local(bad,data.getvalue(),'symlink.zip')['reply']['ok']
 checks['duplicateEntryRejected']=not local(bad,package(entries=[('plugin.json','{}')]),'duplicate.zip')['reply']['ok']
 checks['builtinNotReplaced']=not local(bad,package(id='opensv2.mcp'),'builtin.zip')['reply']['ok']
 checks['wrongAPIRejected']=not local(bad,package(extra={'loaderAPI':2}),'api.zip')['reply']['ok']
 for label,bootstrap in [('booleanABI',{'abi':True,'guardOwners':[]}),('unsupportedABI',{'abi':2,'guardOwners':[]}),('invalidOwner',{'abi':1,'guardOwners':['owner:invalid']}),('duplicateOwner',{'abi':1,'guardOwners':['owner.valid','owner.valid']}),('nonArrayOwners',{'abi':1,'guardOwners':{}})]:
  checks['bootstrapRejects'+label]=not local(bad,package(extra={'bootstrap':bootstrap}),label+'.zip')['reply']['ok']
 checks['validBootstrapPackageQueued']=local(profile('bootstrap-valid'),package(extra={'bootstrap':{'abi':1,'guardOwners':['owner.valid']}}),'bootstrap-valid.zip')['reply']['ok']

 checks['wrongBinaryRejected']=not local(bad,package(extra={'nativeLibrary':'marker.txt'}),'binary.zip')['reply']['ok']
 checks['noFailedPackageQueued']=not run(bad,'manager.snapshot')['snapshot']['pending']
 remote=profile('remote');responses['/install.json']=json.dumps({'id':'example.manager','version':'1.0.0','downloadURL':url+'/install.zip','sha256':hashlib.sha256(blob).hexdigest()}).encode();responses['/install.zip']=blob;r=run(remote,'manager.install_url',{'url':url+'/install.json'});checks['remoteVerifiedAndQueued']=len(r['snapshot']['pending'])==1
 responses['/bad-hash.json']=json.dumps({'id':'example.manager','version':'1.0.0','downloadURL':url+'/install.zip','sha256':'0'*64}).encode();r=run(profile('hash'),'manager.install_url',{'url':url+'/bad-hash.json'});checks['hashMismatchRejected']=r['snapshot']['lastError']=='PLUGIN_HASH_MISMATCH' and not r['snapshot']['pending']
 responses['/wrong-id.json']=json.dumps({'id':'other.plugin','version':'1.0.0','downloadURL':url+'/install.zip','sha256':hashlib.sha256(blob).hexdigest()}).encode();r=run(profile('identity'),'manager.install_url',{'url':url+'/wrong-id.json'});checks['downloadIdentityChecked']=r['snapshot']['lastError']=='PACKAGE_IDENTITY_MISMATCH' and not r['snapshot']['pending']
 checks['sourceURLsNotExposed']='updateURL' not in json.dumps(run(remote,'manager.snapshot')['snapshot']) and 'downloadURL' not in json.dumps(r['snapshot'])
server.shutdown();server.server_close();report={'checks':checks,'passed':all(checks.values()),'scope':'Real libarchive, native Mach-O selection, filesystem queue/apply/backup/crash recovery and resident HTTP metadata/hash downloads. CLI fixtures, not actual SV2 UI or Windows runtime.'};(work/'results.json').write_text(json.dumps(report,indent=2));print(json.dumps(report,indent=2));assert report['passed']
