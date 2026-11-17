"""Generate a test-only Core source; compile with the verified native category binding."""
import argparse
from pathlib import Path

parser = argparse.ArgumentParser()
parser.add_argument('--core', type=Path, required=True)
parser.add_argument('--output', type=Path, required=True)
args = parser.parse_args()
test_root = Path(__file__).resolve().parent
output = args.output.resolve()
if not output.is_relative_to(test_root) or output == test_root:
    raise ValueError('The generated source must be inside the repository test directory')
source = args.core.read_text(encoding='utf-8')
entry = 'extern "C" __declspec(dllexport) int ExtendInitialize(void*){'
registration = 'load_packages();startup_receipt();'
if source.count(entry) != 1 or source.count(registration) != 1:
    raise ValueError('The Core source does not match the test injection points')
source = source.replace(entry, '#include "windows_category_probe.h"\n' + entry)
source = source.replace(registration, 'load_packages();if(!category_probe::install(api))return LOADER_ERR_INVALID;startup_receipt();')
output.parent.mkdir(parents=True, exist_ok=True)
output.write_text(source, encoding='utf-8', newline='\n')
