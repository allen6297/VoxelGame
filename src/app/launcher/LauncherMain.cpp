#include "launcher/LauncherCore.hpp"

#include <GLFW/glfw3.h>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_opengl2.h>
#include <imgui.h>

#include <array>
#include <cstdio>
#include <filesystem>
#include <stdexcept>
#include <string>

#ifndef TERRALITE_SOURCE_DIR
#define TERRALITE_SOURCE_DIR "."
#endif

namespace {

using terralite::launcher::AccountProfile;
using terralite::launcher::GameVersion;
using terralite::launcher::LaunchOptions;
using terralite::launcher::LauncherState;
using terralite::launcher::PlayMode;

struct LauncherRuntime {
    LaunchOptions launchOptions;
    std::array<char, 96> accountName {};
    std::array<char, 64> serverHost {};
    std::array<char, 16> port {};
    std::array<char, 64> versionId {};
    std::array<char, 96> versionName {};
    std::array<char, 512> gameExecutable {};
    std::array<char, 512> serverExecutable {};
    std::array<char, 512> workingDirectory {};
    std::array<char, 256> extraArguments {};
    std::string loadedVersionId;
    std::string loadedAccountId;
    std::string status;
};

void copyToBuffer(const std::string& text, auto& buffer) {
    std::snprintf(buffer.data(), buffer.size(), "%s", text.c_str());
}

void syncRuntimeBuffers(LauncherState& state, LauncherRuntime& runtime) {
    if (runtime.port[0] == '\0') {
        copyToBuffer(runtime.launchOptions.port, runtime.port);
    }
    if (runtime.serverHost[0] == '\0') {
        copyToBuffer(runtime.launchOptions.hostName, runtime.serverHost);
    }

    if (AccountProfile* account = terralite::launcher::selectedAccount(state);
        account != nullptr && runtime.loadedAccountId != account->id) {
        copyToBuffer(account->displayName, runtime.accountName);
        runtime.loadedAccountId = account->id;
    }

    if (GameVersion* version = terralite::launcher::selectedVersion(state);
        version != nullptr && runtime.loadedVersionId != version->id) {
        copyToBuffer(version->id, runtime.versionId);
        copyToBuffer(version->name, runtime.versionName);
        copyToBuffer(terralite::launcher::pathString(version->gameExecutable), runtime.gameExecutable);
        copyToBuffer(terralite::launcher::pathString(version->serverExecutable), runtime.serverExecutable);
        copyToBuffer(terralite::launcher::pathString(version->workingDirectory), runtime.workingDirectory);
        copyToBuffer(version->extraArguments, runtime.extraArguments);
        runtime.loadedVersionId = version->id;
    }
}

void saveSelectedAccount(LauncherState& state, LauncherRuntime& runtime) {
    terralite::launcher::renameSelectedAccount(state, runtime.accountName.data());
    if (const AccountProfile* account = terralite::launcher::selectedAccount(state)) {
        runtime.loadedAccountId = account->id;
    }
}

void saveSelectedVersion(LauncherState& state, LauncherRuntime& runtime) {
    GameVersion updated;
    updated.id = runtime.versionId.data();
    updated.name = runtime.versionName.data();
    updated.gameExecutable = runtime.gameExecutable.data();
    updated.serverExecutable = runtime.serverExecutable.data();
    updated.workingDirectory = runtime.workingDirectory.data();
    updated.extraArguments = runtime.extraArguments.data();
    terralite::launcher::updateSelectedVersion(state, updated);
    runtime.loadedVersionId = updated.id;
}

LaunchOptions launchOptionsFromRuntime(LauncherRuntime& runtime) {
    runtime.launchOptions.hostName = runtime.serverHost.data();
    runtime.launchOptions.port = runtime.port.data();
    return runtime.launchOptions;
}

void drawAccounts(LauncherState& state, LauncherRuntime& runtime, const std::filesystem::path& configPath) {
    ImGui::TextUnformatted("Accounts");
    ImGui::Separator();
    for (const AccountProfile& account : state.accounts) {
        const bool selected = account.id == state.selectedAccountId;
        if (ImGui::Selectable(account.displayName.c_str(), selected)) {
            state.selectedAccountId = account.id;
            runtime.loadedAccountId.clear();
        }
    }

    if (ImGui::Button("Add account")) {
        terralite::launcher::addAccount(state);
        runtime.loadedAccountId.clear();
        terralite::launcher::saveState(state, configPath);
    }
    ImGui::SameLine();
    if (ImGui::Button("Remove") && state.accounts.size() > 1) {
        terralite::launcher::removeSelectedAccount(state);
        runtime.loadedAccountId.clear();
        terralite::launcher::saveState(state, configPath);
    }

    ImGui::Spacing();
    ImGui::InputText("Display name", runtime.accountName.data(), runtime.accountName.size());
    if (ImGui::Button("Save account")) {
        saveSelectedAccount(state, runtime);
        terralite::launcher::saveState(state, configPath);
        runtime.status = "Saved account.";
    }
}

void drawVersions(LauncherState& state, LauncherRuntime& runtime, const std::filesystem::path& configPath) {
    ImGui::TextUnformatted("Game Versions");
    ImGui::Separator();
    for (const GameVersion& version : state.versions) {
        const bool selected = version.id == state.selectedVersionId;
        if (ImGui::Selectable(version.name.c_str(), selected)) {
            state.selectedVersionId = version.id;
            runtime.loadedVersionId.clear();
        }
    }

    if (ImGui::Button("Add version")) {
        terralite::launcher::addVersion(state);
        runtime.loadedVersionId.clear();
        terralite::launcher::saveState(state, configPath);
    }
    ImGui::SameLine();
    if (ImGui::Button("Remove version") && state.versions.size() > 1) {
        terralite::launcher::removeSelectedVersion(state);
        runtime.loadedVersionId.clear();
        terralite::launcher::saveState(state, configPath);
    }

    ImGui::Spacing();
    ImGui::InputText("Version id", runtime.versionId.data(), runtime.versionId.size());
    ImGui::InputText("Name", runtime.versionName.data(), runtime.versionName.size());
    ImGui::InputText("Game executable", runtime.gameExecutable.data(), runtime.gameExecutable.size());
    ImGui::InputText("Server executable", runtime.serverExecutable.data(), runtime.serverExecutable.size());
    ImGui::InputText("Working directory", runtime.workingDirectory.data(), runtime.workingDirectory.size());
    ImGui::InputText("Extra args", runtime.extraArguments.data(), runtime.extraArguments.size());
    if (ImGui::Button("Save version")) {
        saveSelectedVersion(state, runtime);
        terralite::launcher::saveState(state, configPath);
        runtime.status = "Saved version.";
    }
}

void drawLaunchPanel(LauncherState& state, LauncherRuntime& runtime) {
    AccountProfile* account = terralite::launcher::selectedAccount(state);
    GameVersion* version = terralite::launcher::selectedVersion(state);

    ImGui::TextUnformatted("Launch");
    ImGui::Separator();
    int playMode = static_cast<int>(runtime.launchOptions.playMode);
    ImGui::RadioButton("Offline", &playMode, static_cast<int>(PlayMode::Offline));
    ImGui::SameLine();
    ImGui::RadioButton("Host", &playMode, static_cast<int>(PlayMode::Host));
    ImGui::SameLine();
    ImGui::RadioButton("Connect", &playMode, static_cast<int>(PlayMode::Connect));
    runtime.launchOptions.playMode = static_cast<PlayMode>(playMode);

    ImGui::InputText("Host", runtime.serverHost.data(), runtime.serverHost.size());
    ImGui::InputText("Port", runtime.port.data(), runtime.port.size());

    if (ImGui::Button("Launch game", ImVec2(160, 0)) && account != nullptr && version != nullptr) {
        runtime.status = terralite::launcher::launchGame(*account, *version, launchOptionsFromRuntime(runtime));
    }
    ImGui::SameLine();
    if (ImGui::Button("Start server", ImVec2(160, 0)) && version != nullptr) {
        const LaunchOptions options = launchOptionsFromRuntime(runtime);
        runtime.status = terralite::launcher::launchServer(*version, options.port);
    }

    if (!runtime.status.empty()) {
        ImGui::Spacing();
        ImGui::TextWrapped("%s", runtime.status.c_str());
    }
}

void drawLauncher(LauncherState& state, LauncherRuntime& runtime, const std::filesystem::path& configPath) {
    syncRuntimeBuffers(state, runtime);

    ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize, ImGuiCond_Always);
    ImGui::Begin("Terralite Launcher", nullptr,
        ImGuiWindowFlags_NoDecoration |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoSavedSettings);

