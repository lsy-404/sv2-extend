"""Check the isolated host entry probe receipt."""
import argparse
import json
from pathlib import Path
import time

p = argparse.ArgumentParser()
p.add_argument("--profile", type=Path, required=True)
a = p.parse_args()
receipt = a.profile / "outputs" / "scripts-probe.json"
deadline = time.monotonic() + 30
while not receipt.exists():
    if time.monotonic() >= deadline:
        raise TimeoutError("Entry probe receipt was not produced")
    time.sleep(.1)
result = json.loads(receipt.read_text(encoding="utf-8"))
assert result["scriptsEntries"] == 1, result
assert result["separatorAlpha"] == 64 and result["separatorWidth"] > 0 and result["separatorHeight"] > 0, result
for name in ("separatorInBounds", "separatorDrawn", "scriptsCaption", "rootCaption", "rootOpened", "hiddenSaved", "hiddenRegistry", "nativeButton", "shortHeightHidden",
             "restoredSaved", "restoredRegistry", "rootOpenKnown", "rootOpen", "toggleValue", "toggleChecked"):
    assert result[name] is True, result
if result["hiddenExtensions"] == 0:
    assert result["hiddenNoDraw"], result
print(json.dumps(result))
