import Foundation

public enum LauncherPaths {
    public static var appDataDirectory: URL {
        let support = FileManager.default.urls(for: .applicationSupportDirectory, in: .userDomainMask).first
            ?? URL(fileURLWithPath: NSHomeDirectory()).appendingPathComponent("Library/Application Support")
        return support.appendingPathComponent("TERRALITE/Launcher", isDirectory: true)
    }

    public static var configURL: URL {
        appDataDirectory.appendingPathComponent("launcher.json")
    }

    public static var versionRootURL: URL {
        appDataDirectory.appendingPathComponent("versions", isDirectory: true)
    }

    public static var launcherExecutableURL: URL {
        Bundle.main.executableURL ?? URL(fileURLWithPath: CommandLine.arguments.first ?? "TerraliteLauncherSwift")
    }

    public static var sourceDirectory: URL {
        if let override = ProcessInfo.processInfo.environment["TERRALITE_SOURCE_DIR"], !override.isEmpty {
            return URL(fileURLWithPath: override, isDirectory: true)
        }

        let bundleURL = Bundle.main.bundleURL
        if bundleURL.pathExtension == "app", bundleURL.deletingLastPathComponent().lastPathComponent == "dist" {
            return bundleURL.deletingLastPathComponent().deletingLastPathComponent()
        }

        return URL(fileURLWithPath: FileManager.default.currentDirectoryPath, isDirectory: true)
    }
}
