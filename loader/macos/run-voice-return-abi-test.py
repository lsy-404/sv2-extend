from pathlib import Path
import subprocess,json,hashlib
source=Path(__file__).resolve().parent;base=source.parents[3];scratch=base/'work/voice-abi';scratch.mkdir(parents=True,exist_ok=True);rows=[]
for optimization in ['O0','O2']:
 binary=scratch/('test-'+optimization);subprocess.run(['xcrun','clang++','-arch','arm64','-std=c++17','-'+optimization,'-fno-elide-constructors','-Wall','-Wextra','-Werror',str(source/'voice_return_abi_test.cpp'),str(source/'voice_return_trampoline.S'),'-o',str(binary)],check=True);p=subprocess.run([str(binary)],capture_output=True,text=True,check=True);value=json.loads(p.stdout);value['optimization']=optimization;rows.append(value)
report={'rows':rows,'passed':all(v['passed'] for v in rows),'sourceSHA256':hashlib.sha256((source/'voice_return_trampoline.S').read_bytes()).hexdigest(),'scope':'ARM64 C++ nontrivial 16-byte return fixture, constructor elision disabled, sentinel and ownership counts plus original exception unwinding. No native host function-size inference from this test alone.'};(scratch/'abi-results.json').write_text(json.dumps(report,indent=2));print(json.dumps(report,indent=2));assert report['passed']
