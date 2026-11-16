import argparse, json, os, pathlib, subprocess, tempfile
repo=pathlib.Path(__file__).resolve().parents[1];parser=argparse.ArgumentParser();parser.add_argument('--installer',type=pathlib.Path,default=repo/'loader/windows/install.ps1');args=parser.parse_args();root=pathlib.Path(tempfile.mkdtemp(prefix='sv2-long-backup-'));build=root/'build';build.mkdir();install=root/'install';install.mkdir();hostdir=root/'official';hostdir.mkdir();(hostdir/'synthv-studio.exe').write_bytes(b'fixture');local=root/'local';local.mkdir()
for name in ('ExtendLauncher.exe','ExtendCore.dll','Sv2Mcp.exe','THIRD-PARTY-NOTICES.txt'):(build/name).write_bytes(b'fixture')
resources=build/'EditorLua';resources.mkdir()
for name in ('sv2_extend.lua','sv2_extend_json.lua','editor_provider.lua','windows_runtime.lua'):(resources/name).write_text('fixture')
package=build/'plugins/sv2.mcp';dll=package/'native/windows-x86_64/plugin.dll';dll.parent.mkdir(parents=True);dll.write_bytes(b'fixture');(package/'plugin.json').write_text(json.dumps({'schema':1,'id':'sv2.mcp','nativeLibraries':{'windows-x86_64':'native/windows-x86_64/plugin.dll'}}))
(install/'Native').write_text('prevent install beyond backup')
relative=pathlib.Path('Profile/runtime')
for n in range(5): relative=relative/('segment'+str(n)+'-'+'x'*42)
relative=relative/'data.lmdb';deep=install/relative;extended='\\\\?\\'+str(deep);os.makedirs('\\\\?\\'+str(deep.parent),exist_ok=True)
with open(extended,'wb') as f:f.write(b'preserved')
assert len(str(deep))>260
# Reset a deliberately explicit ACL on a separate long-path fixture.
separate=root/'acl-fixture';separate.mkdir();aclfile=separate/relative;os.makedirs('\\\\?\\'+str(aclfile.parent),exist_ok=True)
with open('\\\\?\\'+str(aclfile),'wb') as f:f.write(b'acl')
subprocess.run(['icacls','\\\\?\\'+str(aclfile),'/grant','*S-1-1-0:F'],capture_output=True,check=True)
subprocess.run(['icacls','\\\\?\\'+str(separate),'/reset','/T','/Q'],capture_output=True,check=True)
acl=subprocess.run(['icacls','\\\\?\\'+str(aclfile)],capture_output=True,text=True,encoding='utf-8',errors='replace',check=True).stdout
assert '(I)' in acl and 'Everyone:(F)' not in acl
# Exercise the actual installer through its backup phase; the Native file stops it before installation or shortcuts.
env=os.environ.copy();env['LOCALAPPDATA']=str(local)
result=subprocess.run(['pwsh','-NoProfile','-NonInteractive','-File',str(args.installer),'-BuildDirectory',str(build),'-HostExecutable',str(hostdir/'synthv-studio.exe'),'-InstallDirectory',str(install)],capture_output=True,text=True,encoding='utf-8',errors='replace',env=env,timeout=60)
output=result.stdout+result.stderr
assert result.returncode!=0 and 'Cannot protect the copied backup' not in output,output
backups=list((local/'SV2ExtendBackups').iterdir());assert len(backups)==1,output
copy=backups[0]/'Installation'/relative;assert len(str(copy))>260
with open('\\\\?\\'+str(copy),'rb') as f:assert f.read()==b'preserved'
assert (install/'Native').read_text()=='prevent install beyond backup'
assert 'ExtendLauncher.exe' in output,output
print('extended-path recursive ACL reset succeeded on long file; installer passed backup phase without modifying source')
