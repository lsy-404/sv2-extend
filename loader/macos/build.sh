#!/bin/sh
set -eu
export MACOSX_DEPLOYMENT_TARGET=13.0
cd "$(dirname "$0")"
mkdir -p build
python3 - <<'PYJS'
from pathlib import Path
import json
source=Path('../../plugins/javascript-extend/sv2_extend.js').read_text()
Path('build/javascript_bundle.h').write_text('static const char *loaderJavaScriptModule='+json.dumps(source)+';\n')
PYJS
# Swift object lifecycle adapter stays inside the same resident core dylib.
for loader_arch in arm64 x86_64; do
    xcrun swiftc -emit-object -parse-as-library -target "$loader_arch-apple-macos13.0" -import-objc-header swift_network_gate.h swift_network_gate.swift -o "build/swift-network-$loader_arch.o"
done
xcrun lipo -create build/swift-network-arm64.o build/swift-network-x86_64.o -output build/swift-network.o
loader_sdk="$(xcrun --show-sdk-path)"
loader_swift_lib="$(dirname "$(xcrun --find swiftc)")/../lib/swift/macosx"
# Production builds intentionally exclude the superseded /dev/fd editor experiment.
xcrun clang -arch arm64 -arch x86_64 -dynamiclib -fobjc-arc -Wall -Wextra -Werror build/swift-network.o swift_network_trampoline.S probe.c startup_policy.m network_gate.m resident_http.m urlsession_callbacks.m plugin_storage.m socket_gate.c network_framework_gate.m webkit_gate.m webkit_rtc_gate.m web_content_retirement.m web_store_registry.m website_data_namespace.m host_data_namespace.m editor_api.m plugin_ui.m plugin_packages.m plugin_manager.m extend_control_view.m extend_category.mm local_json_server.m lua_call_gate.m lua_editor_service.m lua_capability_service.m lua_editor_autostart.m http_capability.m javascript_gate.m voice_return_trampoline.S voice_host_adapter.mm voice_catalog_native.mm export_host_adapter.mm -framework Foundation -framework AppKit -framework WebKit -framework Network -framework UniformTypeIdentifiers -larchive -lcurl -lc++ -L "$loader_sdk/usr/lib/swift" -L "$loader_swift_lib" -Wl,-rpath,/usr/lib/swift -o build/loader-probe.dylib
# Replace atomically: never truncate a dylib mapped by a running research host.
mv build/loader-probe.dylib loader-probe.dylib
xcrun clang -arch arm64 -arch x86_64 -dynamiclib -fobjc-arc -Wall -Wextra -Werror network_plugin.m -framework Foundation -framework AppKit -o build/network-plugin.dylib
mv build/network-plugin.dylib network-plugin.dylib
xcrun clang -arch arm64 -arch x86_64 -dynamiclib -Wall -Wextra -Werror plugin.c -o build/sample-plugin.dylib
mv build/sample-plugin.dylib sample-plugin.dylib
xcrun clang -arch arm64 -arch x86_64 -Wall -Wextra -Werror launcher.c -framework CoreFoundation -o build/loader-launcher
mv build/loader-launcher loader-launcher
xcrun clang -Wall -Wextra -Werror fixture.c -o build/fixture
mv build/fixture fixture
xcrun clang -arch arm64 -arch x86_64 -Wall -Wextra -Werror sideloader.c -o build/sideloader
mv build/sideloader sideloader
xcrun clang -arch arm64 -arch x86_64 -Wall -Wextra -Werror task-port-probe.c -o build/task-port-probe
mv build/task-port-probe task-port-probe
xcrun clang -arch arm64 -arch x86_64 -dynamiclib -fobjc-arc -Wall -Wextra -Werror coverage_plugin.m -framework Foundation -framework AppKit -framework WebKit -o build/coverage-plugin.dylib
mv build/coverage-plugin.dylib coverage-plugin.dylib

xcrun clang -arch arm64 -arch x86_64 -dynamiclib -fobjc-arc -Wall -Wextra -Werror ../../plugins/sv2-mcp/mcp_plugin.m -framework Foundation -o build/sv2-mcp.dylib
mv build/sv2-mcp.dylib sv2-mcp.dylib

# Diagnostic fixture is built separately and is never included in Loader.app.
xcrun clang -arch arm64 -arch x86_64 -dynamiclib -fobjc-arc -Wall -Wextra -Werror network_framework_fixture.m -framework Foundation -framework Network -o build/network-framework-plugin.dylib
mv build/network-framework-plugin.dylib network-framework-plugin.dylib

xcrun clang -arch arm64 -arch x86_64 -dynamiclib -fobjc-arc -Wall -Wextra -Werror urlsession_callback_fixture.m -framework Foundation -o build/urlsession-callback-plugin.dylib
mv build/urlsession-callback-plugin.dylib urlsession-callback-plugin.dylib

# Synthetic website storage fixture requires the public macOS 14 profile API.
xcrun clang -arch arm64 -arch x86_64 -dynamiclib -fobjc-arc -Wall -Wextra -Werror -mmacosx-version-min=14.0 website_data_fixture.m -framework Foundation -framework WebKit -o build/website-data-plugin.dylib
mv build/website-data-plugin.dylib website-data-plugin.dylib

# Independent socket / Network / WKWebView regression fixture; never shipped in Loader.app.
xcrun clang -arch arm64 -arch x86_64 -dynamiclib -fobjc-arc -Wall -Wextra -Werror transport_matrix_plugin.m -framework Foundation -framework WebKit -framework Network -framework Security -o build/transport-matrix-plugin.dylib
mv build/transport-matrix-plugin.dylib transport-matrix-plugin.dylib
