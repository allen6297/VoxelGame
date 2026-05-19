import Foundation

public struct GameVersion: Codable, Identifiable, Equatable {
    public var id: String
    public var name: String
    public var channel: String
    public var source: String
    public var gameExecutable: String
    public var serverExecutable: String
    public var workingDirectory: String
    public var extraArguments: String

    public init(
        id: String = "",
        name: String = "",
        channel: String = "local",
        source: String = "manual",
        gameExecutable: String = "",
        serverExecutable: String = "",
        workingDirectory: String = "",
        extraArguments: String = ""
    ) {
        self.id = id
        self.name = name
        self.channel = channel
        self.source = source
        self.gameExecutable = gameExecutable
        self.serverExecutable = serverExecutable
        self.workingDirectory = workingDirectory
        self.extraArguments = extraArguments
    }
}

public enum VersionStatus: String {
    case localDev = "Local dev"
    case installed = "Installed"
    case missingExecutable = "Missing executable"
    case invalidManifest = "Invalid manifest"
}

struct VersionManifest: Codable, Equatable {
    var id: String
    var name: String
    var channel: String
    var source: String
    var gameExecutable: String
    var serverExecutable: String
    var workingDirectory: String
    var extraArguments: String
    var installed: Bool

    init(version: GameVersion, installed: Bool = true) {
        self.id = version.id
        self.name = version.name
        self.channel = version.channel
        self.source = version.source
        self.gameExecutable = version.gameExecutable
        self.serverExecutable = version.serverExecutable
        self.workingDirectory = version.workingDirectory
        self.extraArguments = version.extraArguments
        self.installed = installed
    }

    var version: GameVersion {
        GameVersion(
            id: id,
            name: name,
            channel: channel,
            source: source,
            gameExecutable: gameExecutable,
            serverExecutable: serverExecutable,
            workingDirectory: workingDirectory,
            extraArguments: extraArguments
        )
    }
}
