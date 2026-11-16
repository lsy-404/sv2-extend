from pathlib import Path
import argparse, json, subprocess
p=argparse.ArgumentParser();p.add_argument('--out',type=Path,required=True);p.add_argument('--compiler',default='g++');p.add_argument('--tests',action='store_true');args=p.parse_args()
here=Path(__file__).resolve().parent;repo=here.parents[2];out=args.out.resolve();out.mkdir(parents=True,exist_ok=True)
def run(*args_): subprocess.run([args.compiler,'-std=c++17','-D_WIN32_WINNT=0x0602','-O2','-static','-Wno-misleading-indentation',*map(str,args_)],check=True)
plugin=out/'plugins/sv2.mcp/native/windows-x86_64/plugin.dll';plugin.parent.mkdir(parents=True,exist_ok=True)
run('-shared',here/'mcp_plugin.cpp','-o',plugin,'-ladvapi32','-luser32')
run('-municode',here/'stdio_bridge.cpp','-o',out/'Sv2Mcp.exe','-ladvapi32')
(plugin.parents[2]/'plugin.json').write_text(json.dumps({'schema':1,'id':'sv2.mcp','name':'MCP','version':'0.3.0','nativeLibraries':{'windows-x86_64':'native/windows-x86_64/plugin.dll'}},indent=2),encoding='utf-8')
if args.tests:
 host=out/'network-host';host.mkdir(exist_ok=True)
 run('-municode',repo/'test/windows_pipe_inherit_launcher.cpp','-o',out/'InheritLauncher.exe','-luserenv','-ladvapi32','-lshell32','-lole32','-luuid')
 run('-municode',repo/'test/windows_mcp_fixture.cpp','-o',host/'McpFixture.exe','-lshell32','-lole32','-luuid','-luser32')
 (host/'McpPlugin.dll').write_bytes(plugin.read_bytes())
