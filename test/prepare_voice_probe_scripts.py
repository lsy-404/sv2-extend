"""Prepare test-only voice scripts that report through the parity probe."""
import argparse
import json
from pathlib import Path

parser = argparse.ArgumentParser()
parser.add_argument('--out', type=Path, required=True)
parser.add_argument('--export-lua-path', type=Path)
parser.add_argument('--export-js-path', type=Path)
args = parser.parse_args()
source = Path(__file__).resolve().parent
replacements = {
    'lua': ("SV:print('EXTEND_LUA_VOICE_VERIFICATION '..J.encode(checks))", "E.request('editor.verification_report', checks)"),
    'js': ("SV.print('EXTEND_JS_VOICE_VERIFICATION '+JSON.stringify(checks))", "E.request('editor.verification_report', checks)"),
}
outputs = []
for language, (old, new) in replacements.items():
    body = (source / f'script_voice_live.{language}').read_text(encoding='utf-8')
    if body.count(old) != 1:
        raise RuntimeError('Unexpected verification script format')
    path = args.out / f'extend-voice-{language}.{language}'
    outputs.append((path, body.replace(old, new)))
if bool(args.export_lua_path) != bool(args.export_js_path):
    raise ValueError('Both export paths must be supplied')
if args.export_lua_path:
    lua_path, js_path = args.export_lua_path.resolve(), args.export_js_path.resolve()
    if lua_path == js_path or lua_path.exists() or js_path.exists():
        raise ValueError('Export paths must be distinct and unoccupied')
    for language, output in [('lua', lua_path), ('js', js_path)]:
        body = (source / f'script_export_live.{language}').read_text(encoding='utf-8')
        if language == 'lua':
            old_report = "SV:print('EXTEND_LUA_EXPORT_VERIFICATION '..J.encode(checks))"
            old_path = "os.getenv('OPEN_SV2_TEST_EXPORT_PATH')"
        else:
            old_report = "SV.print('EXTEND_JS_EXPORT_VERIFICATION '+JSON.stringify(checks))"
            old_path = "typeof TEST_OUTPUT_PATH==='undefined'?null:TEST_OUTPUT_PATH"
        if body.count(old_report) != 1 or body.count(old_path) != 1:
            raise RuntimeError('Unexpected export verification script format')
        body = body.replace(old_report, "E.request('editor.verification_report', checks)")
        body = body.replace(old_path, json.dumps(str(output), ensure_ascii=False))
        outputs.append((args.out / f'extend-export-{language}.{language}', body))
args.out.mkdir(parents=True, exist_ok=True)
for path, _ in outputs:
    if path.exists():
        raise FileExistsError(path)
for path, body in outputs:
    with path.open('x', encoding='utf-8', newline='\n') as file:
        file.write(body)
print('Prepared Lua and JavaScript test scripts')
