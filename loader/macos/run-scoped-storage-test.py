from pathlib import Path
import ctypes as C,json,os,stat,subprocess,sys,tempfile,time
source=Path(__file__).resolve().parent;work=source.parents[3]/'work/scoped-storage';work.mkdir(parents=True,exist_ok=True);library=work/'storage.dylib'
subprocess.run(['xcrun','clang','-dynamiclib','-fobjc-arc','-Wall','-Wextra','-Werror',str(source/'plugin_storage.m'),'-framework','Foundation','-o',str(library)],check=True)
lib=C.CDLL(str(library));write=lib.loader_plugin_storage_write_scoped;write.argtypes=[C.c_char_p,C.c_char_p,C.c_void_p,C.c_size_t];write.restype=C.c_int;read=lib.loader_plugin_storage_read_scoped;read.argtypes=[C.c_char_p,C.c_char_p,C.c_void_p,C.POINTER(C.c_size_t)];read.restype=C.c_int
legacy=lib.loader_plugin_storage_write;legacy.argtypes=[C.c_char_p,C.c_void_p,C.c_size_t];legacy.restype=C.c_int
checks={}
def put(plugin,key,value):return write(plugin.encode(),key.encode(),value,len(value))
def get(plugin,key,capacity=1048576):
 data=C.create_string_buffer(capacity or 1);n=C.c_size_t(capacity);code=read(plugin.encode(),key.encode(),data,C.byref(n));return code,data.raw[:n.value] if code==0 else None,n.value
with tempfile.TemporaryDirectory(prefix='extend-scoped-storage-') as temporary:
 root=Path(temporary);os.environ['OPEN_SV2_LOADER_DATA_DIR']=str(root)
 checks['pluginNamespacesSeparate']=put('example.a','state.json',b'A')==put('example.b','state.json',b'B')==0 and get('example.a','state.json')[1]==b'A' and get('example.b','state.json')[1]==b'B'
 checks['storageDoesNotCreateInstallDirectories']=not (root/'plugins/example.a').exists() and not (root/'plugins/example.b').exists()
 checks['caseDistinctNamespaces']=put('Example.a','state.json',b'upper')==0 and get('Example.a','state.json')[1]==b'upper' and get('example.a','state.json')[1]==b'A'
 checks['caseDistinctKeys']=put('example.a','STATE.json',b'upper key')==0 and get('example.a','STATE.json')[1]==b'upper key' and get('example.a','state.json')[1]==b'A'
 checks['reservedCaseAliasesRejected']=all(put('example.a',key,b'bad')==-1 for key in ['.LOADER-storage.lock','.Loader-temp'])
 checks['longEncodedComponents']=put('A'*96,'Z'*128,b'long')==0 and get('A'*96,'Z'*128)[1]==b'long'
 checks['privateFileAndDirectoryModes']=stat.S_IMODE((root/'plugin-data/example.a/state.json').stat().st_mode)==0o600 and stat.S_IMODE((root/'plugin-data/example.a').stat().st_mode)==0o700
 n=C.c_size_t();checks['sizeQuery']=read(b'example.a',b'state.json',None,C.byref(n))==0 and n.value==1
 data=C.create_string_buffer(b'sentinel');n=C.c_size_t(0);checks['smallBufferUnchanged']=read(b'example.a',b'state.json',data,C.byref(n))==-9 and n.value==1 and data.value==b'sentinel'
 checks['missingDistinct']=get('example.a','missing')[0]==-8
 checks['emptyRoundTrip']=put('example.a','empty',b'')==0 and get('example.a','empty')[1]==b''
 checks['binaryRoundTrip']=put('example.a','binary',bytes(range(256)))==0 and get('example.a','binary')[1]==bytes(range(256))
 checks['oneMiBBoundary']=put('example.a','large',b'x'*1048576)==0 and get('example.a','large')[2]==1048576 and put('example.a','oversize',b'x'*1048577)==-1
 checks['invalidNamesRejected']=all(put(p,k,b'x')==-1 for p,k in [('..','state'),('example/a','state'),('example.a','../escape'),('example.a','.loader-storage.lock'),('example.a','a/b')])
 outside=root/'outside';outside.write_bytes(b'preserve');target=root/'plugin-data/example.a/link';target.symlink_to(outside);checks['symlinkRefusedAndTargetPreserved']=put('example.a','link',b'bad')==-10 and get('example.a','link')[0]==-10 and outside.read_bytes()==b'preserve';target.unlink()
 target.hardlink_to(outside);checks['hardlinkRefused']=put('example.a','link',b'bad')==-10 and get('example.a','link')[0]==-10 and outside.read_bytes()==b'preserve';target.unlink()
 os.mkfifo(target);checks['fifoReadDoesNotBlock']=get('example.a','link')[0]==-10;target.unlink()
 (root/'plugin-data/example.symlink').symlink_to(root/'plugin-data/example.a',target_is_directory=True);checks['packageSymlinkRefused']=put('example.symlink','state.json',b'bad')==-10 and get('example.a','state.json')[1]==b'A'
 unsafe=root/'plugin-data/example.unsafe';unsafe.mkdir(mode=0o777);unsafe.chmod(0o777);checks['writableDirectoryRefused']=put('example.unsafe','state',b'bad')==-10 and not (unsafe/'state').exists()
 checks['legacyPathPreserved']=legacy(b'legacy',b'compat',6)==0 and (root/'plugins/network-probe/data/legacy').read_bytes()==b'compat'
 lock=root/'plugin-data/example.lock';lock.mkdir(parents=True,mode=0o700);(lock/'.loader-storage.lock').symlink_to(outside);checks['lockSymlinkRefused']=put('example.lock','key',b'x')==-10 and outside.read_bytes()==b'preserve'
 checks['noTemporaryFilesAfterWrites']=not list(root.rglob('.loader-tmp-*'))
 put('example.concurrent','value',b'initial')
 child='import ctypes as c,sys;d=c.CDLL(sys.argv[1]);w=d.loader_plugin_storage_write_scoped;w.argtypes=[c.c_char_p,c.c_char_p,c.c_void_p,c.c_size_t];b=("writer"+sys.argv[2]).encode()*8192;assert all(w(b"example.concurrent",b"value",b,len(b))==0 for _ in range(12))'
 children=[subprocess.Popen([sys.executable,'-c',child,str(library),str(i)]) for i in range(4)];valid={b'initial',*[(('writer'+str(i)).encode()*8192) for i in range(4)]};consistent=True;reads=0;deadline=time.monotonic()+20
 while any(p.poll() is None for p in children) and time.monotonic()<deadline:
  code,value,_=get('example.concurrent','value');consistent&=code==0 and value in valid;reads+=1
 for p in children:p.wait(timeout=5)
 checks['multiprocessAtomicSnapshots']=consistent and reads>0 and all(p.returncode==0 for p in children)
 checks['noCrossNamespaceMutation']=get('example.a','state.json')[1]==b'A' and get('example.b','state.json')[1]==b'B'
 os.environ.pop('OPEN_SV2_LOADER_DATA_DIR');checks['unconfiguredDistinct']=put('example.a','state',b'x')==-4
report={'checks':checks,'passed':all(checks.values()),'scope':'Actual macOS filesystem, isolated plugin namespaces, bounded binary state, no-follow/hardlink/FIFO refusal, cooperative multiprocess atomic snapshots and legacy compatibility. Not a native-plugin security sandbox or account transaction.'};(work/'results.json').write_text(json.dumps(report,indent=2));print(json.dumps(report,indent=2));assert report['passed']
