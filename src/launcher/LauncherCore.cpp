#include "launcher/LauncherCore.hpp"

#include "data/JsonValue.hpp"

#include <algorithm>
#include <array>
#include <cerrno>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <sstream>
#include <stdexcept>

#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#else
#include <limits.h>
#include <unistd.h>
#ifdef __APPLE__
#include <mach-o/dyld.h>
#endif
#endif

namespace terralite::launcher {
namespace {

const voxel::JsonValue* objectValue(const voxel::JsonValue::Object& object, const std::string& key) {
    const auto it = object.find(key);
    return it == object.end() ? nullptr : &it->second;
}

std::string stringValue(const voxel::JsonValue::Object& object, const std::string& key, const std::string& fallback = {}) {
    const voxel::JsonValue* value = objectValue(object, key);
    return value != nullptr && value->isString() ? value->asString() : fallback;
}

voxel::JsonValue jsonString(const std::string& value) {
    return voxel::JsonValue {value};
}

voxel::JsonValue accountToJson(const AccountProfile& account) {
    voxel::JsonValue::Object object;
    object["displayName"] = jsonString(account.displayName);
    object["id"] = jsonString(account.id);
    object["type"] = jsonString(account.type);
    return voxel::JsonValue {object};
}

voxel::JsonValue versionToJson(const GameVersion& version) {
    voxel::JsonValue::Object object;
    object["extraArguments"] = jsonString(version.extraArguments);
    object["gameExecutable"] = jsonString(pathString(version.gameExecutable));
    object["id"] = jsonString(version.id);
    object["name"] = jsonString(version.name);
    object["serverExecutable"] = jsonString(pathString(version.serverExecutable));
    object["workingDirectory"] = jsonString(pathString(version.workingDirectory));
    return voxel::JsonValue {object};
}

std::vector<std::string> splitExtraArguments(const std::string& input) {
    std::istringstream stream(input);
    std::vector<std::string> result;
    std::string part;
    while (stream >> part) {
        result.push_back(part);
    }
    return result;
}

std::string launchProcess(
    const std::filesystem::path& executable,
    const std::filesystem::path& workingDirectory,
    const std::vector<std::string>& arguments) {
    if (executable.empty()) {
        return "No executable is configured.";
    }
    if (!std::filesystem::exists(executable)) {
        return "Executable not found: " + executable.string();
    }
    if (!workingDirectory.empty() && !std::filesystem::exists(workingDirectory)) {
        return "Working directory not found: " + workingDirectory.string();
    }

#ifdef _WIN32
    std::string command = "\"" + executable.string() + "\"";
    for (const std::string& argument : arguments) {
        command += " \"" + argument + "\"";
    }

    STARTUPINFOA startup {};
    startup.cb = sizeof(startup);
    PROCESS_INFORMATION process {};
    std::string working = workingDirectory.empty() ? executable.parent_path().string() : workingDirectory.string();
    if (!CreateProcessA(
            nullptr,
            command.data(),
            nullptr,
            nullptr,
            FALSE,
            0,
            nullptr,
            working.c_str(),
            &startup,
            &process)) {
        return "Failed to start process.";
    }
    CloseHandle(process.hThread);
    CloseHandle(process.hProcess);
#else
    std::vector<std::string> ownedArgs;
    ownedArgs.push_back(executable.string());
    ownedArgs.insert(ownedArgs.end(), arguments.begin(), arguments.end());

    std::vector<char*> argv;
    argv.reserve(ownedArgs.size() + 1);
    for (std::string& argument : ownedArgs) {
        argv.push_back(argument.data());
    }
    argv.push_back(nullptr);

    const std::filesystem::path working = workingDirectory.empty() ? executable.parent_path() : workingDirectory;
    const pid_t pid = fork();
    if (pid == 0) {
        if (chdir(working.c_str()) != 0) {
            _exit(127);
        }
        execv(executable.c_str(), argv.data());
        _exit(127);
    }
    if (pid < 0) {
        return std::string("Failed to start process: ") + std::strerror(errno);
    }
#endif
    return "Started " + executable.filename().string();
}

}  // namespace

std::filesystem::path executablePath(const char* argv0) {
#ifdef _WIN32
    std::array<char, MAX_PATH> path {};
    const DWORD length = GetModuleFileNameA(nullptr, path.data(), static_cast<DWORD>(path.size()));
    if (length > 0) {
        return std::filesystem::path(std::string(path.data(), length));
    }
#elif defined(__APPLE__)
    std::array<char, PATH_MAX> path {};
    uint32_t size = static_cast<uint32_t>(path.size());
    if (_NSGetExecutablePath(path.data(), &size) == 0) {
        return std::filesystem::weakly_canonical(path.data());
    }
#else
    std::array<char, PATH_MAX> path {};
    const ssize_t length = readlink("/proc/self/exe", path.data(), path.size() - 1);
    if (length > 0) {
        path[static_cast<std::size_t>(length)] = '\0';
        return std::filesystem::path(path.data());
    }
#endif
    return std::filesystem::absolute(argv0);
}

std::filesystem::path appDataDirectory() {
#ifdef _WIN32
    if (const char* appdata = std::getenv("APPDATA")) {
        return std::filesystem::path(appdata) / "TERRALITE" / "Launcher";
    }
#elif defined(__APPLE__)
    if (const char* home = std::getenv("HOME")) {
        return std::filesystem::path(home) / "Library" / "Application Support" / "TERRALITE" / "Launcher";
    }
#else
    if (const char* xdg = std::getenv("XDG_DATA_HOME")) {
        return std::filesystem::path(xdg) / "TERRALITE" / "Launcher";
    }
    if (const char* home = std::getenv("HOME")) {
        return std::filesystem::path(home) / ".local" / "share" / "TERRALITE" / "Launcher";
    }
#endif
    return std::filesystem::current_path() / "launcher-data";
}

std::filesystem::path siblingExecutable(const std::filesystem::path& launcherPath, const std::string& name) {
#ifdef _WIN32
    return launcherPath.parent_path() / (name + ".exe");
#else
    return launcherPath.parent_path() / name;
#endif
}

std::string pathString(const std::filesystem::path& path) {
    return path.lexically_normal().string();
}

std::string slugFromName(const std::string& name) {
    std::string slug;
    for (const char c : name) {
        if (std::isalnum(static_cast<unsigned char>(c))) {
            slug.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
        } else if (!slug.empty() && slug.back() != '-') {
            slug.push_back('-');
        }
    }
    while (!slug.empty() && slug.back() == '-') {
        slug.pop_back();
    }
    return slug.empty() ? "player" : slug;
}

LauncherState defaultState(const std::filesystem::path& launcherPath, const std::filesystem::path& sourceDirectory) {
    LauncherState state;
    state.accounts.push_back(AccountProfile {
        .id = "offline-player",
        .displayName = "Player",
        .type = "offline",
    });

    state.versions.push_back(GameVersion {
        .id = "local-dev",
        .name = "Local development build",
        .gameExecutable = siblingExecutable(launcherPath, "Terralite"),
        .serverExecutable = siblingExecutable(launcherPath, "TerraliteServer"),
        .workingDirectory = sourceDirectory,
        .extraArguments = "",
    });

    state.selectedAccountId = state.accounts.front().id;
    state.selectedVersionId = state.versions.front().id;
    return state;
}

void saveState(const LauncherState& state, const std::filesystem::path& path) {
    std::filesystem::create_directories(path.parent_path());

    voxel::JsonValue::Array accounts;
    for (const AccountProfile& account : state.accounts) {
        accounts.push_back(accountToJson(account));
    }

    voxel::JsonValue::Array versions;
    for (const GameVersion& version : state.versions) {
        versions.push_back(versionToJson(version));
    }

    voxel::JsonValue::Object root;
    root["accounts"] = voxel::JsonValue {accounts};
    root["selectedAccountId"] = jsonString(state.selectedAccountId);
    root["selectedVersionId"] = jsonString(state.selectedVersionId);
    root["versions"] = voxel::JsonValue {versions};

    std::ofstream file(path);
    if (!file) {
        throw std::runtime_error("Could not write launcher state: " + path.string());
    }
    file << voxel::serializeJson(voxel::JsonValue {root}) << '\n';
}

LauncherState loadState(
    const std::filesystem::path& path,
    const std::filesystem::path& launcherPath,
    const std::filesystem::path& sourceDirectory) {
    if (!std::filesystem::exists(path)) {
        return defaultState(launcherPath, sourceDirectory);
    }

    try {
        std::ifstream file(path);
        std::stringstream buffer;
        buffer << file.rdbuf();

        LauncherState state;
        const voxel::JsonValue root = voxel::parseJson(buffer.str());
        const auto& object = root.asObject();

        if (const voxel::JsonValue* accounts = objectValue(object, "accounts");
            accounts != nullptr && accounts->isArray()) {
            for (const voxel::JsonValue& value : accounts->asArray()) {
                if (!value.isObject()) continue;
                const auto& accountObject = value.asObject();
                AccountProfile account;
                account.id = stringValue(accountObject, "id");
                account.displayName = stringValue(accountObject, "displayName", account.id);
                account.type = stringValue(accountObject, "type", "offline");
                if (!account.id.empty()) {
                    state.accounts.push_back(account);
                }
            }
        }

        if (const voxel::JsonValue* versions = objectValue(object, "versions");
            versions != nullptr && versions->isArray()) {
            for (const voxel::JsonValue& value : versions->asArray()) {
                if (!value.isObject()) continue;
                const auto& versionObject = value.asObject();
                GameVersion version;
                version.id = stringValue(versionObject, "id");
                version.name = stringValue(versionObject, "name", version.id);
                version.gameExecutable = stringValue(versionObject, "gameExecutable");
                version.serverExecutable = stringValue(versionObject, "serverExecutable");
                version.workingDirectory = stringValue(versionObject, "workingDirectory");
                version.extraArguments = stringValue(versionObject, "extraArguments");
                if (!version.id.empty()) {
                    state.versions.push_back(version);
                }
            }
        }

        if (state.accounts.empty() || state.versions.empty()) {
            return defaultState(launcherPath, sourceDirectory);
        }

        state.selectedAccountId = stringValue(object, "selectedAccountId", state.accounts.front().id);
        state.selectedVersionId = stringValue(object, "selectedVersionId", state.versions.front().id);
        return state;
    } catch (const std::exception&) {
        return defaultState(launcherPath, sourceDirectory);
    }
}

AccountProfile* selectedAccount(LauncherState& state) {
    auto it = std::find_if(state.accounts.begin(), state.accounts.end(), [&](const AccountProfile& account) {
        return account.id == state.selectedAccountId;
    });
    return it == state.accounts.end() ? nullptr : &*it;
}

const AccountProfile* selectedAccount(const LauncherState& state) {
    auto it = std::find_if(state.accounts.begin(), state.accounts.end(), [&](const AccountProfile& account) {
        return account.id == state.selectedAccountId;
    });
    return it == state.accounts.end() ? nullptr : &*it;
}

GameVersion* selectedVersion(LauncherState& state) {
    auto it = std::find_if(state.versions.begin(), state.versions.end(), [&](const GameVersion& version) {
        return version.id == state.selectedVersionId;
    });
    return it == state.versions.end() ? nullptr : &*it;
}

const GameVersion* selectedVersion(const LauncherState& state) {
    auto it = std::find_if(state.versions.begin(), state.versions.end(), [&](const GameVersion& version) {
        return version.id == state.selectedVersionId;
    });
    return it == state.versions.end() ? nullptr : &*it;
}

AccountProfile addAccount(LauncherState& state) {
    const std::string id = "offline-" + std::to_string(state.accounts.size() + 1);
    AccountProfile account {
        .id = id,
        .displayName = "Player " + std::to_string(state.accounts.size() + 1),
        .type = "offline",
    };
    state.accounts.push_back(account);
    state.selectedAccountId = id;
    return account;
}

void removeSelectedAccount(LauncherState& state) {
    if (state.accounts.size() <= 1) return;
    state.accounts.erase(std::remove_if(state.accounts.begin(), state.accounts.end(), [&](const AccountProfile& account) {
        return account.id == state.selectedAccountId;
    }), state.accounts.end());
    state.selectedAccountId = state.accounts.front().id;
}

GameVersion addVersion(LauncherState& state) {
    const std::string id = "version-" + std::to_string(state.versions.size() + 1);
    const GameVersion* current = selectedVersion(state);
    GameVersion version = current != nullptr ? *current : GameVersion {};
    version.id = id;
    version.name = "New Version";
    state.versions.push_back(version);
    state.selectedVersionId = id;
    return version;
}

void removeSelectedVersion(LauncherState& state) {
    if (state.versions.size() <= 1) return;
    state.versions.erase(std::remove_if(state.versions.begin(), state.versions.end(), [&](const GameVersion& version) {
        return version.id == state.selectedVersionId;
    }), state.versions.end());
    state.selectedVersionId = state.versions.front().id;
}

void renameSelectedAccount(LauncherState& state, const std::string& displayName) {
    AccountProfile* account = selectedAccount(state);
    if (account == nullptr) return;
    account->displayName = displayName;
    account->id = slugFromName(account->displayName);
    state.selectedAccountId = account->id;
}

void updateSelectedVersion(LauncherState& state, const GameVersion& updatedVersion) {
    GameVersion* version = selectedVersion(state);
    if (version == nullptr) return;
    *version = updatedVersion;
    state.selectedVersionId = version->id;
}

std::vector<std::string> gameArguments(
    const AccountProfile& account,
    const GameVersion& version,
    const LaunchOptions& options) {
    std::vector<std::string> arguments = splitExtraArguments(version.extraArguments);
    arguments.push_back("--name");
    arguments.push_back(account.displayName);

    if (options.playMode == PlayMode::Host) {
        arguments.push_back("--host");
        arguments.push_back(options.port);
    } else if (options.playMode == PlayMode::Connect) {
        arguments.push_back("--connect");
        arguments.push_back(options.hostName);
        arguments.push_back(options.port);
    }

    return arguments;
}

std::string launchGame(const AccountProfile& account, const GameVersion& version, const LaunchOptions& options) {
    return launchProcess(
        version.gameExecutable,
        version.workingDirectory,
        gameArguments(account, version, options));
}

std::string launchServer(const GameVersion& version, const std::string& port) {
    return launchProcess(
        version.serverExecutable,
        version.workingDirectory,
        {"--port", port});
}

}  // namespace terralite::launcher
