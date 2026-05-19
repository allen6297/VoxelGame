import SwiftUI
import TerraliteLauncherSwift

@main
struct TerraliteLauncherSwiftStandaloneApp: App {
    @NSApplicationDelegateAdaptor(LauncherAppDelegate.self) private var appDelegate

    var body: some Scene {
        TerraliteLauncherScene()
    }
}