    ImGui::TextUnformatted("Terralite Launcher");
    ImGui::TextDisabled("%s", configPath.string().c_str());
    ImGui::Separator();

    ImGui::Columns(3, "launcher-columns", true);
    drawAccounts(state, runtime, configPath);
    ImGui::NextColumn();
    drawVersions(state, runtime, configPath);
    ImGui::NextColumn();
    drawLaunchPanel(state, runtime);
    ImGui::Columns(1);

    ImGui::End();
}

class ImGuiGlfwApp {
public:
    ImGuiGlfwApp() {
        if (!glfwInit()) {
            throw std::runtime_error("Failed to initialize GLFW.");
        }
        window_ = glfwCreateWindow(1040, 680, "Terralite Launcher", nullptr, nullptr);
        if (window_ == nullptr) {
            glfwTerminate();
            throw std::runtime_error("Failed to create launcher window.");
        }
        glfwMakeContextCurrent(window_);
        glfwSwapInterval(1);

        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGui::StyleColorsDark();
        ImGui_ImplGlfw_InitForOpenGL(window_, true);
        ImGui_ImplOpenGL2_Init();
    }

    ~ImGuiGlfwApp() {
        ImGui_ImplOpenGL2_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
        if (window_ != nullptr) {
            glfwDestroyWindow(window_);
        }
        glfwTerminate();
    }

    GLFWwindow* window() const { return window_; }

private:
    GLFWwindow* window_ = nullptr;
};

}  // namespace

int main(int argc, char** argv) {
    try {
        const std::filesystem::path launcherPath =
            terralite::launcher::executablePath(argc > 0 ? argv[0] : "TerraliteLauncher");
        const std::filesystem::path configPath =
            terralite::launcher::appDataDirectory() / "launcher.json";

        LauncherState state = terralite::launcher::loadState(
            configPath,
            launcherPath,
            std::filesystem::path(TERRALITE_SOURCE_DIR));
        LauncherRuntime runtime;

        ImGuiGlfwApp app;
        while (!glfwWindowShouldClose(app.window())) {
            glfwPollEvents();
            ImGui_ImplOpenGL2_NewFrame();
            ImGui_ImplGlfw_NewFrame();
            ImGui::NewFrame();

            drawLauncher(state, runtime, configPath);

            ImGui::Render();
            int width = 0;
            int height = 0;
            glfwGetFramebufferSize(app.window(), &width, &height);
            glViewport(0, 0, width, height);
            glClearColor(0.08f, 0.09f, 0.10f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT);
            ImGui_ImplOpenGL2_RenderDrawData(ImGui::GetDrawData());
            glfwSwapBuffers(app.window());
        }

        terralite::launcher::saveState(state, configPath);
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "Fatal launcher error: %s\n", error.what());
        return 1;
    }
}
