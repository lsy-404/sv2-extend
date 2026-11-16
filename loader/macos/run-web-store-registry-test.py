from pathlib import Path
import json,subprocess,tempfile
source=Path(__file__).resolve().parent;work=source.parents[3]/'work/web-store-registry';work.mkdir(parents=True,exist_ok=True);binary=work/'fixture'
subprocess.run(['xcrun','clang','-fobjc-arc','-Wall','-Wextra','-Werror',*[str(source/p) for p in ['web_store_registry_test.m','web_store_registry.m','website_data_namespace.m']],'-framework','AppKit','-framework','WebKit','-o',str(binary)],check=True)
with tempfile.TemporaryDirectory(prefix='extend-web-store-registry-') as directory:
 p=subprocess.run([str(binary),directory],capture_output=True,text=True,timeout=20);(work/'stderr.log').write_text(p.stderr);assert p.returncode==0,(p.returncode,p.stdout,p.stderr);checks=json.loads(p.stdout)
report={'checks':checks,'passed':all(checks.values()),'scope':'Real WebKit objects, factory/configuration namespace chaining, weak lifetime, main-thread snapshot and bounded-registry failure closure. No network or whole-host isolation claim.'};(work/'results.json').write_text(json.dumps(report,indent=2));print(json.dumps(report,indent=2));assert report['passed']
