from pathlib import Path
import os, subprocess, tempfile, json
source=Path(__file__).resolve().parent
with tempfile.TemporaryDirectory(prefix='extend-settings-') as directory:
 root=Path(directory);binary=root/'settings-test';profile=root/'profile';profile.mkdir(mode=0o700)
 subprocess.run(['xcrun','clang','-fobjc-arc','-Wall','-Wextra','-Werror','-Wl,-export_dynamic',str(source/'runtime_settings_test.m'),str(source/'plugin_ui.m'),str(source/'lua_call_gate.m'),'-framework','Foundation','-o',str(binary)],check=True)
 env=dict(os.environ,OPEN_SV2_LOADER_DATA_DIR=str(profile))
 phases={}
 for name,args in [('write',[]),('restore',['restore'])]:
  result=subprocess.run([str(binary),*args],env=env,capture_output=True,text=True);assert result.returncode==0,result.stdout+result.stderr;phases[name]=json.loads(result.stdout)
 legacy=root/'legacy';(legacy/'preferences').mkdir(parents=True,mode=0o700);(legacy/'preferences/extend-ui.json').write_text(json.dumps({'schema':1,'plugins':{'opensv2.lua-extend':{'visible':True,'settings':{'voicebanks':False}}}}))
 result=subprocess.run([str(binary),'migration'],env=dict(env,OPEN_SV2_LOADER_DATA_DIR=str(legacy)),capture_output=True,text=True);assert result.returncode==0,result.stdout+result.stderr;phases['migration']=json.loads(result.stdout)
 print(json.dumps({'phases':phases,'passed':True,'scope':'Native registry, real script call gate and profile persistence. Capability handlers are test stubs; not a real-host functionality claim.'},indent=2))
