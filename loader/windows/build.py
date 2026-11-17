"""Reproducible x64 MinGW cross/native build; never installs into SV2."""
from pathlib import Path
import argparse, hashlib, json, os, re, shutil, subprocess
parser=argparse.ArgumentParser();parser.add_argument('--out',type=Path,required=True);parser.add_argument('--prefix',default=os.environ.get('MINGW_PREFIX','x86_64-w64-mingw32-'));args=parser.parse_args()
source=Path(__file__).resolve().parent;out=args.out.resolve();out.mkdir(parents=True,exist_ok=True)
module=(source.parents[1]/'plugins/javascript-extend/sv2_extend.js').read_text(encoding='utf-8')
(out/'javascript_bundle.h').write_text('static const char loaderJavaScriptModule[]='+json.dumps(module)+';\n',encoding='utf-8')
def run(tool,*arguments):subprocess.run([args.prefix+tool,*map(str,arguments)],check=True)
run('gcc','-c',source/'early_entry.S','-o',out/'early_entry.o')
relocations=subprocess.check_output([args.prefix+'objdump','-r',str(out/'early_entry.o')],text=True)
assert 'RELOCATION RECORDS' not in relocations,relocations
run('objcopy','-O','binary','--only-section=.text',out/'early_entry.o',out/'early_entry.bin')
blob=(out/'early_entry.bin').read_bytes();assert 64<len(blob)<2048
(out/'early_entry_blob.h').write_text('static const unsigned char extend_apc_blob[]={'+','.join(map(str,blob))+'};\n')
flags=['-std=c++17','-D_WIN32_WINNT=0x0602','-O2','-Wall','-Wextra','-Werror','-Wno-cast-function-type','-Wno-misleading-indentation','-static','-static-libgcc','-static-libstdc++','-I'+str(out)]
hook_objects=[]
for relative in ['buffer.c','hook.c','trampoline.c','hde/hde64.c']:
    obj=out/('minhook-'+Path(relative).stem+'.o');run('gcc','-O2','-c',source/'vendor/minhook/src'/relative,'-o',obj);hook_objects.append(obj)
run('g++',*flags,'-shared',source/'core.cpp',source/'editor_gate.cpp',source/'editor_autostart.cpp',source/'javascript_gate.cpp',source/'voice_adapter.cpp',*hook_objects,'-o',out/'ExtendCore.dll','-luser32','-lgdi32','-ladvapi32','-lshell32','-lole32','-luuid','-lbcrypt','-Wl,--dynamicbase,--nxcompat')
run('g++',*flags,'-municode',source/'launcher.cpp','-o',out/'ExtendLauncher.exe','-luserenv','-ladvapi32','-lshell32','-lole32','-loleaut32','-luuid','-Wl,--dynamicbase,--nxcompat')
run('g++',*flags,'-municode',source/'fixture.cpp','-o',out/'ExtendFixture.exe','-luser32','-Wl,--dynamicbase,--nxcompat')
(out/'namespace-host').mkdir(exist_ok=True)
run('g++',*flags,'-municode',source.parents[1]/'test/windows_namespace_fixture.cpp','-o',out/'namespace-host/NamespaceFixture.exe','-lshell32','-lole32','-luuid','-luser32','-Wl,--dynamicbase,--nxcompat')
plugin=out/'smoke-profile/plugins/example.hello/native/windows-x86_64/plugin.dll';plugin.parent.mkdir(parents=True,exist_ok=True)
run('gcc','-shared','-O2','-Wall','-Wextra','-Werror','-I'+str(source.parents[1]/'sdk/include'),source.parents[1]/'sdk/examples/hello_plugin.c','-o',plugin,'-static-libgcc')
(plugin.parents[2]/'plugin.json').write_text(json.dumps({'schema':1,'id':'example.hello','name':'Hello','version':'1.0.0','nativeLibraries':{'windows-x86_64':'native/windows-x86_64/plugin.dll'}},indent=2))
files=['ExtendCore.dll','ExtendLauncher.exe','ExtendFixture.exe']
imports={name:re.findall(r'DLL Name: (.+)',subprocess.check_output([args.prefix+'objdump','-p',str(out/name)],text=True)) for name in files}
assert all(dll.lower() in {'kernel32.dll','user32.dll','gdi32.dll','shell32.dll','ole32.dll','oleaut32.dll','userenv.dll','msvcrt.dll','ucrtbase.dll','advapi32.dll','bcrypt.dll'} or dll.lower().startswith('api-ms-win-') for names in imports.values() for dll in names),imports
report={'platform':'windows-x86_64','crossCompiled':os.name!='nt','runtimeVerified':False,'apcRelocationFree':True,'systemDLLsOnly':True,'imports':imports,'files':{name:hashlib.sha256((out/name).read_bytes()).hexdigest() for name in files},'scope':'Windows resident Core and sandboxed managed-host build. Runtime and installed SV2 verification are separate; editor availability comes from the live provider and whole-host network verification remains incomplete.'}
(out/'build-results.json').write_text(json.dumps(report,indent=2));print(json.dumps(report,indent=2))
(out/'THIRD-PARTY-NOTICES.txt').write_text((source.parents[1]/'NOTICE').read_text(encoding='utf-8')+'\n'+(source/'vendor/minhook/LICENSE.txt').read_text(encoding='utf-8')+'\n'+(source/'vendor/JSON-LICENSE.MIT').read_text(encoding='utf-8'),encoding='utf-8')
resources=out/'EditorLua';resources.mkdir(exist_ok=True)
for name in ['sv2_extend.lua','sv2_extend_json.lua','editor_provider.lua','windows_runtime.lua']:
    shutil.copyfile(source.parents[1]/'plugins/lua-extend/lua'/name,resources/name)
subprocess.run([os.sys.executable,str(source.parents[1]/'plugins/sv2-mcp/windows/build.py'),'--out',str(out),'--compiler',args.prefix+'g++'],check=True)
