import Foundation

enum LauncherCore {
    static let encoder: JSONEncoder = {
        let encoder = JSONEncoder()
        encoder.outputFormatting = [.prettyPrinted, .sortedKeys]
        return encoder
    }()

    static let decoder = JSONDecoder()

    static func loadState(configURL: URL, launcherURL: URL, sourceDirectory: URL) -> LauncherState {
        guard let data = try? Data(contentsOf: configURL),
              let state = try? decoder.decode(LauncherState.self, from: data),
              !state.accounts.isEmpty,
              !state.versions.isEmpty
        else {
            return .defaultState(launcherURL: launcherURL, sourceDirectory: sourceDirectory)
        }
        return state
    }

    static func saveState(_ state: LauncherState, configURL: URL) throws {
        try FileManager.default.createDirectory(
            at: configURL.deletingLastPathComponent(),
            withIntermediateDirectories: true
        )
        let data = try encoder.encode(state)
        try data.write(to: configURL, options: .atomic)
    }

    static func selectedAccount(in state: LauncherState) -> AccountProfile? {
        state.accounts.first { $0.id == state.selectedAccountId }
    }

    static func selectedVersion(in state: LauncherState) -> GameVersion? {
        state.versions.first { $0.id == state.selectedVersionId }
    }

    static func addAccount(to state: inout LauncherState) {
        let next = state.accounts.count + 1
        let account = AccountProfile(id: "offline-\(next)", displayName: "Player \(next)")
        state.accounts.append(account)
        state.selectedAccountId = account.id
    }

    static func removeSelectedAccount(from state: inout LauncherState) {
        guard state.accounts.count > 1 else { return }
        state.accounts.removeAll { $0.id == state.selectedAccountId }
        state.selectedAccountId = state.accounts.first?.id ?? ""
    }

    static func renameSelectedAccount(in state: inout LauncherState, displayName: String) {
        guard let index = state.accounts.firstIndex(where: { $0.id == state.selectedAccountId }) else { return }
        state.accounts[index].displayName = displayName
        state.accounts[index].id = displayName.terraliteSlug
        state.selectedAccountId = state.accounts[index].id
    }

    static func addVersion(to state: inout LauncherState) {
        let next = state.versions.count + 1
        var version = selectedVersion(in: state) ?? GameVersion()
        version.id = "version-\(next)"
        version.name = "New Version"
        state.versions.append(version)
        state.selectedVersionId = version.id
    }

    static func removeSelectedVersion(from state: inout LauncherState) {
        guard state.versions.count > 1 else { return }
        state.versions.removeAll { $0.id == state.selectedVersionId }
        state.selectedVersionId = state.versions.first?.id ?? ""
    }

    static func updateSelectedVersion(in state: inout LauncherState, with updated: GameVersion) {
        guard let index = state.versions.firstIndex(where: { $0.id == state.selectedVersionId }) else { return }
        state.versions[index] = updated
        state.selectedVersionId = updated.id
    }

    static func versionStatus(_ version: GameVersion) -> VersionStatus {
        if version.source == "invalid-manifest" || version.id.isEmpty || version.name.isEmpty {
            return .invalidManifest
        }
        if version.source == "local-dev" {
            return .localDev
        }
        if version.gameExecutable.isEmpty || !FileManager.default.fileExists(atPath: version.gameExecutable) {
            return .missingExecutable
        }
        if version.serverExecutable.isEmpty || !FileManager.default.fileExists(atPath: version.serverExecutable) {
            return .missingExecutable
        }
        return .installed
    }

    static func loadVersionManifest(at manifestURL: URL) throws -> GameVersion {
        let data = try Data(contentsOf: manifestURL)
        let manifest = try decoder.decode(VersionManifest.self, from: data)
        var version = manifest.version
        let baseURL = manifestURL.deletingLastPathComponent()
        version.gameExecutable = resolve(version.gameExecutable, relativeTo: baseURL).path
        version.serverExecutable = resolve(version.serverExecutable, relativeTo: baseURL).path
        version.workingDirectory = resolve(version.workingDirectory.isEmpty ? "." : version.workingDirectory, relativeTo: baseURL).path
        if version.source.isEmpty {
            version.source = "manifest"
        }
        return version
    }

    static func saveVersionManifest(version: GameVersion, installed: Bool, manifestURL: URL) throws {
        try FileManager.default.createDirectory(
            at: manifestURL.deletingLastPathComponent(),
            withIntermediateDirectories: true
        )

        var relative = version
        relative.gameExecutable = URL(fileURLWithPath: version.gameExecutable).lastPathComponent
        relative.serverExecutable = URL(fileURLWithPath: version.serverExecutable).lastPathComponent
        relative.workingDirectory = "."
        let data = try encoder.encode(VersionManifest(version: relative, installed: installed))
        try data.write(to: manifestURL, options: .atomic)
    }

    static func discoverInstalledVersions(in versionRoot: URL) -> [GameVersion] {
        guard let entries = try? FileManager.default.contentsOfDirectory(
            at: versionRoot,
            includingPropertiesForKeys: [.isDirectoryKey],
            options: [.skipsHiddenFiles]
        ) else {
            return []
        }

        return entries.compactMap { directory -> GameVersion? in
            let values = try? directory.resourceValues(forKeys: [.isDirectoryKey])
            guard values?.isDirectory == true else { return nil }
            let manifestURL = directory.appendingPathComponent("manifest.json")
            guard FileManager.default.fileExists(atPath: manifestURL.path) else { return nil }

            do {
                return try loadVersionManifest(at: manifestURL)
            } catch {
                return GameVersion(
                    id: directory.lastPathComponent,
                    name: directory.lastPathComponent,
                    source: "invalid-manifest",
                    workingDirectory: directory.path
                )
            }
        }
        .sorted { $0.id < $1.id }
    }

