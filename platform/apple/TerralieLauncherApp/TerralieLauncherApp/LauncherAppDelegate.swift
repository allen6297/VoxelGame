import AppKit

final class LauncherAppDelegate: NSObject, NSApplicationDelegate {
    func applicationDidFinishLaunching(_ notification: Notification) {
        guard !isRunningForPreviews else { return }
        NSApp.setActivationPolicy(.regular)
        NSApp.activate(ignoringOtherApps: true)
    }

    func applicationWillFinishLaunching(_ notification: Notification) {
        if let window = NSApp.windows.first {
            configureWindow(window)
        }
    }

    func applicationDidBecomeActive(_ notification: Notification) {
        NSApp.windows.forEach { configureWindow($0) }
    }

    private func configureWindow(_ window: NSWindow) {
        window.titleVisibility = .hidden
        window.titlebarAppearsTransparent = true
        window.isMovableByWindowBackground = true
        window.toolbarStyle = .unifiedCompact
        window.standardWindowButton(.zoomButton)?.isHidden = false
        window.standardWindowButton(.miniaturizeButton)?.isHidden = false
        window.standardWindowButton(.closeButton)?.isHidden = false
        window.isOpaque = false
        window.backgroundColor = NSColor.windowBackgroundColor
    }

    private var isRunningForPreviews: Bool {
        ProcessInfo.processInfo.environment["XCODE_RUNNING_FOR_PREVIEWS"] == "1"
    }
}
