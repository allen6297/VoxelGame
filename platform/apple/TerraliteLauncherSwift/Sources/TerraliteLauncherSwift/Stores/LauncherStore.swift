import Combine
import Foundation

@MainActor
public final class LauncherStore: ObservableObject {
    @Published public private(set) var state: LauncherState
    @Published public var launchOptions = LaunchOptions()
    @Published public var statusMessage = ""
    @Published public var installId = "local-dev-install"
    @Published public var installName = "Local Dev Install"
    @Published public var installChannel = "local"
    @Published public var overwriteInstall = false

    public let configURL: URL
    public let versionRootURL: URL
    public let sourceDirectory: URL
    private let launcherURL: URL

    public init(
        configURL: URL = LauncherPaths.configURL,
        versionRootURL: URL = LauncherPaths.versionRootURL,
        launcherURL: URL = LauncherPaths.launcherExecutableURL,
        sourceDirectory: URL = LauncherPaths.sourceDirectory
    ) {
        self.configURL = configURL
        self.versionRootURL = versionRootURL
        self.launcherURL = launcherURL
        self.sourceDirectory = sourceDirectory
        self.state = LauncherCore.loadState(
            configURL: configURL,
            launcherURL: launcherURL,
            sourceDirectory: sourceDirectory
        )
        refreshVersions(save: false)
    }

    public var accounts: [AccountProfile] { state.accounts }
    public var versions: [GameVersion] { state.versions }

    public var selectedAccount: AccountProfile? {
        LauncherCore.selectedAccount(in: state)
    }

    public var selectedVersion: GameVersion? {
        LauncherCore.selectedVersion(in: state)
    }

    public func versionStatus(_ version: GameVersion) -> VersionStatus {
        LauncherCore.versionStatus(version)
    }

    public func selectAccount(_ id: AccountProfile.ID) {
        state.selectedAccountId = id
        save()
    }

    public func selectVersion(_ id: GameVersion.ID) {
        state.selectedVersionId = id
        save()
    }

    public func addAccount() {
        LauncherCore.addAccount(to: &state)
        save()
    }

    public func removeSelectedAccount() {
        LauncherCore.removeSelectedAccount(from: &state)
        save()
    }

    public func renameSelectedAccount(_ displayName: String) {
        LauncherCore.renameSelectedAccount(in: &state, displayName: displayName)
        save()
    }

    public func addVersion() {
        LauncherCore.addVersion(to: &state)
        save()
    }

    public func removeSelectedVersion() {
        LauncherCore.removeSelectedVersion(from: &state)
        save()
    }

    public func updateSelectedVersion(_ version: GameVersion) {
        LauncherCore.updateSelectedVersion(in: &state, with: version)
        save()
    }

    public func refreshVersions(save shouldSave: Bool = true) {
        LauncherCore.mergeDiscoveredVersions(
            into: &state,
            discovered: LauncherCore.discoverInstalledVersions(in: versionRootURL)
        )
        statusMessage = "Refreshed version manifests."
        if shouldSave {
            save()
        }
    }

    public func installLocalBuild() {
        guard let localVersion = state.versions.first(where: { $0.source == "local-dev" }) else {
            statusMessage = "No local-dev version is available."
            return
        }

        do {
            let installed = try LauncherCore.installLocalBuild(
                id: installId,
                name: installName,
                channel: installChannel,
                localVersion: localVersion,
                versionRoot: versionRootURL,
                sourcePacksDirectory: sourceDirectory.appendingPathComponent("packs", isDirectory: true),
                overwrite: overwriteInstall
            )
            LauncherCore.mergeDiscoveredVersions(into: &state, discovered: [installed])
            state.selectedVersionId = installed.id
            statusMessage = "Installed local build as \(installed.id)."
            save()
        } catch {
            statusMessage = error.localizedDescription
        }
    }

    public func launchGame() {
        guard let account = selectedAccount, let version = selectedVersion else { return }
        do {
            statusMessage = try LauncherCore.launchGame(
                account: account,
                version: version,
                options: launchOptions
            )
        } catch {
            statusMessage = error.localizedDescription
        }
    }

    public func launchServer() {
        guard let version = selectedVersion else { return }
        do {
            statusMessage = try LauncherCore.launchServer(version: version, port: launchOptions.port)
        } catch {
            statusMessage = error.localizedDescription
        }
    }

    public func save() {
        do {
            try LauncherCore.saveState(state, configURL: configURL)
        } catch {
            statusMessage = "Could not save launcher state: \(error.localizedDescription)"
        }
    }
}
