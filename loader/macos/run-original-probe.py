import json,os,pathlib,subprocess,time
root=pathlib.Path(__file__).resolve().parent
app=pathlib.Path(os.environ.get('LOADER_PROBE_APP','/Applications/Synthesizer V Studio 2 Pro.app'))
exe=app/'Contents/MacOS/synthv-studio'
results=[]
for name in ['baseline','original-dyld']:
 env=os.environ.copy()
 for key in ['DYLD_INSERT_LIBRARIES','LOADER_PROBE_PLUGIN']: env.pop(key,None)
 if name=='original-dyld':
  env['DYLD_INSERT_LIBRARIES']=str(root/'loader-probe.dylib')
  env['LOADER_PROBE_PLUGIN']=str(root/'sample-plugin.dylib')
 with (root/(name+'.stdout')).open('wb') as out,(root/(name+'.stderr')).open('wb') as err:
  p=subprocess.Popen([str(exe)],cwd=exe.parent,env=env,stdout=out,stderr=err)
  time.sleep(4)
  alive=p.poll() is None
  vm=subprocess.run(['vmmap',str(p.pid)],capture_output=True,text=True) if alive else None
  if vm: (root/(name+'.vmmap')).write_text(vm.stdout+vm.stderr)
  r={'case':name,'pid':p.pid,'alive_at_4s':alive,'early_exit':p.poll(),'vmmap_exit':vm.returncode if vm else None}
  if alive:
   p.terminate()
   try:p.wait(timeout=5)
   except subprocess.TimeoutExpired:p.kill();p.wait(timeout=5)
  r['final_exit']=p.returncode
 log=(root/(name+'.stderr')).read_text(errors='replace')
 r['probe_markers']=[line for line in log.splitlines() if 'LOADER_PROBE' in line]
 r['probe_image_visible']=bool(vm and any('loader-probe.dylib' in line or 'sample-plugin.dylib' in line for line in vm.stdout.splitlines()))
 results.append(r)
(root/'original-runtime-results.json').write_text(json.dumps(results,indent=2))
print(json.dumps(results,indent=2))
