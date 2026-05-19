import Foundation

public struct LauncherState: Codable, Equatable {
    public var accounts: [AccountProfile]
    public var versions: [GameVersion]
    public var selectedAccountId: String
    public var selectedVersionId: String

    static func defaultState(launcherURL: URL, sourceDirectory: URL) -> LauncherState {
        let account = AccountProfile(id: "offline-player", displayName: "Player")
        let localBuildDirectory = sourceDirectory.appendingPathComponent("cmake-build-debug", isDirectory: true)
        let executableDirectory: URL
        if FileManager.default.fileExists(atPath: localBuildDirectory.appendingPathComponent("Terralite").path),
           FileManager.default.fileExists(atPath: localBuildDirectory.appendingPathComponent("TerraliteServer").path) {
            executableDirectory = localBuildDirectory
        } else {
            executableDirectory = launcherURL.deletingLastPathComponent()
        }
        let version = GameVersion(
            id: "local-dev",
            name: "Local development build",
            channel: "dev",
            source: "local-dev",
            gameExecutable: executableDirectory.appendingPathComponent("Terralite").path,
            serverExecutable: executableDirectory.appendingPathComponent("TerraliteServer").path,
            workingDirectory: sourceDirectory.path
        )

        return LauncherState(
            accounts: [account],
            versions: [version],
            selectedAccountId: account.id,
            selectedVersionId: version.id
        )
    }
}

public enum PlayMode: String, CaseIterable, Identifiable {
    case offline = "Offline"
    case host = "Host"
    case connect = "Connect"

    public var id: String { rawValue }
}

public struct LaunchOptions: Equatable {
    public var playMode: PlayMode = .offline
    public var hostName: String = "127.0.0.1"
    public var port: String = "27015"

    public init(playMode: PlayMode = .offline, hostName: String = "127.0.0.1", port: String = "27015") {
        self.playMode = playMode
        self.hostName = hostName
        self.port = port
    }
}
