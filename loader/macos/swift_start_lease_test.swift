import Foundation
import Network

@_silgen_name("extend_test_scope") private func setScope(_ value: Int32)

@main struct StartLeaseTest {
    static func pointer<T: AnyObject>(_ object: T) -> UnsafeMutableRawPointer {
        Unmanaged.passUnretained(object).toOpaque()
    }
    static func blocksUntilEnd(_ begin: () -> Int32) -> Bool {
        guard begin() == 1 else { return false }
        let entered = DispatchSemaphore(value: 0), finished = DispatchSemaphore(value: 0)
        DispatchQueue.global().async {
            entered.signal()
            loaderSwiftNetworkSetBlocked(1)
            finished.signal()
        }
        guard entered.wait(timeout: .now() + 2) == .success else {
            loaderSwiftNetworkStartEnd(); return false
        }
        let waits = finished.wait(timeout: .now() + 0.1) == .timedOut
        loaderSwiftNetworkStartEnd()
        return waits && finished.wait(timeout: .now() + 2) == .success
    }
    static func main() throws {
        var checks: [String: Bool] = [:]
        let baseline = loaderSwiftNetworkOutstanding()
        // Constructors are deliberately not interposed in this executable.
        // Start admission must discover these real SDK objects on its own.
        let connection = NWConnection(host: "127.0.0.1", port: 9, using: .tcp)
        checks["connectionStartSerializesWithBlock"] = blocksUntilEnd {
            loaderSwiftConnectionStartBegin(pointer(connection))
        }
        checks["unknownConnectionDiscoveredAtStart"] = loaderSwiftNetworkOutstanding() == baseline + 1
        loaderSwiftNetworkSetBlocked(0)
        checks["concurrentlyRevokedConnectionCannotRestart"] = loaderSwiftConnectionStartBegin(pointer(connection)) == 0
        let descriptor = try NWMulticastGroup(for: [.hostPort(host: "239.255.18.42", port: 45981)])
        let group = NWConnectionGroup(with: descriptor, using: .udp)
        checks["groupStartSerializesWithBlock"] = blocksUntilEnd {
            loaderSwiftGroupStartBegin(pointer(group))
        }
        loaderSwiftNetworkSetBlocked(0)
        checks["concurrentlyRevokedGroupCannotRestart"] = loaderSwiftGroupStartBegin(pointer(group)) == 0
        loaderSwiftNetworkSetBlocked(1)
        let late = NWConnection(host: "127.0.0.1", port: 9, using: .tcp)
        checks["unknownBlockedConnectionDenied"] = loaderSwiftConnectionStartBegin(pointer(late)) == 0
        let lateGroup = NWConnectionGroup(with: descriptor, using: .udp)
        checks["unknownBlockedGroupDenied"] = loaderSwiftGroupStartBegin(pointer(lateGroup)) == 0
        loaderSwiftNetworkSetBlocked(0)
        checks["unknownBlockedConnectionRemainsRevoked"] = loaderSwiftConnectionStartBegin(pointer(late)) == 0
        checks["unknownBlockedGroupRemainsRevoked"] = loaderSwiftGroupStartBegin(pointer(lateGroup)) == 0
        loaderSwiftNetworkSetBlocked(1)
        let beforePlugin = loaderSwiftNetworkOutstanding()
        setScope(2)
        let plugin = NWConnection(host: "127.0.0.1", port: 9, using: .tcp)
        let pluginAdmission = loaderSwiftConnectionStartBegin(pointer(plugin))
        if pluginAdmission == 1 { loaderSwiftNetworkStartEnd() }
        setScope(0)
        checks["pluginStartAllowedAndExcluded"] = pluginAdmission == 1 && loaderSwiftNetworkOutstanding() == beforePlugin
        let local = NWConnection(to: .unix(path: "/tmp/extend-nonexistent-start-test.sock"), using: .tcp)
        let localAdmission = loaderSwiftConnectionStartBegin(pointer(local))
        if localAdmission == 1 { loaderSwiftNetworkStartEnd() }
        checks["unixStartAllowedAndExcluded"] = localAdmission == 1 && loaderSwiftNetworkOutstanding() == beforePlugin
        loaderSwiftNetworkSetBlocked(0)
        let normal = NWConnection(host: "127.0.0.1", port: 9, using: .tcp)
        let first = loaderSwiftConnectionStartBegin(pointer(normal))
        let nested = first == 1 ? loaderSwiftConnectionStartBegin(pointer(normal)) : 0
        if nested == 1 { loaderSwiftNetworkStartEnd() }
        if first == 1 { loaderSwiftNetworkStartEnd() }
        checks["recursiveSDKForwardingBalanced"] = first == 1 && nested == 1
        let result: [String: Any] = ["checks": checks, "passed": checks.values.allSatisfy { $0 },
            "scope": "Production Swift gate with real SDK objects and contending threads. Admission/blocked ordering only; not whole-host or callback-drain proof."]
        let data = try JSONSerialization.data(withJSONObject: result, options: [.prettyPrinted, .sortedKeys])
        try data.write(to: URL(fileURLWithPath: CommandLine.arguments[1]))
        print(String(decoding: data, as: UTF8.self))
        if !checks.values.allSatisfy({ $0 }) { exit(1) }
    }
}
