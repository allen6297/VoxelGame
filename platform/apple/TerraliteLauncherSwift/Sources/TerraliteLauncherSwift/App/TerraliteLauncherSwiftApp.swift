import AppKit
import SwiftUI

public final class LauncherAppDelegate: NSObject, NSApplicationDelegate {
    public override init() {
        super.init()
    }

    public func applicationDidFinishLaunching(_ notification: Notification) {
        NSApp.setActivationPolicy(.regular)
        NSApp.activate(ignoringOtherApps: true)
    }

    public func applicationWillFinishLaunching(_ notification: Notification) {
        // Prefer a unified, transparent titlebar for a modern look.
        if let window = NSApp.windows.first {
            configureWindow(window)
        }
    }

    public func applicationDidBecomeActive(_ notification: Notification) {
        // Ensure window remains key and adopts our style if new windows open.
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
}

public struct LauncherRootView: View {
    @StateObject private var store = LauncherStore()

    public init() {}

    public var body: some View {
        ContentView(store: store)
            .frame(minWidth: 1040, minHeight: 680)
            .toolbar {
                ToolbarItemGroup(placement: .primaryAction) {
                    Button {
                        store.refreshVersions()
                    } label: {
                        Label("Refresh", systemImage: "arrow.clockwise")
                    }
                    .help("Refresh")
                }
            }
            .navigationTitle("Terralite Launcher")
    }
}

public struct TerraliteLauncherScene: Scene {
    @StateObject private var store = LauncherStore()

    public init() {}

    public var body: some Scene {
        WindowGroup("Terralite Launcher") {
            ContentView(store: store)
                .frame(minWidth: 1040, minHeight: 680)
                .toolbar {
                    ToolbarItemGroup(placement: .primaryAction) {
                        Button {
                            store.refreshVersions()
                        } label: {
                            Label("Refresh", systemImage: "arrow.clockwise")
                        }
                        .help("Refresh")
                    }
                }
                .navigationTitle("Terralite Launcher")
        }
        .commands {
            CommandGroup(after: .newItem) {
                Button("Refresh Versions") {
                    store.refreshVersions()
                }
                .keyboardShortcut("r", modifiers: [.command])
                .help("Refresh the list of available versions")
            }
        }

        Settings {
            SettingsView(store: store)
                .frame(minWidth: 520, idealWidth: 560, minHeight: 420, idealHeight: 460)
                .padding()
        }
    }
}
