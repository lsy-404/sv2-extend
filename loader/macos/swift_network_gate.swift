import Foundation
import Network

// Constructor observation is currently arm64-only. Keep LOADER_NET_SWIFT_NETWORK
// unverified: callbacks, alternate initializers and multiplex paths need proof.
private final class GroupRecord {
    weak var group: NWConnectionGroup?
    var revoked = false
    let scope: Int32
    init(_ group: NWConnectionGroup, _ scope: Int32) { self.group = group; self.scope = scope }
    var host: Bool { scope == 0 || scope == 3 }
}
private final class ConnectionRecord {
    weak var connection: NWConnection?
    var revoked = false
    let scope: Int32
    init(_ connection: NWConnection, _ scope: Int32) {
        self.connection = connection
        if case .unix = connection.endpoint { self.scope = 1 } else { self.scope = scope }
    }
    var host: Bool { scope == 0 || scope == 3 }
}
private var connectionRecords: [ConnectionRecord] = []
private let swiftGateLock = NSRecursiveLock()
private var groupRecords: [GroupRecord] = []
private var swiftDenied = false

@_cdecl("loader_swift_group_created")
public func loaderSwiftGroupCreated(_ pointer: UnsafeMutableRawPointer) {
    let group = Unmanaged<NWConnectionGroup>.fromOpaque(pointer).takeUnretainedValue()
    let record = GroupRecord(group, socket_gate_current_scope())
    swiftGateLock.lock()
    groupRecords.removeAll { $0.group == nil }
    groupRecords.append(record)
    if swiftDenied && record.host { record.revoked = true; group.cancel() }
    swiftGateLock.unlock()
}

@_cdecl("loader_swift_network_set_blocked")
public func loaderSwiftNetworkSetBlocked(_ blocked: Int32) {
    swiftGateLock.lock()
    swiftDenied = blocked != 0
    groupRecords.removeAll { $0.group == nil }
    connectionRecords.removeAll { $0.connection == nil }
    if swiftDenied {
        for record in groupRecords where record.host { record.revoked = true; record.group?.cancel() }
        for record in connectionRecords where record.host { record.revoked = true; record.connection?.forceCancel() }
    }
    swiftGateLock.unlock()
}

// Conservative accounting: a retained Swift object remains outstanding even
// after cancel(). The SDK may keep state == setup forever after pre-start cancel;
// no fabricated terminal callback or isolation receipt is generated.
@_cdecl("loader_swift_network_outstanding")
public func loaderSwiftNetworkOutstanding() -> UInt32 {
    swiftGateLock.lock()
    defer { swiftGateLock.unlock() }
    groupRecords.removeAll { $0.group == nil }
    connectionRecords.removeAll { $0.connection == nil }
    return UInt32(clamping: groupRecords.filter { $0.host }.count + connectionRecords.filter { $0.host }.count)
}

// Calling SDK start after a pre-start cancel can retain its Swift wrapper
// indefinitely. Reject it at the same Swift boundary, preserving native silence.
@_cdecl("loader_swift_group_start_allowed")
public func loaderSwiftGroupStartAllowed(_ pointer: UnsafeMutableRawPointer) -> Int32 {
    let group = Unmanaged<NWConnectionGroup>.fromOpaque(pointer).takeUnretainedValue()
    swiftGateLock.lock()
    defer { swiftGateLock.unlock() }
    if let record = groupRecords.first(where: { $0.group === group }) {
        return record.host && (swiftDenied || record.revoked) ? 0 : 1
    }
    return swiftDenied ? 0 : 1
}

// The trampoline keeps this lease across SDK start/restart. A check followed by
// an unlocked SDK call can race block->cancel and start the pre-cancelled wrapper
// afterwards. Native callbacks remain asynchronous; recursive SDK forwarding on
// this thread is permitted by NSRecursiveLock.
@_cdecl("loader_swift_group_start_begin")
public func loaderSwiftGroupStartBegin(_ pointer: UnsafeMutableRawPointer) -> Int32 {
    let group = Unmanaged<NWConnectionGroup>.fromOpaque(pointer).takeUnretainedValue()
    swiftGateLock.lock()
    var record = groupRecords.first(where: { $0.group === group })
    if record == nil {
        groupRecords.removeAll { $0.group == nil }
        record = GroupRecord(group, socket_gate_current_scope())
        groupRecords.append(record!)
    }
    if record!.host && (swiftDenied || record!.revoked) {
        if !record!.revoked { record!.revoked = true; group.cancel() }
        swiftGateLock.unlock()
        return 0
    }
    return 1
}

@_cdecl("loader_swift_network_start_end")
public func loaderSwiftNetworkStartEnd() { swiftGateLock.unlock() }

@_cdecl("loader_swift_connection_created")
public func loaderSwiftConnectionCreated(_ pointer: UnsafeMutableRawPointer) {
    let connection = Unmanaged<NWConnection>.fromOpaque(pointer).takeUnretainedValue()
    swiftGateLock.lock()
    defer { swiftGateLock.unlock() }
    connectionRecords.removeAll { $0.connection == nil }
    // A convenience initializer can forward through another observed initializer.
    if connectionRecords.contains(where: { $0.connection === connection }) { return }
    let record = ConnectionRecord(connection, socket_gate_current_scope())
    connectionRecords.append(record)
    if swiftDenied && record.host { record.revoked = true; connection.forceCancel() }
}

@_cdecl("loader_swift_connection_start_allowed")
public func loaderSwiftConnectionStartAllowed(_ pointer: UnsafeMutableRawPointer) -> Int32 {
    let connection = Unmanaged<NWConnection>.fromOpaque(pointer).takeUnretainedValue()
    swiftGateLock.lock()
    defer { swiftGateLock.unlock() }
    if let record = connectionRecords.first(where: { $0.connection === connection }) {
        return record.host && (swiftDenied || record.revoked) ? 0 : 1
    }
    if case .unix = connection.endpoint { return 1 }
    return swiftDenied ? 0 : 1
}

@_cdecl("loader_swift_connection_start_begin")
public func loaderSwiftConnectionStartBegin(_ pointer: UnsafeMutableRawPointer) -> Int32 {
    let connection = Unmanaged<NWConnection>.fromOpaque(pointer).takeUnretainedValue()
    swiftGateLock.lock()
    var record = connectionRecords.first(where: { $0.connection === connection })
    if record == nil {
        connectionRecords.removeAll { $0.connection == nil }
        record = ConnectionRecord(connection, socket_gate_current_scope())
        connectionRecords.append(record!)
    }
    if record!.host && (swiftDenied || record!.revoked) {
        if !record!.revoked { record!.revoked = true; connection.forceCancel() }
        swiftGateLock.unlock()
        return 0
    }
    return 1
}
