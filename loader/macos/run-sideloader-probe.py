import hashlib,json,os,pathlib,subprocess,time
root=pathlib.Path(__file__).resolve().parent
app=pathlib.Path('/Applications/Synthesizer V Studio 2 Pro.app');exe=app/'Contents/MacOS/synthv-studio'
def digest():
 with exe.open('rb') as stream:return hashlib.file_digest(stream,'sha256').hexdigest()
before=digest();environment=os.environ.copy()
for key in ['DYLD_INSERT_LIBRARIES','LOADER_PROBE_PLUGIN','LOADER_PROBE_HTTP_BASE','LOADER_PROBE_BLOCK_HOST_AT_START']:environment.pop(key,None)
with (root/'sideloader.stdout').open('wb') as out,(root/'sideloader.stderr').open('wb') as err:
 p=subprocess.Popen([str(root/'sideloader'),str(exe),str(root/'loader-probe.dylib'),str(root/'network-plugin.dylib')],env=environment,stdout=out,stderr=err)
 time.sleep(4);alive=p.poll() is None
 vm=subprocess.run(['vmmap',str(p.pid)],capture_output=True,text=True) if alive else None
 permission=subprocess.run([str(root/'task-port-probe'),str(p.pid)],capture_output=True,text=True) if alive else None
 child=subprocess.run(['pgrep','-P',str(p.pid)],capture_output=True,text=True)
 if alive:
  p.terminate()
  try:p.wait(timeout=5)
  except subprocess.TimeoutExpired:p.kill();p.wait(timeout=5)
log=(root/'sideloader.stderr').read_text(errors='replace')
verify=subprocess.run(['codesign','--verify','--strict',str(app)],capture_output=True,text=True)
report={'case':'independent-no-patch-sideloader','pid':p.pid,'alive_at_4s':alive,'child_pids':child.stdout.split(),'official_executable_sha256_before':before,'official_executable_sha256_after':digest(),'strict_signature_exit':verify.returncode,'launch_markers':[line for line in log.splitlines() if 'SIDELOADER' in line or 'LOADER_PROBE' in line],'probe_loaded':bool(vm and any('loader-probe.dylib' in line or 'network-plugin.dylib' in line for line in vm.stdout.splitlines())),'vmmap_exit':vm.returncode if vm else None,'task_port_probe':json.loads(permission.stdout) if permission and permission.returncode==0 else None,'final_exit':p.returncode}
(root/'sideloader-results.json').write_text(json.dumps(report,indent=2));print(json.dumps(report,indent=2))
