"""Prepare test-only voice scripts that report through the parity probe."""
import argparse
from pathlib import Path

parser = argparse.ArgumentParser()
parser.add_argument('--out', type=Path, required=True)
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
args.out.mkdir(parents=True, exist_ok=True)
for path, _ in outputs:
    if path.exists():
        raise FileExistsError(path)
for path, body in outputs:
    with path.open('x', encoding='utf-8', newline='\n') as file:
        file.write(body)
print('Prepared Lua and JavaScript test scripts')
