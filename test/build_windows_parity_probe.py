"""Build test-only runtime instrumentation without changing the production build."""
from pathlib import Path
import argparse,subprocess,json
parser=argparse.ArgumentParser();parser.add_argument('--build',type=Path,required=True);parser.add_argument('--out',type=Path,required=True);args=parser.parse_args()
repo=Path(__file__).resolve().parent.parent;source=repo/'loader/windows';out=args.out.resolve();assert out.is_relative_to(repo/'test') and out!=repo/'test';out.mkdir(parents=True,exist_ok=True)
def replace_once(text,before,after):
 if text.count(before)!=1:raise RuntimeError('instrumentation target missing or ambiguous: '+before)
 return text.replace(before,after,1)
panel=replace_once((source/'native_panel.h').read_text(encoding='utf-8'),'struct Control {','static uint64_t verification_serial=0;\nstruct Control {\n uint64_t verification_identity=++verification_serial;')
(out/'native_panel.h').write_text(panel,encoding='utf-8')
module=(repo/'plugins/javascript-extend/sv2_extend.js').read_text(encoding='utf-8')
(out/'javascript_bundle.h').write_text('static const char loaderJavaScriptModule[]='+json.dumps(module)+';\n',encoding='utf-8')
core=(source/'core.cpp').read_text(encoding='utf-8')
core=replace_once(core,'#include "editor_autostart.h"','#include "editor_autostart.h"\n#include "windows_parity_probe.h"')
core=replace_once(core,'editor_gate::set_router(script_call)','editor_gate::set_router(parity_probe::route)')
core=replace_once(core,'load_packages();startup_receipt();','voice_error_probe::install(main_thread);load_packages();parity_probe::install(api,dispatcher);startup_receipt();')
core += '\nnamespace render_probe = export_native;\n' + 'namespace editor_autostart {nlohmann::json render_call(const std::string &op,const nlohmann::json &args){if(op=="configuration")return render_probe::configuration();if(op=="configure_path")return render_probe::configure_path(args.value("staging",std::string()));if(op=="path_status")return render_probe::path_status();if(op=="restore_path")return render_probe::restore_path();if(op=="finish_path")return render_probe::finish_path();if(op=="configure_format")return render_probe::configure_format(args.value("channels",1),args.value("rate",44100),args.value("bits",16));if(op=="format_status")return render_probe::format_status();if(op=="restore_format")return render_probe::restore_format();if(op=="finish_format")return render_probe::finish_format();if(op=="trigger")return render_probe::trigger();return {{"error","INVALID_OPERATION"}};} nlohmann::json render_status(){return render_probe::status();} nlohmann::json render_request(){return render_probe::request();}}'
(out/'core.cpp').write_text(core,encoding='utf-8')
autostart=(source/'editor_autostart.cpp').read_text(encoding='utf-8')+'\nnamespace editor_autostart { bool run_verification_script(const wchar_t *path){if(!path||!resident||GetCurrentThreadId()!=resident->thread||!liveRoot.load()||!manager||!run)return false;wchar_t full[32768];DWORD size=GetFullPathNameW(path,32768,full,nullptr);if(!size||size>=32768)return false;auto previous=controlled;controlled=full;void *item=nullptr;bool matched=find(nullptr,false,&item);controlled=std::move(previous);if(!matched)return false;run(item);return true;}}'
(out/'autostart.cpp').write_text(autostart,encoding='utf-8')
flags=['-std=c++17','-D_WIN32_WINNT=0x0602','-O2','-Wall','-Wextra','-Werror','-Wno-cast-function-type','-Wno-misleading-indentation','-static','-static-libgcc','-static-libstdc++','-I'+str(source),'-I'+str(repo/'test'),'-I'+str(args.build.resolve())]
objects=[args.build/('minhook-'+name+'.o') for name in ['buffer','hook','trampoline','hde64']]
subprocess.run(['g++',*flags,'-shared',str(out/'core.cpp'),str(source/'editor_gate.cpp'),str(out/'autostart.cpp'),str(source/'javascript_gate.cpp'),str(source/'voice_adapter.cpp'),*map(str,objects),'-o',str(out/'ExtendCore.dll'),'-luser32','-lgdi32','-ladvapi32','-lshell32','-lole32','-loleaut32','-liphlpapi','-lws2_32','-luuid','-lbcrypt','-Wl,--dynamicbase,--nxcompat'],check=True)
print('Built test-only parity Core')
