#include "launcher/LauncherCore.hpp"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace {

using terralite::launcher::AccountProfile;
using terralite::launcher::GameVersion;
using terralite::launcher::LaunchOptions;
using terralite::launcher::LauncherState;
using terralite::launcher::PlayMode;

bool expect(bool condition, const std::string& message) {
    if (!condition) {
        std::cerr << "[FAIL] " << message << '\n';
        return false;
    }
    return true;
}

std::filesystem::path uniqueTempDir() {
    const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
    return std::filesystem::temp_directory_path() / ("terralite-launcher-core-tests-" + std::to_string(stamp));
}

bool expectArgs(
    const std::vector<std::string>& actual,
    const std::vector<std::string>& expected,
    const std::string& message) {
    if (actual == expected) {
        return true;
    }

    std::cerr << "[FAIL] " << message << '\n';
    std::cerr << "  expected:";
    for (const std::string& arg : expected) {
        std::cerr << " [" << arg << "]";
    }
    std::cerr << "\n  actual:  ";
    for (const std::string& arg : actual) {
        std::cerr << " [" << arg << "]";
    }
    std::cerr << '\n';
    return false;
}

}  // namespace

int main() {
    bool ok = true;

    const std::filesystem::path tempDir = uniqueTempDir();
    const std::filesystem::path launcherPath = tempDir / "bin" / "TerraliteLauncher";
    const std::filesystem::path sourceDir = tempDir / "source";
    std::filesystem::create_directories(launcherPath.parent_path());
    std::filesystem::create_directories(sourceDir);

    LauncherState state = terralite::launcher::defaultState(launcherPath, sourceDir);
    ok &= expect(state.accounts.size() == 1, "default state should contain one offline account");
    ok &= expect(state.versions.size() == 1, "default state should contain one local development version");
    ok &= expect(state.selectedAccountId == "offline-player", "default account should be selected");
    ok &= expect(state.selectedVersionId == "local-dev", "default version should be selected");
    ok &= expect(terralite::launcher::selectedAccount(state) != nullptr, "selected account should resolve");
    ok &= expect(terralite::launcher::selectedVersion(state) != nullptr, "selected version should resolve");
    ok &= expect(
        terralite::launcher::selectedVersion(state)->workingDirectory == sourceDir,
        "default version should use supplied source directory");

    terralite::launcher::addAccount(state);
    ok &= expect(state.accounts.size() == 2, "addAccount should append a profile");
    ok &= expect(state.selectedAccountId == "offline-2", "addAccount should select the new profile");

    terralite::launcher::renameSelectedAccount(state, "Kyle Test");
    ok &= expect(state.selectedAccountId == "kyle-test", "renameSelectedAccount should update slug id");
    ok &= expect(
        terralite::launcher::selectedAccount(state)->displayName == "Kyle Test",
        "renameSelectedAccount should update display name");

    terralite::launcher::removeSelectedAccount(state);
    ok &= expect(state.accounts.size() == 1, "removeSelectedAccount should remove selected non-final profile");
    ok &= expect(state.selectedAccountId == "offline-player", "removeSelectedAccount should select remaining profile");
    terralite::launcher::removeSelectedAccount(state);
    ok &= expect(state.accounts.size() == 1, "removeSelectedAccount should keep at least one profile");

    terralite::launcher::addVersion(state);
    ok &= expect(state.versions.size() == 2, "addVersion should append a version");
    ok &= expect(state.selectedVersionId == "version-2", "addVersion should select the new version");

    GameVersion updatedVersion;
    updatedVersion.id = "stable-0-1";
    updatedVersion.name = "Stable 0.1";
    updatedVersion.gameExecutable = tempDir / "versions" / "0.1" / "Terralite";
    updatedVersion.serverExecutable = tempDir / "versions" / "0.1" / "TerraliteServer";
    updatedVersion.workingDirectory = tempDir / "versions" / "0.1";
    updatedVersion.extraArguments = "--fullscreen --limit-fps 1";
    terralite::launcher::updateSelectedVersion(state, updatedVersion);
    ok &= expect(state.selectedVersionId == "stable-0-1", "updateSelectedVersion should update selected id");
    ok &= expect(
        terralite::launcher::selectedVersion(state)->name == "Stable 0.1",
        "updateSelectedVersion should update selected version fields");

    LaunchOptions offlineOptions;
    ok &= expectArgs(
        terralite::launcher::gameArguments(
            *terralite::launcher::selectedAccount(state),
            *terralite::launcher::selectedVersion(state),
            offlineOptions),
        {"--fullscreen", "--limit-fps", "1", "--name", "Player"},
        "offline launch arguments should include extra args and account name");

    LaunchOptions hostOptions;
    hostOptions.playMode = PlayMode::Host;
    hostOptions.port = "28000";
    ok &= expectArgs(
        terralite::launcher::gameArguments(
            *terralite::launcher::selectedAccount(state),
            *terralite::launcher::selectedVersion(state),
            hostOptions),
        {"--fullscreen", "--limit-fps", "1", "--name", "Player", "--host", "28000"},
        "host launch arguments should include host port");

    LaunchOptions connectOptions;
    connectOptions.playMode = PlayMode::Connect;
    connectOptions.hostName = "example.local";
    connectOptions.port = "28001";
    ok &= expectArgs(
        terralite::launcher::gameArguments(
            *terralite::launcher::selectedAccount(state),
            *terralite::launcher::selectedVersion(state),
            connectOptions),
        {"--fullscreen", "--limit-fps", "1", "--name", "Player", "--connect", "example.local", "28001"},
        "connect launch arguments should include host and port");

    const std::filesystem::path configPath = tempDir / "launcher" / "launcher.json";
    try {
        terralite::launcher::saveState(state, configPath);
        LauncherState loaded = terralite::launcher::loadState(configPath, launcherPath, sourceDir);
        ok &= expect(loaded.accounts.size() == state.accounts.size(), "loadState should preserve account count");
        ok &= expect(loaded.versions.size() == state.versions.size(), "loadState should preserve version count");
        ok &= expect(loaded.selectedVersionId == "stable-0-1", "loadState should preserve selected version id");
        ok &= expect(
            terralite::launcher::selectedVersion(loaded)->extraArguments == "--fullscreen --limit-fps 1",
            "loadState should preserve extra arguments");
    } catch (const std::exception& error) {
        std::cerr << "[FAIL] save/load round trip should not throw: " << error.what() << '\n';
        ok = false;
    }

    {
        std::ofstream broken(configPath);
        broken << "{ not valid json";
    }
    LauncherState fallback = terralite::launcher::loadState(configPath, launcherPath, sourceDir);
    ok &= expect(fallback.accounts.size() == 1, "invalid config should fall back to default account");
    ok &= expect(fallback.versions.size() == 1, "invalid config should fall back to default version");
    ok &= expect(fallback.selectedVersionId == "local-dev", "invalid config should select default version");

    std::error_code cleanupError;
    std::filesystem::remove_all(tempDir, cleanupError);

    if (!ok) {
        return 1;
    }

    std::cout << "Launcher core tests passed.\n";
    return 0;
}
