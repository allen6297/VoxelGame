#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace terralite::launcher {

struct AccountProfile {
    std::string id;
    std::string displayName;
    std::string type = "offline";
};

struct GameVersion {
    std::string id;
    std::string name;
    std::filesystem::path gameExecutable;
    std::filesystem::path serverExecutable;
    std::filesystem::path workingDirectory;
    std::string extraArguments;
};

struct LauncherState {
    std::vector<AccountProfile> accounts;
    std::vector<GameVersion> versions;
    std::string selectedAccountId;
    std::string selectedVersionId;
};

enum class PlayMode {
    Offline,
    Host,
    Connect,
};

struct LaunchOptions {
    PlayMode playMode = PlayMode::Offline;
    std::string hostName = "127.0.0.1";
    std::string port = "27015";
};

std::filesystem::path executablePath(const char* argv0);
std::filesystem::path appDataDirectory();
std::filesystem::path siblingExecutable(const std::filesystem::path& launcherPath, const std::string& name);
std::string pathString(const std::filesystem::path& path);
std::string slugFromName(const std::string& name);

LauncherState defaultState(const std::filesystem::path& launcherPath, const std::filesystem::path& sourceDirectory);
LauncherState loadState(
    const std::filesystem::path& path,
    const std::filesystem::path& launcherPath,
    const std::filesystem::path& sourceDirectory);
void saveState(const LauncherState& state, const std::filesystem::path& path);

AccountProfile* selectedAccount(LauncherState& state);
const AccountProfile* selectedAccount(const LauncherState& state);
GameVersion* selectedVersion(LauncherState& state);
const GameVersion* selectedVersion(const LauncherState& state);

AccountProfile addAccount(LauncherState& state);
void removeSelectedAccount(LauncherState& state);
GameVersion addVersion(LauncherState& state);
void removeSelectedVersion(LauncherState& state);
void renameSelectedAccount(LauncherState& state, const std::string& displayName);
void updateSelectedVersion(LauncherState& state, const GameVersion& updatedVersion);

std::vector<std::string> gameArguments(
    const AccountProfile& account,
    const GameVersion& version,
    const LaunchOptions& options);
std::string launchGame(const AccountProfile& account, const GameVersion& version, const LaunchOptions& options);
std::string launchServer(const GameVersion& version, const std::string& port);

}  // namespace terralite::launcher
