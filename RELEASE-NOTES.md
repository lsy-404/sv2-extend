# SV2 Extend 0.9.10

SV2 Extend provides an in-process plugin Loader, realtime MCP, and Lua/JavaScript extensions for Synthesizer V Studio 2.

- Managed host copy and configurable data profiles.
- Dynamic plugin panels, per-plugin settings, installation and update management.
- Realtime note pitch and pitchDelta curve editing.
- Voicebank reading and selection, native WAV export and cancellation.
- Local/online HTTP requests for Lua and JavaScript.
- Versioned C plugin API and scoped plugin storage.

## Platform status

macOS functionality was tested locally with SV2 2.3.0 arm64. The macOS package is universal and ad-hoc signed. Windows x64 includes managed host launch, a native panel, resident MCP and an internal Lua editor backend tested with SV2 2.3.0. Network transport coverage is documented in `docs/NETWORK-STATUS.md`.

## Downloads

- macOS Loader package.
- Windows x64 development package.
- Corresponding source archive.
- Validation summary and SHA256 checksums.

License: GPL-3.0-only. Bundled JSON components retain their MIT notices.
