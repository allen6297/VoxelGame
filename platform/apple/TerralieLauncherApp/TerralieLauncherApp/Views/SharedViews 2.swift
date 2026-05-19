import Combine
import SwiftUI
import TerraliteLauncherSwift

protocol LauncherStoreLike: AnyObject, ObservableObject {
    var configURL: URL { get }
    var versionRootURL: URL { get }
    var sourceDirectory: URL { get }
}

struct HeaderView: View {
    let title: String
    let subtitle: String

    var body: some View {
        VStack(alignment: .leading, spacing: 6) {
            Text(title)
                .font(.largeTitle.weight(.semibold))
                .accessibilityAddTraits(.isHeader)
            Text(subtitle)
                .font(.title3)
                .foregroundStyle(.secondary)
                .lineLimit(2)
                .textSelection(.enabled)
        }
        .padding(.vertical, 4)
    }
}

struct StatusView: View {
    let message: String

    var body: some View {
        Group {
            if message.isEmpty {
                EmptyView()
            } else {
                Text(message)
                    .foregroundStyle(.secondary)
                    .textSelection(.enabled)
                    .padding(.top, 4)
            }
        }
    }
}

struct SettingsView<Store: LauncherStoreLike>: View {
    @ObservedObject var store: Store

    var body: some View {
        Form {
            LabeledContent("Config") {
                Text(store.configURL.path).textSelection(.enabled)
            }
            LabeledContent("Versions") {
                Text(store.versionRootURL.path).textSelection(.enabled)
            }
            LabeledContent("Source") {
                Text(store.sourceDirectory.path).textSelection(.enabled)
            }
        }
        .padding(24)
        .frame(width: 560)
    }
}

extension LauncherStore: LauncherStoreLike {}

// MARK: - Preview Mocks
final class MockLauncherStore: ObservableObject, LauncherStoreLike {
    let configURL = URL(fileURLWithPath: "/Users/preview/Library/Application Support/Terralite/config.json")
    let versionRootURL = URL(fileURLWithPath: "/Users/preview/.terralite/versions")
    let sourceDirectory = URL(fileURLWithPath: "/Users/preview/Projects/Terralite")
}

extension LauncherViewModel {
    static var preview: LauncherViewModel {
        let root = URL(fileURLWithPath: "/tmp/terralite-launcher-preview", isDirectory: true)
        let store = LauncherStore(
            configURL: root.appendingPathComponent("config.json"),
            versionRootURL: root.appendingPathComponent("versions", isDirectory: true),
            launcherURL: root.appendingPathComponent("bin/TerraliteLauncher"),
            sourceDirectory: root.appendingPathComponent("source", isDirectory: true)
        )
        return LauncherViewModel(store: store)
    }
}

#Preview("HeaderView – Long Subtitle") {
    HeaderView(
        title: "Play Terralite",
        subtitle: "Local development build — Manage versions, accounts, and launch options"
    )
    .padding()
    .frame(maxWidth: 600)
}

#Preview("StatusView – With Message") {
    VStack(alignment: .leading) {
        StatusView(message: "Refreshed version manifests.")
        StatusView(message: "") // Should render nothing
    }
    .padding()
    .frame(maxWidth: 600)
}

#Preview("SettingsView") {
    let mock = MockLauncherStore()
    SettingsView(store: mock)
}
