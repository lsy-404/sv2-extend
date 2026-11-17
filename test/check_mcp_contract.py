from pathlib import Path
import re

root = Path(__file__).resolve().parents[1]
mac = (root / "plugins/sv2-mcp/mcp_plugin.m").read_text(encoding="utf-8")
windows = (root / "plugins/sv2-mcp/windows/protocol.h").read_text(encoding="utf-8")
mac_rows = re.findall(r'@"name":@"(sv_[a-z_]+)",@"method":@"([a-z._]+)",@"description":@"([^"]+)"', mac)
windows_rows = re.findall(r'\{"([a-z._]+)","(sv_[a-z_]+)","([^"]+)","([^"]+)"\}', windows)
mac_tools = {name: (method, description) for name, method, description in mac_rows}
windows_tools = {name: (method, description) for method, name, _, description in windows_rows}
assert len(mac_tools) == len(mac_rows), "duplicate macOS built-in tool"
assert len(windows_tools) == len(windows_rows), "duplicate Windows built-in tool"
assert mac_tools == windows_tools, "built-in MCP names, methods or descriptions differ"
assert mac_tools.get("sv_project_voicebanks", (None,))[0] == "voicebanks.project"

mac_settings = (root / "loader/macos/plugin_ui.m").read_text(encoding="utf-8")
win_settings = (root / "loader/windows/script_settings.h").read_text(encoding="utf-8")
mac_voice = dict(re.findall(r'@\[@"(voicebanks\.[a-z]+)",@"([^"]+)"', mac_settings))
win_voice = dict(re.findall(r'\{"(voicebanks\.[a-z]+)","([^"]+)"', win_settings))
assert mac_voice == win_voice and "voicebanks.project" in mac_voice, "voice settings differ"
lua = (root / "plugins/lua-extend/lua/sv2_extend.lua").read_text(encoding="utf-8")
js = (root / "plugins/javascript-extend/sv2_extend.js").read_text(encoding="utf-8")
assert "M.voicebanks.project(callback)" in lua and "call('voicebanks.project', {}, callback)" in lua
assert "project: function (callback)" in js and "call('voicebanks.project', {}, callback)" in js
print(f"MCP contract: {len(mac_tools)} built-ins; {len(mac_voice)} voice settings match")
