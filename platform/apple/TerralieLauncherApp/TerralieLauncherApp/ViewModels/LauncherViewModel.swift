import Combine
import Foundation
import TerraliteLauncherSwift

@MainActor
final class LauncherViewModel: ObservableObject {
    private let store: LauncherStore
    private var cancellables: Set<AnyCancellable> = []

    init() {
        self.store = LauncherStore()
        bindStoreChanges()
    }

    init(store: LauncherStore) {
        self.store = store
        bindStoreChanges()
    }

    private func bindStoreChanges() {
        store.objectWillChange
            .sink { [weak self] _ in
                self?.objectWillChange.send()
            }
            .store(in: &cancellables)
    }

    var configURL: URL { store.configURL }
    var versionRootURL: URL { store.versionRootURL }
    var sourceDirectory: URL { store.sourceDirectory }
    var statusMessage: String { store.statusMessage }

    var accounts: [AccountProfile] { store.accounts }
    var versions: [GameVersion] { store.versions }
    var selectedAccount: AccountProfile? { store.selectedAccount }
    var selectedVersion: GameVersion? { store.selectedVersion }

    var selectedAccountId: AccountProfile.ID {
        store.state.selectedAccountId
    }

    var selectedVersionId: GameVersion.ID {
        store.state.selectedVersionId
    }

    var launchOptions: LaunchOptions {
        get { store.launchOptions }
        set { store.launchOptions = newValue }
    }

    var installId: String {
        get { store.installId }
        set { store.installId = newValue }
    }

    var installName: String {
        get { store.installName }
        set { store.installName = newValue }
    }

    var installChannel: String {
        get { store.installChannel }
        set { store.installChannel = newValue }
    }

    var overwriteInstall: Bool {
        get { store.overwriteInstall }
        set { store.overwriteInstall = newValue }
    }

    func versionStatus(_ version: GameVersion) -> VersionStatus {
        store.versionStatus(version)
    }

    func selectAccount(_ id: AccountProfile.ID) {
        store.selectAccount(id)
    }

    func selectVersion(_ id: GameVersion.ID) {
        store.selectVersion(id)
    }

    func addAccount() {
        store.addAccount()
    }

    func removeSelectedAccount() {
        store.removeSelectedAccount()
    }

    func renameSelectedAccount(_ displayName: String) {
        store.renameSelectedAccount(displayName)
    }

    func addVersion() {
        store.addVersion()
    }

    func removeSelectedVersion() {
        store.removeSelectedVersion()
    }

    func updateSelectedVersion(_ version: GameVersion) {
        store.updateSelectedVersion(version)
    }

    func refreshVersions() {
        store.refreshVersions()
    }

    func installLocalBuild() {
        store.installLocalBuild()
    }

    func launchGame() {
        store.launchGame()
    }

    func launchServer() {
        store.launchServer()
    }
}

extension LauncherViewModel: LauncherStoreLike {}
