import AppKit
import CoreGraphics
import Foundation
import ImageIO
import ScreenCaptureKit
import UniformTypeIdentifiers

@MainActor final class CaptureDelegate: NSObject, NSApplicationDelegate {
    private var window: NSWindow!
    private var status: NSTextField!
    private var busy = false
    private let pid: pid_t
    private let windowID: CGWindowID
    private let output: URL
    private let receipt: URL

    init?(arguments: [String]) {
        guard arguments.count == 5, let pid = Int32(arguments[1]),
              let id = UInt32(arguments[2]), arguments[3].hasPrefix("/"),
              arguments[4].hasPrefix("/") else { return nil }
        self.pid = pid
        self.windowID = id
        self.output = URL(fileURLWithPath: arguments[3])
        self.receipt = URL(fileURLWithPath: arguments[4])
    }

    private func record(_ value: String) {
        try? (value + "\n").write(to: receipt, atomically: true, encoding: .utf8)
    }

    func applicationDidFinishLaunching(_ notification: Notification) {
        window = NSWindow(contentRect: NSRect(x: 0, y: 0, width: 460, height: 130),
                          styleMask: [.titled, .closable], backing: .buffered, defer: false)
        window.title = "Extend UI Check"
        status = NSTextField(wrappingLabelWithString: "Allow screen recording for Extend UI Check, then click Capture Window.")
        status.frame = NSRect(x: 20, y: 55, width: 420, height: 52)
        let button = NSButton(title: "Capture Window", target: self, action: #selector(capture))
        button.frame = NSRect(x: 300, y: 14, width: 140, height: 28)
        window.contentView?.addSubview(status)
        window.contentView?.addSubview(button)
        window.center()
        window.makeKeyAndOrderFront(nil)
        NSApp.activate(ignoringOtherApps: true)
        if CGPreflightScreenCaptureAccess() {
            record("authorized")
            capture()
        } else {
            record("authorization_requested")
            let granted = CGRequestScreenCaptureAccess()
            record(granted ? "authorized" : "awaiting_authorization")
            if granted { capture() }
        }
    }

    @objc private func capture() {
        guard !busy else { return }
        guard CGPreflightScreenCaptureAccess() else {
            status.stringValue = "Screen recording is not enabled. Enable it in System Settings, then reopen this app."
            record("awaiting_authorization")
            return
        }
        busy = true
        Task {
            do {
                let content = try await SCShareableContent.excludingDesktopWindows(false, onScreenWindowsOnly: true)
                guard let target = content.windows.first(where: { $0.windowID == windowID && $0.owningApplication?.processID == pid }) else {
                    throw NSError(domain: "Capture", code: 1, userInfo: [NSLocalizedDescriptionKey: "The specified SV2 window is unavailable."])
                }
                let filter = SCContentFilter(desktopIndependentWindow: target)
                let config = SCStreamConfiguration()
                config.width = max(1, Int(ceil(filter.contentRect.width * CGFloat(filter.pointPixelScale))))
                config.height = max(1, Int(ceil(filter.contentRect.height * CGFloat(filter.pointPixelScale))))
                config.showsCursor = false
                let image = try await SCScreenshotManager.captureImage(contentFilter: filter, configuration: config)
                guard let destination = CGImageDestinationCreateWithURL(output as CFURL, UTType.png.identifier as CFString, 1, nil) else {
                    throw NSError(domain: "Capture", code: 2)
                }
                CGImageDestinationAddImage(destination, image, nil)
                guard CGImageDestinationFinalize(destination) else { throw NSError(domain: "Capture", code: 3) }
                record("captured")
                NSApp.terminate(nil)
            } catch {
                status.stringValue = error.localizedDescription
                record("capture_failed")
                busy = false
            }
        }
    }

    func applicationShouldTerminateAfterLastWindowClosed(_ sender: NSApplication) -> Bool { true }
}

@main struct Main {
 @MainActor static func main() {
  guard let delegate = CaptureDelegate(arguments: CommandLine.arguments) else { exit(64) }
  let application = NSApplication.shared
  application.setActivationPolicy(.regular)
  application.delegate = delegate
  application.run()
 }
}
