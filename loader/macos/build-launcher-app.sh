#!/bin/sh
set -eu
source_dir="$(CDPATH= cd -- "$(dirname "$0")" && pwd)"
app_dir="${1:?Pass an absolute destination ending in .app}"
case "$app_dir" in /*.app) ;; *) echo 'Destination must be an absolute .app path' >&2; exit 64 ;; esac
if [ -e "$app_dir" ]; then echo 'Destination already exists; preserve it and choose a new destination.' >&2; exit 1; fi
sh "$source_dir/build.sh"
mkdir -p "$source_dir/build" "$app_dir/Contents/MacOS" "$app_dir/Contents/Resources/Native"
cp "$source_dir/archive_reader_abi.h" "$app_dir/Contents/Resources/Libarchive-Notice.txt"
xcrun swiftc -O -target arm64-apple-macos13.0 "$source_dir/data_launcher.swift" -o "$source_dir/build/data-launcher-arm64"
xcrun swiftc -O -target x86_64-apple-macos13.0 "$source_dir/data_launcher.swift" -o "$source_dir/build/data-launcher-x86_64"
xcrun lipo -create "$source_dir/build/data-launcher-arm64" "$source_dir/build/data-launcher-x86_64" -output "$app_dir/Contents/MacOS/ExtendLoader"
cp "$source_dir/loader-launcher" "$source_dir/loader-probe.dylib" "$source_dir/network-plugin.dylib" "$source_dir/sv2-mcp.dylib" "$app_dir/Contents/Resources/Native/"
mkdir -p "$app_dir/Contents/Resources/Native/Lua"
cp "$source_dir/../../plugins/lua-extend/lua/"*.lua "$app_dir/Contents/Resources/Native/Lua/"
mkdir -p "$app_dir/Contents/Resources/Native/JavaScript"
cp "$source_dir/../../plugins/javascript-extend/sv2_extend.js" "$app_dir/Contents/Resources/Native/JavaScript/"
cat > "$app_dir/Contents/Info.plist" <<'PLIST'
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0"><dict>
<key>CFBundleIdentifier</key><string>io.opensv2.extend-loader</string>
<key>CFBundleExecutable</key><string>ExtendLoader</string>
<key>CFBundleName</key><string>Extend Loader</string>
<key>CFBundleDisplayName</key><string>Extend Loader</string>
<key>CFBundleShortVersionString</key><string>0.9.10</string>
<key>CFBundleVersion</key><string>27</string>
<key>CFBundlePackageType</key><string>APPL</string>
<key>LSMinimumSystemVersion</key><string>13.0</string>
<key>NSHighResolutionCapable</key><true/>
</dict></plist>
PLIST
cp "$source_dir/../../LICENSE" "$app_dir/Contents/Resources/LICENSE.txt"
cp "$source_dir/../../NOTICE" "$app_dir/Contents/Resources/NOTICE.txt"
/usr/bin/codesign --force --sign - "$app_dir"
/usr/bin/codesign --verify --deep --strict "$app_dir"
