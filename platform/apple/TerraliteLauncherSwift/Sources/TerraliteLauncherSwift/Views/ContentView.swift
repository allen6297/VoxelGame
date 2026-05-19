import SwiftUI

struct ContentView: View {
    @ObservedObject var store: LauncherStore
    @SceneStorage("selectedLauncherSection") private var selectedSection: LauncherSection = .play

    var body: some View {
        NavigationSplitView {
            SidebarView(selection: $selectedSection)
        } detail: {
            switch selectedSection {
            case .play:
                LaunchView(store: store)
            case .accounts:
                AccountsView(store: store)
            case .versions:
                VersionsView(store: store)
            }
        }
        .toolbar {
            ToolbarItemGroup {
                Button {
                    store.refreshVersions()
                } label: {
                    Label("Refresh Versions", systemImage: "arrow.clockwise")
                }
                Button {
                    store.launchGame()
                } label: {
                    Label("Launch", systemImage: "play.fill")
                }
            }
        }
    }
}

enum LauncherSection: String, CaseIterable, Identifiable {
    case play = "Play"
    case accounts = "Accounts"
    case versions = "Versions"

    var id: String { rawValue }

    var systemImage: String {
        switch self {
        case .play: return "play.circle"
        case .accounts: return "person.crop.circle"
        case .versions: return "square.stack.3d.up"
        }
    }
}

#Preview {
    ContentView(store: LauncherStore())
}
