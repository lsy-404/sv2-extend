#!/bin/sh
set -eu
root=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
app="$root/test/.outputs/Extend UI Check.app"
if [ -e "$app" ]; then echo 'Capture app already exists' >&2; exit 1; fi
mkdir -p "$app/Contents/MacOS"
xcrun swiftc -parse-as-library -warnings-as-errors -target arm64-apple-macos14.0 "$root/test/macos_visual_capture.swift" -o "$app/Contents/MacOS/ExtendUICheck"
cat > "$app/Contents/Info.plist" <<'PLIST'
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0"><dict>
<key>CFBundleIdentifier</key><string>io.opensv2.extend-ui-check</string>
<key>CFBundleExecutable</key><string>ExtendUICheck</string>
<key>CFBundleName</key><string>Extend UI Check</string>
<key>CFBundleDisplayName</key><string>Extend UI Check</string>
<key>CFBundlePackageType</key><string>APPL</string>
<key>LSMinimumSystemVersion</key><string>14.0</string>
<key>NSHighResolutionCapable</key><true/>
</dict></plist>
PLIST
codesign --force --sign - "$app"
codesign --verify --strict "$app"
