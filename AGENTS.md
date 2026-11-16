# SV2 Extend repository rules

- This is the public monorepo for Loader, MCP, Lua Extend and JavaScript Extend. Keep vendor authentication and account/session implementations outside this repository.
- Plugins use the versioned Loader API. MCP and script extensions use the public editor and runtime capabilities.
- Production runtimes run inside SV2. The launcher starts the managed host and does not leave an external resident manager. Editor operations must not use the old Bridge file/pipe protocol.
- Preserve the official SV2 installation, user projects and original account/data files. Before an upgrade, back up the installed application and relevant configuration; preserve existing migration targets.
- Build success is not installed delivery. After a validated change, install it and verify the installed entry, runtime process ownership and absence of an external resident manager. Report an uninstalled candidate as uninstalled.
- Network coverage flags and isolation receipts must reflect actual scope. Do not remove an unverified flag or report full isolation based only on tracked counters draining, synthetic callbacks or a narrower diagnostic passing.
- Do not commit official host binaries, managed host copies, account/session data, personal paths, credentials, raw runtime logs or local verification outputs. Keep reproducible source fixtures separate from shipped application resources.
- The former private-repo source copies are migration backups. Make public implementation changes here; do not silently update both trees.
- Preserve the original host's window titles, application/menu display names and icon. Extend owns its added categories only; do not rebrand the host as SV2 Extended or append diagnostic titles. The separate launcher retains its own Extend Loader name.