    static func mergeDiscoveredVersions(into state: inout LauncherState, discovered: [GameVersion]) {
        for version in discovered {
            if let index = state.versions.firstIndex(where: { $0.id == version.id }) {
                if state.versions[index].source == "manifest" || state.versions[index].source == "invalid-manifest" {
                    state.versions[index] = version
                }
            } else {
                state.versions.append(version)
            }
        }
    }

    static func installLocalBuild(
        id: String,
        name: String,
        channel: String,
        localVersion: GameVersion,
        versionRoot: URL,
        sourcePacksDirectory: URL,
        overwrite: Bool
    ) throws -> GameVersion {
        guard !id.isEmpty else { throw LauncherError.validation("Version id is required.") }
        guard !name.isEmpty else { throw LauncherError.validation("Version name is required.") }

        let installURL = versionRoot.appendingPathComponent(id, isDirectory: true)
        if FileManager.default.fileExists(atPath: installURL.path) {
            guard overwrite else { throw LauncherError.validation("Version already exists: \(installURL.path)") }
            try FileManager.default.removeItem(at: installURL)
        }

        try FileManager.default.createDirectory(at: installURL, withIntermediateDirectories: true)
        let gameURL = URL(fileURLWithPath: localVersion.gameExecutable)
        let serverURL = URL(fileURLWithPath: localVersion.serverExecutable)
        try copyRequiredFile(from: gameURL, to: installURL.appendingPathComponent(gameURL.lastPathComponent))
        try copyRequiredFile(from: serverURL, to: installURL.appendingPathComponent(serverURL.lastPathComponent))
        try copyOptionalDirectory(from: sourcePacksDirectory, to: installURL.appendingPathComponent("packs", isDirectory: true))

        let installed = GameVersion(
            id: id,
            name: name,
            channel: channel.isEmpty ? "local" : channel,
            source: "manifest",
            gameExecutable: installURL.appendingPathComponent(gameURL.lastPathComponent).path,
            serverExecutable: installURL.appendingPathComponent(serverURL.lastPathComponent).path,
            workingDirectory: installURL.path,
            extraArguments: localVersion.extraArguments
        )
        try saveVersionManifest(
            version: installed,
            installed: true,
            manifestURL: installURL.appendingPathComponent("manifest.json")
        )
        return try loadVersionManifest(at: installURL.appendingPathComponent("manifest.json"))
    }

    static func gameArguments(account: AccountProfile, version: GameVersion, options: LaunchOptions) -> [String] {
        var arguments = version.extraArguments.split { $0.isWhitespace }.map(String.init)
        arguments.append(contentsOf: ["--name", account.displayName])
        switch options.playMode {
        case .offline:
            break
        case .host:
            arguments.append(contentsOf: ["--host", options.port])
        case .connect:
            arguments.append(contentsOf: ["--connect", options.hostName, options.port])
        }
        return arguments
    }

    static func launchGame(account: AccountProfile, version: GameVersion, options: LaunchOptions) throws -> String {
        try launchProcess(
            executable: URL(fileURLWithPath: version.gameExecutable),
            workingDirectory: URL(fileURLWithPath: version.workingDirectory),
            arguments: gameArguments(account: account, version: version, options: options)
        )
    }

    static func launchServer(version: GameVersion, port: String) throws -> String {
        try launchProcess(
            executable: URL(fileURLWithPath: version.serverExecutable),
            workingDirectory: URL(fileURLWithPath: version.workingDirectory),
            arguments: ["--port", port]
        )
    }

    private static func resolve(_ path: String, relativeTo baseURL: URL) -> URL {
        (path.hasPrefix("/") ? URL(fileURLWithPath: path) : baseURL.appendingPathComponent(path)).standardizedFileURL
    }

    private static func copyRequiredFile(from source: URL, to destination: URL) throws {
        guard FileManager.default.fileExists(atPath: source.path) else {
            throw LauncherError.validation("Install source file not found: \(source.path)")
        }
        try FileManager.default.createDirectory(
            at: destination.deletingLastPathComponent(),
            withIntermediateDirectories: true
        )
        if FileManager.default.fileExists(atPath: destination.path) {
            try FileManager.default.removeItem(at: destination)
        }
        try FileManager.default.copyItem(at: source, to: destination)
    }

    private static func copyOptionalDirectory(from source: URL, to destination: URL) throws {
        guard FileManager.default.fileExists(atPath: source.path) else { return }
        if FileManager.default.fileExists(atPath: destination.path) {
            try FileManager.default.removeItem(at: destination)
        }
        try FileManager.default.copyItem(at: source, to: destination)
    }

    private static func launchProcess(executable: URL, workingDirectory: URL, arguments: [String]) throws -> String {
        guard FileManager.default.fileExists(atPath: executable.path) else {
            throw LauncherError.validation("Executable not found: \(executable.path)")
        }
        if !workingDirectory.path.isEmpty && !FileManager.default.fileExists(atPath: workingDirectory.path) {
            throw LauncherError.validation("Working directory not found: \(workingDirectory.path)")
        }

        let process = Process()
        process.executableURL = executable
        process.currentDirectoryURL = workingDirectory
        process.arguments = arguments
        try process.run()
        return "Started \(executable.lastPathComponent)"
    }
}

enum LauncherError: LocalizedError {
    case validation(String)

    var errorDescription: String? {
        switch self {
        case .validation(let message):
            return message
        }
    }
}
