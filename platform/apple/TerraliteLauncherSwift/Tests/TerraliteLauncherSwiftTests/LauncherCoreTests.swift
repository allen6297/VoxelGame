import XCTest
@testable import TerraliteLauncherSwift

final class LauncherCoreTests: XCTestCase {
    func testSlugMatchesLauncherConventions() {
        XCTAssertEqual("Kyle Test".terraliteSlug, "kyle-test")
        XCTAssertEqual("  ".terraliteSlug, "player")
        XCTAssertEqual("Player!!!Two".terraliteSlug, "player-two")
    }

    func testDefaultStateUsesSiblingExecutables() {
        let root = temporaryDirectory()
        let launcher = root.appendingPathComponent("bin/TerraliteLauncherSwift")
        let source = root.appendingPathComponent("source", isDirectory: true)
        let state = LauncherState.defaultState(launcherURL: launcher, sourceDirectory: source)

        XCTAssertEqual(state.accounts.count, 1)
        XCTAssertEqual(state.versions.count, 1)
        XCTAssertEqual(state.selectedAccountId, "offline-player")
        XCTAssertEqual(state.selectedVersionId, "local-dev")
        XCTAssertEqual(state.versions[0].gameExecutable, root.appendingPathComponent("bin/Terralite").path)
        XCTAssertEqual(state.versions[0].serverExecutable, root.appendingPathComponent("bin/TerraliteServer").path)
        XCTAssertEqual(state.versions[0].workingDirectory, source.path)
    }

    func testDefaultStatePrefersSourceLocalBuildExecutables() {
        let root = temporaryDirectory()
        let launcher = root.appendingPathComponent("DerivedData/Debug/TerralieLauncherApp")
        let source = root.appendingPathComponent("source", isDirectory: true)
        let build = source.appendingPathComponent("cmake-build-debug", isDirectory: true)
        try? FileManager.default.createDirectory(at: build, withIntermediateDirectories: true)
        FileManager.default.createFile(atPath: build.appendingPathComponent("Terralite").path, contents: Data())
        FileManager.default.createFile(atPath: build.appendingPathComponent("TerraliteServer").path, contents: Data())

        let state = LauncherState.defaultState(launcherURL: launcher, sourceDirectory: source)

        XCTAssertEqual(state.versions[0].gameExecutable, build.appendingPathComponent("Terralite").path)
        XCTAssertEqual(state.versions[0].serverExecutable, build.appendingPathComponent("TerraliteServer").path)
        XCTAssertEqual(state.versions[0].workingDirectory, source.path)
    }

    func testStateRoundTrip() throws {
        let root = temporaryDirectory()
        let config = root.appendingPathComponent("launcher/launcher.json")
        let launcher = root.appendingPathComponent("bin/TerraliteLauncherSwift")
        let source = root.appendingPathComponent("source", isDirectory: true)
        var state = LauncherState.defaultState(launcherURL: launcher, sourceDirectory: source)

        LauncherCore.addAccount(to: &state)
        LauncherCore.renameSelectedAccount(in: &state, displayName: "Kyle Test")
        LauncherCore.addVersion(to: &state)
        var version = LauncherCore.selectedVersion(in: state)!
        version.id = "stable-0-1"
        version.name = "Stable 0.1"
        version.extraArguments = "--fullscreen --limit-fps 1"
        LauncherCore.updateSelectedVersion(in: &state, with: version)

        try LauncherCore.saveState(state, configURL: config)
        let loaded = LauncherCore.loadState(configURL: config, launcherURL: launcher, sourceDirectory: source)

        XCTAssertEqual(loaded.accounts.count, 2)
        XCTAssertEqual(loaded.selectedAccountId, "kyle-test")
        XCTAssertEqual(loaded.selectedVersionId, "stable-0-1")
        XCTAssertEqual(LauncherCore.selectedVersion(in: loaded)?.extraArguments, "--fullscreen --limit-fps 1")
    }

    func testManifestDiscoveryAndLaunchArguments() throws {
        let root = temporaryDirectory()
        let versionRoot = root.appendingPathComponent("versions", isDirectory: true)
        let release = versionRoot.appendingPathComponent("0.1.0", isDirectory: true)
        try FileManager.default.createDirectory(at: release, withIntermediateDirectories: true)
        FileManager.default.createFile(atPath: release.appendingPathComponent("Terralite").path, contents: Data())
        FileManager.default.createFile(atPath: release.appendingPathComponent("TerraliteServer").path, contents: Data())

        let manifest = VersionManifest(version: GameVersion(
            id: "0.1.0",
            name: "Terralite 0.1.0",
            channel: "stable",
            source: "manifest",
            gameExecutable: "Terralite",
            serverExecutable: "TerraliteServer",
            workingDirectory: ".",
            extraArguments: "--limit-fps 1"
        ))
        let data = try LauncherCore.encoder.encode(manifest)
        try data.write(to: release.appendingPathComponent("manifest.json"))

        let versions = LauncherCore.discoverInstalledVersions(in: versionRoot)
        XCTAssertEqual(versions.count, 1)
        XCTAssertEqual(versions[0].gameExecutable, release.appendingPathComponent("Terralite").path)
        XCTAssertEqual(LauncherCore.versionStatus(versions[0]), .installed)

        let args = LauncherCore.gameArguments(
            account: AccountProfile(id: "offline-player", displayName: "Player"),
            version: versions[0],
            options: LaunchOptions(playMode: .connect, hostName: "example.local", port: "28001")
        )
        XCTAssertEqual(args, ["--limit-fps", "1", "--name", "Player", "--connect", "example.local", "28001"])
    }

    private func temporaryDirectory() -> URL {
        FileManager.default.temporaryDirectory
            .appendingPathComponent("terralite-swift-launcher-tests-\(UUID().uuidString)", isDirectory: true)
    }
}
