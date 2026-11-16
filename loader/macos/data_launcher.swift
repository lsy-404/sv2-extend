import Foundation
import AppKit
import CryptoKit
import Darwin

struct LoaderSettings: Codable, Equatable {
    var schema = 1
    var sourceApplication: String
    var dataMode: String
    var dataDirectory: String
    var scriptHTTPLocal: Bool?
    var scriptHTTPOnline: Bool?
    var hostDataNamespace: Bool?
    var isolateHostData: Bool { hostDataNamespace ?? false }
    var allowScriptHTTPLocal: Bool { scriptHTTPLocal ?? true }
    var allowScriptHTTPOnline: Bool { scriptHTTPOnline ?? true }
}
struct RuntimeReceipt: Codable {
    var fingerprint: String
    var sourceApplication: String
    var sourceVersion: String
    var appDirectory: String
    var createdAt: String
    var hostBundleIdentifier: String?
}
struct LoaderError: LocalizedError {
    let message: String
    var errorDescription: String? { message }
}
let fm = FileManager.default
let defaults = UserDefaults.standard
let home = fm.homeDirectoryForCurrentUser
let originalData = home.appendingPathComponent("Library/Application Support/Dreamtonics/Synthesizer V Studio 2")
let alongsideData = originalData.deletingLastPathComponent().appendingPathComponent("Open SV2 Loader")
let arguments = Array(CommandLine.arguments.dropFirst())
func canonical(_ path: String) -> URL {
    URL(fileURLWithPath: (path as NSString).expandingTildeInPath).standardizedFileURL.resolvingSymlinksInPath()
}
func child(_ item: URL, of directory: URL) -> Bool {
    item.path == directory.path || item.path.hasPrefix(directory.path + "/")
}
func run(_ executable: String, _ args: [String]) throws -> String {
    let process = Process(); let output = Pipe()
    process.executableURL = URL(fileURLWithPath: executable); process.arguments = args
    process.standardOutput = output; process.standardError = output
    try process.run()
    let data = output.fileHandleForReading.readDataToEndOfFile(); process.waitUntilExit()
    let text = String(decoding: data, as: UTF8.self)
    guard process.terminationStatus == 0 else { throw LoaderError(message: "\(executable): \(text)") }
    return text
}
func writeJSON<T: Encodable>(_ value: T, to path: URL) throws {
    let encoder = JSONEncoder(); encoder.outputFormatting = [.prettyPrinted, .sortedKeys]
    try encoder.encode(value).write(to: path, options: .atomic)
}
func currentSettings() throws -> LoaderSettings {
    let selected = defaults.string(forKey: "loaderDataDirectory") ?? alongsideData.path
    let path = canonical(selected).appendingPathComponent("settings.json")
    if fm.fileExists(atPath: path.path) { return try JSONDecoder().decode(LoaderSettings.self, from: Data(contentsOf: path)) }
    return LoaderSettings(sourceApplication: "/Applications/Synthesizer V Studio 2 Pro.app", dataMode: canonical(selected) == canonical(alongsideData.path) ? "alongside-original-data" : "independent", dataDirectory: selected, hostDataNamespace: true)
}
func validate(_ settings: LoaderSettings) throws {
    guard ["alongside-original-data", "independent"].contains(settings.dataMode) else { throw LoaderError(message: "Unknown data directory mode.") }
    let source = canonical(settings.sourceApplication); let root = canonical(settings.dataDirectory)
    guard !child(root, of: source), !child(root, of: originalData), root.path != home.path, root.path != "/" else {
        throw LoaderError(message: "Choose a separate Loader data folder. The official application and original data folder are preserved.")
    }
    guard fm.fileExists(atPath: source.appendingPathComponent("Contents/Info.plist").path) else { throw LoaderError(message: "The official SV2 application was not found.") }
}
func saveSettings(_ settings: LoaderSettings, previous: LoaderSettings) throws {
    try validate(settings)
    let root = canonical(settings.dataDirectory); let old = canonical(previous.dataDirectory)
    let file = root.appendingPathComponent("settings.json")
    if root == old, let data = try? Data(contentsOf: file), let existing = try? JSONDecoder().decode(LoaderSettings.self, from: data), existing == settings {
        defaults.set(root.path, forKey: "loaderDataDirectory"); return
    }
    if root != old && (child(root, of: old) || child(old, of: root)) { throw LoaderError(message: "Use separate data folders, rather than nesting one inside the other.") }
    if root != old && fm.fileExists(atPath: file.path) {
        // Selecting an existing profile activates it without merging other data.
        let existing = try JSONDecoder().decode(LoaderSettings.self, from: Data(contentsOf: file))
        guard existing.schema == 1, canonical(existing.dataDirectory) == root,
              canonical(existing.sourceApplication) == canonical(settings.sourceApplication), existing.dataMode == settings.dataMode else {
            throw LoaderError(message: "The destination has different Loader settings. Existing data was preserved.")
        }
        defaults.set(root.path, forKey: "loaderDataDirectory"); return
    }
    // Copy user-owned Loader data, keeping the previous directory intact. Runtime copies are regenerated.
    if root != old {
        let migratable = ["plugins", "plugin-data", "plugin-pending", "plugin-backups", "plugin-transactions", "plugin-removals", "preferences", "logs", "cache", "host-home"]
        for name in migratable where fm.fileExists(atPath: old.appendingPathComponent(name).path) {
            guard !fm.fileExists(atPath: root.appendingPathComponent(name).path) else { throw LoaderError(message: "Migration destination already contains \(name); nothing was overwritten.") }
        }
        try fm.createDirectory(at: root, withIntermediateDirectories: true)
        for name in migratable where fm.fileExists(atPath: old.appendingPathComponent(name).path) {
            try fm.copyItem(at: old.appendingPathComponent(name), to: root.appendingPathComponent(name))
        }
    }
    try fm.createDirectory(at: root, withIntermediateDirectories: true)
    if fm.fileExists(atPath: file.path) {
        let backups = root.appendingPathComponent("backups"); try fm.createDirectory(at: backups, withIntermediateDirectories: true)
        try fm.copyItem(at: file, to: backups.appendingPathComponent("settings-\(UUID().uuidString).json"))
    }
    try writeJSON(settings, to: file)
    defaults.set(root.path, forKey: "loaderDataDirectory")
}
func assetDirectory() throws -> URL {
    guard let resource = Bundle.main.resourceURL else { throw LoaderError(message: "Launcher resources are missing.") }
    return resource.appendingPathComponent("Native")
}
let luaResourceNames = ["sv2_extend.lua", "sv2_extend_json.lua", "editor_provider.lua"]
func installLuaResources(_ root: URL, assets: URL, hostData: URL? = nil) throws {
    let directory = root.appendingPathComponent("plugins/lua-extend/lua")
    guard child(canonical(directory.path), of: root) else { throw LoaderError(message: "Lua extension directory resolves outside the Loader data folder.") }
    try fm.createDirectory(at: directory, withIntermediateDirectories: true, attributes: [.posixPermissions: 0o700])
    func install(_ data: Data, to target: URL, owned: Bool = true) throws {
        if let previous = try? Data(contentsOf: target) {
            if previous == data { return }
            guard owned else { throw LoaderError(message: "An existing script uses the Loader entry filename. It was preserved: \(target.path)") }
            let backups = root.appendingPathComponent("backups/lua-extend/\(UUID().uuidString)")
            try fm.createDirectory(at: backups, withIntermediateDirectories: true)
            try fm.copyItem(at: target, to: backups.appendingPathComponent(target.lastPathComponent))
        }
        try data.write(to: target, options: .atomic)
        try fm.setAttributes([.posixPermissions: 0o600], ofItemAtPath: target.path)
    }
    for name in luaResourceNames { try install(Data(contentsOf: assets.appendingPathComponent("Lua/\(name)")), to: directory.appendingPathComponent(name)) }
    let jsDirectory = root.appendingPathComponent("plugins/javascript-extend/js")
    guard child(canonical(jsDirectory.path), of: root) else { throw LoaderError(message: "JavaScript extension directory resolves outside the Loader data folder.") }
    try fm.createDirectory(at: jsDirectory, withIntermediateDirectories: true, attributes: [.posixPermissions: 0o700])
    try install(Data(contentsOf: assets.appendingPathComponent("JavaScript/sv2_extend.js")), to: jsDirectory.appendingPathComponent("sv2_extend.js"))
    let scripts = (hostData ?? originalData).appendingPathComponent("scripts")
    try fm.createDirectory(at: scripts, withIntermediateDirectories: true)
    let target = scripts.appendingPathComponent("OpenSV2LoaderRuntime.lua")
    let marker = "-- Open SV2 Loader managed Lua Extend entry."
    let stub = """
    -- Open SV2 Loader managed Lua Extend entry.
    function getClientInfo() return {name='Open SV2 Loader Runtime',author='Open SV2 Loader',category='Open SV2 Loader',versionNumber=2,minEditorVersion=131330} end
    function main()
      local root=os.getenv('OPEN_SV2_LOADER_DATA_DIR')
      if not root then SV:showMessageBox('Open SV2 Loader','Start the editor through Extend Loader.');SV:finish();return end
      package.path=root..'/plugins/lua-extend/lua/?.lua;'..package.path
      dofile(root..'/plugins/lua-extend/lua/editor_provider.lua')
    end
    """ + "\n"
    let previous = (try? String(contentsOf: target, encoding: .utf8))
    try install(Data(stub.utf8), to: target, owned: previous == nil || previous!.hasPrefix(marker))
    let controlTarget = scripts.appendingPathComponent("OpenSV2ExtendControl.lua")
    let controlMarker = "-- Open SV2 Loader managed Extend control sidebar."
    if let previousControl = try? String(contentsOf: controlTarget, encoding: .utf8), previousControl.hasPrefix(controlMarker) {
        // Preserve the superseded Scripts-section entry while removing its .lua suffix.
        try fm.moveItem(at: controlTarget, to: controlTarget.deletingLastPathComponent().appendingPathComponent("OpenSV2ExtendControl.retired-" + UUID().uuidString))
    }
}
func validateHostDataTree(_ directory: URL) throws {
    if (try directory.resourceValues(forKeys: [.isSymbolicLinkKey])).isSymbolicLink == true { throw LoaderError(message: "Host data contains a symbolic link; the source and existing destination were preserved.") }
    if let entries = fm.enumerator(at: directory, includingPropertiesForKeys: [.isSymbolicLinkKey]) {
        for case let item as URL in entries {
            if (try item.resourceValues(forKeys: [.isSymbolicLinkKey])).isSymbolicLink == true { throw LoaderError(message: "Host data contains a symbolic link; the source and existing destination were preserved.") }
        }
    }
}
func prepareHostData(_ root: URL) throws -> URL {
    let homeDirectory = root.appendingPathComponent("host-home")
    let relative = "Library/Application Support/Dreamtonics/Synthesizer V Studio 2"
    let marker = homeDirectory.appendingPathComponent(".loader-host-data.json")
    if fm.fileExists(atPath: homeDirectory.path) {
        guard canonical(homeDirectory.path) == homeDirectory,
              let object = try JSONSerialization.jsonObject(with: Data(contentsOf: marker)) as? [String: Any],
              object["kind"] as? String == "OpenSV2.host-data.v1", object["schema"] as? Int == 1,
              fm.fileExists(atPath: homeDirectory.appendingPathComponent(relative).path) else { throw LoaderError(message: "Existing host data was not prepared by this Loader; it was preserved.") }
        try validateHostDataTree(homeDirectory)
        return homeDirectory.appendingPathComponent(relative)
    }
    let staging = root.appendingPathComponent(".host-data-prepare-\(UUID().uuidString)")
    try fm.createDirectory(at: staging, withIntermediateDirectories: true, attributes: [.posixPermissions: 0o700])
    let product = staging.appendingPathComponent(relative)
    try fm.createDirectory(at: product, withIntermediateDirectories: true, attributes: [.posixPermissions: 0o700])
    // Opaque application-data migration; no authentication fields are interpreted.
    for name in ["settings", "databases", "dicts", "scripts", "server-translations", "license"] {
        let source = originalData.appendingPathComponent(name), destination = product.appendingPathComponent(name)
        if fm.fileExists(atPath: source.path) {
            try validateHostDataTree(source)
            _ = try run("/bin/cp", ["-cR", source.path, destination.path])
        }
    }
    try validateHostDataTree(staging)
    if let entries = fm.enumerator(at: staging, includingPropertiesForKeys: [.isDirectoryKey, .isRegularFileKey]) {
        for case let item as URL in entries {
            let values = try item.resourceValues(forKeys: [.isDirectoryKey, .isRegularFileKey])
            if values.isDirectory == true { try fm.setAttributes([.posixPermissions: 0o700], ofItemAtPath: item.path) }
            else if values.isRegularFile == true {
                let mode = (try fm.attributesOfItem(atPath: item.path)[.posixPermissions] as? NSNumber)?.intValue ?? 0
                try fm.setAttributes([.posixPermissions: 0o600 | (mode & 0o100)], ofItemAtPath: item.path)
            }
        }
    }
    let receipt: [String: Any] = ["kind": "OpenSV2.host-data.v1", "schema": 1, "sourceDirectory": originalData.path, "createdAt": ISO8601DateFormatter().string(from: Date())]
    try JSONSerialization.data(withJSONObject: receipt, options: [.sortedKeys]).write(to: staging.appendingPathComponent(".loader-host-data.json"), options: .atomic)
    try fm.setAttributes([.posixPermissions: 0o600], ofItemAtPath: staging.appendingPathComponent(".loader-host-data.json").path)
    // The final directory must be absent; interrupted staging directories survive.
    try fm.moveItem(at: staging, to: homeDirectory)
    return homeDirectory.appendingPathComponent(relative)
}
func storageBundleIdentifier(root: URL, sourceIdentifier: String) -> String {
    let key = "OpenSV2.HostStorage.v1\n" + sourceIdentifier + "\n" + canonical(root.path).path
    return "io.opensv2.extended-host.p" + SHA256.hash(data: Data(key.utf8)).map { String(format: "%02x", $0) }.joined()
}
func prepareRuntime(_ settings: LoaderSettings) throws -> RuntimeReceipt {
    try validate(settings)
    let source = canonical(settings.sourceApplication); let root = canonical(settings.dataDirectory)
    _ = try run("/usr/bin/codesign", ["--verify", "--strict", source.path])
    let signature = try run("/usr/bin/codesign", ["-dv", "--verbose=4", source.path])
    guard signature.contains("TeamIdentifier=9DV6HJ52BP") else { throw LoaderError(message: "The source application is not signed by Dreamtonics.") }
    let plistURL = source.appendingPathComponent("Contents/Info.plist")
    guard let info = try PropertyListSerialization.propertyList(from: Data(contentsOf: plistURL), format: nil) as? [String: Any],
          let executable = info["CFBundleExecutable"] as? String, !executable.contains("/") else { throw LoaderError(message: "Invalid SV2 bundle metadata.") }
    let version = info["CFBundleShortVersionString"] as? String ?? "unknown"
    guard let sourceIdentifier = info["CFBundleIdentifier"] as? String, !sourceIdentifier.isEmpty else { throw LoaderError(message: "The source has no application identifier.") }
    let isolatedHostData = settings.isolateHostData
    let experimentalIdentity = arguments.contains("--experimental-storage-identity") || isolatedHostData
    let hostIdentifier = experimentalIdentity ? storageBundleIdentifier(root: root, sourceIdentifier: sourceIdentifier) : "io.opensv2.extended-host"
    let assets = try assetDirectory()
    var digest = SHA256()
    let fingerprintFiles = [source.appendingPathComponent("Contents/MacOS/\(executable)"), plistURL, source.appendingPathComponent("Contents/_CodeSignature/CodeResources"), assets.appendingPathComponent("loader-launcher"), assets.appendingPathComponent("loader-probe.dylib"), assets.appendingPathComponent("network-plugin.dylib"), assets.appendingPathComponent("sv2-mcp.dylib"), assets.appendingPathComponent("JavaScript/sv2_extend.js")] + luaResourceNames.map { assets.appendingPathComponent("Lua/\($0)") }
    for file in fingerprintFiles {
        digest.update(data: try Data(contentsOf: file, options: .mappedIfSafe))
    }
    if experimentalIdentity { digest.update(data: Data(("OpenSV2.runtime-layout.v2\n" + hostIdentifier + "\n" + String(isolatedHostData)).utf8)) }
    digest.update(data: Data("OpenSV2.preserve-host-presentation.v1\n".utf8))
    let hostDisplayName = (info["CFBundleDisplayName"] as? String) ?? (info["CFBundleName"] as? String) ?? source.deletingPathExtension().lastPathComponent
    digest.update(data: Data(("OpenSV2.script-http-policy.v1\n" + String(settings.allowScriptHTTPLocal) + "\n" + String(settings.allowScriptHTTPOnline)).utf8))
    let fingerprint = digest.finalize().map { String(format: "%02x", $0) }.joined()
    let runtimes = root.appendingPathComponent("runtimes")
    let destination = runtimes.appendingPathComponent(String(fingerprint.prefix(24)))
    let receiptFile = destination.appendingPathComponent("receipt.json")
    if fm.fileExists(atPath: receiptFile.path), let receipt = try? JSONDecoder().decode(RuntimeReceipt.self, from: Data(contentsOf: receiptFile)), receipt.fingerprint == fingerprint {
        _ = try run("/usr/bin/codesign", ["--verify", "--deep", "--strict", receipt.appDirectory])
        let cachedInfo = try PropertyListSerialization.propertyList(from: Data(contentsOf: URL(fileURLWithPath: receipt.appDirectory).appendingPathComponent("Contents/Info.plist")), format: nil) as? [String: Any]
        guard cachedInfo?["CFBundleIdentifier"] as? String == hostIdentifier,
              cachedInfo?["CFBundleName"] as? String == info["CFBundleName"] as? String,
              cachedInfo?["CFBundleDisplayName"] as? String == hostDisplayName,
              cachedInfo?["OpenSV2ScriptHTTPLocal"] as? Bool == settings.allowScriptHTTPLocal,
              cachedInfo?["OpenSV2ScriptHTTPOnline"] as? Bool == settings.allowScriptHTTPOnline,
              !experimentalIdentity || (receipt.hostBundleIdentifier == hostIdentifier && cachedInfo?["OpenSV2LoaderDataDirectory"] as? String == root.path && cachedInfo?["OpenSV2HostDataNamespace"] as? Bool == isolatedHostData) else { throw LoaderError(message: "The cached runtime belongs to another storage identity. It was preserved.") }
        return receipt
    }
    guard !fm.fileExists(atPath: destination.path) else { throw LoaderError(message: "An incomplete runtime already exists. It was preserved for inspection.") }
    try fm.createDirectory(at: runtimes, withIntermediateDirectories: true)
    let staging = runtimes.appendingPathComponent(".prepare-\(UUID().uuidString)")
    try fm.createDirectory(at: staging, withIntermediateDirectories: true)
    let app = staging.appendingPathComponent("SV2 Extended.app")
    _ = try run("/usr/bin/ditto", [source.path, app.path])
    let macOS = app.appendingPathComponent("Contents/MacOS")
    try fm.moveItem(at: macOS.appendingPathComponent(executable), to: macOS.appendingPathComponent("synthv-studio-original"))
    var edited = info
    edited["CFBundleIdentifier"] = hostIdentifier
    // Keep the vendor's menu/window identity. The managed cache filename and
    // storage bundle identifier are internal details, not user-facing branding.
    edited["CFBundleDisplayName"] = hostDisplayName
    edited["CFBundleExecutable"] = "loader-launcher"
    edited["OpenSV2LoaderDataDirectory"] = root.path
    edited["OpenSV2HostDataNamespace"] = isolatedHostData
    edited["OpenSV2ScriptHTTPLocal"] = settings.allowScriptHTTPLocal
    edited["OpenSV2ScriptHTTPOnline"] = settings.allowScriptHTTPOnline
    try PropertyListSerialization.data(fromPropertyList: edited, format: .xml, options: 0).write(to: app.appendingPathComponent("Contents/Info.plist"), options: .atomic)
    try fm.copyItem(at: assets.appendingPathComponent("loader-launcher"), to: macOS.appendingPathComponent("loader-launcher"))
    for (folder, input, output) in [("Frameworks", "loader-probe.dylib", "OpenSV2Loader.dylib"), ("PlugIns", "network-plugin.dylib", "network-plugin.dylib"), ("PlugIns", "sv2-mcp.dylib", "sv2-mcp.dylib")] {
        let directory = app.appendingPathComponent("Contents/\(folder)"); try fm.createDirectory(at: directory, withIntermediateDirectories: true)
        let target = directory.appendingPathComponent(output); try fm.copyItem(at: assets.appendingPathComponent(input), to: target)
        _ = try run("/usr/bin/codesign", ["--force", "--sign", "-", target.path])
    }
    let entitlements: [String: Bool] = ["com.apple.security.cs.allow-jit": true, "com.apple.security.cs.allow-unsigned-executable-memory": true, "com.apple.security.cs.allow-dyld-environment-variables": true, "com.apple.security.cs.disable-library-validation": true]
    let entitlementsFile = staging.appendingPathComponent("entitlements.plist")
    try PropertyListSerialization.data(fromPropertyList: entitlements, format: .xml, options: 0).write(to: entitlementsFile)
    for target in [macOS.appendingPathComponent("synthv-studio-original"), app] {
        _ = try run("/usr/bin/codesign", ["--force", "--sign", "-", "--identifier", hostIdentifier, "--options", "runtime", "--entitlements", entitlementsFile.path, target.path])
    }
    _ = try run("/usr/bin/codesign", ["--verify", "--deep", "--strict", app.path])
    let receipt = RuntimeReceipt(fingerprint: fingerprint, sourceApplication: source.path, sourceVersion: version, appDirectory: destination.appendingPathComponent("SV2 Extended.app").path, createdAt: ISO8601DateFormatter().string(from: Date()), hostBundleIdentifier: hostIdentifier)
    try writeJSON(receipt, to: staging.appendingPathComponent("receipt.json"))
    try fm.moveItem(at: staging, to: destination)
    return receipt
}
func settingsDialog(_ previous: LoaderSettings) throws -> LoaderSettings? {
    let app = NSApplication.shared; app.setActivationPolicy(.regular); app.finishLaunching(); app.activate(ignoringOtherApps: true)
    let alert = NSAlert(); alert.messageText = "Extend Loader 设置"
    alert.informativeText = "数据目录可位于原版数据旁边，或使用独立位置。原版应用和数据保持不动。"
    alert.addButton(withTitle: "保存"); alert.addButton(withTitle: "取消")
    let stack = NSStackView(); stack.orientation = .vertical; stack.alignment = .leading; stack.spacing = 8
    stack.addArrangedSubview(NSTextField(labelWithString: "官方应用位置"))
    let source = NSTextField(string: previous.sourceApplication); source.frame.size.width = 620; stack.addArrangedSubview(source)
    stack.addArrangedSubview(NSTextField(labelWithString: "Loader 数据目录（默认在原版数据目录旁边）"))
    let directory = NSTextField(string: previous.dataDirectory); directory.frame.size.width = 620; stack.addArrangedSubview(directory)
    let isolation = NSButton(checkboxWithTitle: "隔离宿主应用数据（保留原版数据）", target: nil, action: nil)
    isolation.state = previous.isolateHostData ? .on : .off; stack.addArrangedSubview(isolation)
    let local = NSButton(checkboxWithTitle: "允许扩展脚本请求本机回环服务（localhost / 127.0.0.1 / ::1）", target: nil, action: nil)
    local.state = previous.allowScriptHTTPLocal ? .on : .off; stack.addArrangedSubview(local)
    let online = NSButton(checkboxWithTitle: "允许扩展脚本请求在线及其他网络服务", target: nil, action: nil)
    online.state = previous.allowScriptHTTPOnline ? .on : .off; stack.addArrangedSubview(online)
    stack.addArrangedSubview(NSTextField(labelWithString: "适用于 Lua / JS 的共享 HTTP API；下次启动生效。"))
    stack.frame = NSRect(x: 0, y: 0, width: 620, height: 250); alert.accessoryView = stack
    guard alert.runModal() == .alertFirstButtonReturn else { return nil }
    let root = canonical(directory.stringValue)
    return LoaderSettings(sourceApplication: canonical(source.stringValue).path, dataMode: root == canonical(alongsideData.path) ? "alongside-original-data" : "independent", dataDirectory: root.path, scriptHTTPLocal: local.state == .on, scriptHTTPOnline: online.state == .on, hostDataNamespace: isolation.state == .on)
}
func applyRuntimeOptions(_ settings: inout LoaderSettings) throws {
    for (option, local) in [("--script-http-local", true), ("--script-http-online", false)] {
        if let i = arguments.firstIndex(of: option) {
            guard i + 1 < arguments.count, ["allow", "deny"].contains(arguments[i + 1]) else { throw LoaderError(message: "\(option) requires allow or deny.") }
            if local { settings.scriptHTTPLocal = arguments[i + 1] == "allow" }
            else { settings.scriptHTTPOnline = arguments[i + 1] == "allow" }
        }
    }
    let isolation = arguments.contains("--isolate-host-data") || arguments.contains("--experimental-host-data")
    guard !(isolation && arguments.contains("--legacy-host-data")) else { throw LoaderError(message: "Choose either isolated or legacy host data.") }
    if isolation { settings.hostDataNamespace = true }
    if arguments.contains("--legacy-host-data") { settings.hostDataNamespace = false }
}
func main() throws {
    var settings = try currentSettings(); let previous = settings
    if arguments.contains("--settings") || (arguments.isEmpty && NSEvent.modifierFlags.contains(.option)) { guard let edited = try settingsDialog(settings) else { return }; settings = edited }
    if let i = arguments.firstIndex(of: "--data-dir"), i + 1 < arguments.count {
        settings.dataDirectory = canonical(arguments[i + 1]).path
        settings.dataMode = canonical(settings.dataDirectory) == canonical(alongsideData.path) ? "alongside-original-data" : "independent"
    }
    if arguments.contains("--alongside-data") { settings.dataDirectory = alongsideData.path; settings.dataMode = "alongside-original-data" }
    if let i = arguments.firstIndex(of: "--source-app"), i + 1 < arguments.count { settings.sourceApplication = canonical(arguments[i + 1]).path }
    try applyRuntimeOptions(&settings)
    if arguments.contains("--status") {
        let encoder = JSONEncoder(); encoder.outputFormatting = [.prettyPrinted, .sortedKeys]
        print(String(decoding: try encoder.encode(settings), as: UTF8.self)); return
    }
    try saveSettings(settings, previous: previous)
    settings = try currentSettings() // Existing destinations keep their own settings unless explicitly overridden.
    let selected = settings
    try applyRuntimeOptions(&settings)
    if settings != selected { try saveSettings(settings, previous: selected) }
    if arguments.contains("--configure-only") { print(settings.dataDirectory); return }
    let root = canonical(settings.dataDirectory)
    let nativeData = settings.isolateHostData ? try prepareHostData(root) : nil
    if let nativeData = nativeData { try installLuaResources(root, assets: assetDirectory(), hostData: nativeData) }
    let receipt = try prepareRuntime(settings)
    if arguments.contains("--prepare-only") { print(receipt.appDirectory); return }
    if nativeData == nil { try installLuaResources(root, assets: assetDirectory()) }
    for name in ["logs", "plugins", "plugins/network-probe", "plugins/network-probe/data"] { try fm.createDirectory(at: root.appendingPathComponent(name), withIntermediateDirectories: true, attributes: [.posixPermissions: 0o700]) }
    setenv("OPEN_SV2_LOADER_DATA_DIR", root.path, 1)
    let logfile = root.appendingPathComponent("logs/last-launch.log")
    let descriptor = open(logfile.path, O_WRONLY | O_CREAT | O_APPEND, 0o600)
    if descriptor >= 0 { dup2(descriptor, STDERR_FILENO); close(descriptor) }
    let executable = URL(fileURLWithPath: receipt.appDirectory).appendingPathComponent("Contents/MacOS/loader-launcher").path
    fputs("EXTEND_LOADER pid=\(getpid()) data=\(root.path) exec-managed-host\n", stderr)
    let arg = strdup(executable); var argv: [UnsafeMutablePointer<CChar>?] = [arg, nil]
    executable.withCString { path in argv.withUnsafeMutableBufferPointer { buffer in _ = execv(path, buffer.baseAddress!) } }
    throw LoaderError(message: "Unable to start SV2: \(String(cString: strerror(errno)))")
}
do { try main() } catch {
    fputs("Extend Loader: \(error.localizedDescription)\n", stderr)
    if !arguments.contains(where: { $0.hasPrefix("--") }) {
        let app = NSApplication.shared; app.setActivationPolicy(.regular); app.finishLaunching(); app.activate(ignoringOtherApps: true)
        let alert = NSAlert(); alert.messageText = "Extend Loader 启动失败"; alert.informativeText = error.localizedDescription; alert.runModal()
    }
    exit(1)
}
