#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <string>
#include <thread>

#include "app/client/ClientNetworkSession.hpp"
#include "app/client/ClientOptions.hpp"
#include "app/client/ClientRuntimeBridge.hpp"
#include "app/client/ClientUiController.hpp"
#include "common/data/GameData.hpp"
#include "client/game/Game.hpp"
#include "common/network/NetworkManager.hpp"
#include "client/pack/AssetPackManager.hpp"
#include "client/render/Mesh.hpp"
#include "client/render/OpenGLRenderBackend.hpp"
#include "client/render/TextureManager.hpp"
#include "common/pack/PackManager.hpp"
#include "common/pack/ScriptManager.hpp"
#include "server/HeadlessServer.hpp"
#include "server/ServerBootstrap.hpp"
#include "platform/glfw/GlfwClientWindow.hpp"
#include "platform/glfw/GlfwInput.hpp"
#include "client/ui/GameUI.hpp"
#if TERRALITE_ENABLE_DILIGENT
#    include "client/render/DiligentRenderBackend.hpp"
#endif

namespace {
constexpr std::uint16_t kDefaultMultiplayerPort = 27015;
std::atomic_bool gServerRunning {true};

void handleServerSignal(int) {
    gServerRunning = false;
}

std::filesystem::path findClientProjectRoot() {
    std::filesystem::path current = std::filesystem::current_path();

    while (!current.empty()) {
        if (std::filesystem::exists(current / "packs")) {
            return current;
        }

        const auto parent = current.parent_path();
        if (parent == current) break;
        current = parent;
    }

    throw std::runtime_error("Could not find project root containing packs/");
}

std::uint16_t parsePort(const char* value) {
    const int port = std::stoi(value);
    if (port < 0 || port > 65535) {
        throw std::runtime_error("Port must be between 0 and 65535.");
    }
    return static_cast<std::uint16_t>(port);
}

bool isDedicatedServerMode(int argc, char** argv) {
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--server" || arg == "--dedicated-server") {
            return true;
        }
    }
    return false;
}

int runDedicatedServer(int argc, char** argv) {
    std::uint16_t port = kDefaultMultiplayerPort;

    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--server" || arg == "--dedicated-server") {
            continue;
        }
        if (arg == "--port") {
            if (i + 1 >= argc) {
                throw std::runtime_error("--port requires a value.");
            }
            port = parsePort(argv[++i]);
            continue;
        }
        throw std::runtime_error("Unknown dedicated server argument: " + arg);
    }

    std::signal(SIGINT, handleServerSignal);
    std::signal(SIGTERM, handleServerSignal);

    const std::filesystem::path projectRoot = voxel::findProjectRoot();
    voxel::ServerContext context = voxel::loadServerContext(projectRoot);
    voxel::HeadlessServer server(std::move(context.gameData));
    if (!server.start(port, projectRoot)) {
        return 1;
    }

    std::cout << "Terralite dedicated server running on port " << port << ". Press Ctrl+C to stop.\n";
    while (gServerRunning) {
        server.tick();
        std::this_thread::sleep_for(std::chrono::milliseconds(8));
    }
    return 0;
}

int runDiligentProof(voxel::GlfwClientWindow& window, const int frameLimit) {
#if TERRALITE_ENABLE_DILIGENT
    int fbWidth = 0, fbHeight = 0;
    window.framebufferSize(fbWidth, fbHeight);

    voxel::DiligentRenderBackend backend;
    backend.initialize(window.handle(), fbWidth, fbHeight);
    voxel::TextureManager proofTextures(backend);
    voxel::ChunkMesh proofMesh;
    proofMesh.surfaces.push_back({});
    proofMesh.surfaces.back().vertices = {
        {{-0.5f, -0.5f, 0.0f}, {0.0f, 0.0f, 1.0f}, {1.0f, 1.0f, 1.0f}, 0.0f, 0.0f},
        {{ 0.5f, -0.5f, 0.0f}, {0.0f, 0.0f, 1.0f}, {1.0f, 1.0f, 1.0f}, 1.0f, 0.0f},
        {{ 0.0f,  0.5f, 0.0f}, {0.0f, 0.0f, 1.0f}, {1.0f, 1.0f, 1.0f}, 0.5f, 1.0f}
    };
    const std::array<unsigned char, 4> proofPixel {255, 255, 255, 255};
    const bool proofMeshUploaded = backend.uploadChunkMeshSurface(proofMesh, 0);
    voxel::RenderTextureHandle proofTexture = backend.createTexture2D(
        1,
        1,
        4,
        proofPixel.data()
    );
    if (!proofMeshUploaded || !proofMesh.surfaces.front().vertexBuffer.isValid() || !proofTexture.isValid()) {
        throw std::runtime_error("Diligent proof-of-life resource creation failed.");
    }
    std::cout << "Running " << backend.name() << " proof-of-life draw loop. Close the window to exit.\n";

    int renderedFrames = 0;
    while (!window.shouldClose() && (frameLimit <= 0 || renderedFrames < frameLimit)) {
        window.framebufferSize(fbWidth, fbHeight);
        backend.beginFrame(fbWidth, fbHeight, {0.08f, 0.10f, 0.14f});

        const float aspect = fbHeight > 0 ? static_cast<float>(fbWidth) / static_cast<float>(fbHeight) : 1.f;
        backend.setPerspective(60.f, aspect, 0.1f, 100.f);
        backend.applyCameraView({0.f, 0.f, -3.f}, {0.f, 0.f, 1.f});
        backend.renderMesh(proofMesh, proofTextures);

        backend.endFrame();
        window.pollEvents();
        ++renderedFrames;
    }
    backend.destroyChunkMesh(proofMesh);
    backend.destroyTexture(proofTexture);
    return 0;
#else
    (void)window;
    throw std::runtime_error("--diligent-proof requires TERRALITE_ENABLE_DILIGENT=ON.");
#endif
}
}  // namespace

int main(int argc, char** argv) {
    try {
        if (isDedicatedServerMode(argc, argv)) {
            return runDedicatedServer(argc, argv);
        }
    } catch (const std::exception& error) {
        std::cerr << "Fatal server error: " << error.what() << '\n';
        return 1;
    }

    try {
        const voxel::ClientOptions clientOptions = voxel::parseClientOptions(argc, argv);
        voxel::GlfwClientWindow window(clientOptions.window);
        if (clientOptions.diligentProof) {
            return runDiligentProof(window, clientOptions.diligentProofFrames);
        }

        // ── Pack system ───────────────────────────────────────────────────────
        const std::filesystem::path projectRoot = findClientProjectRoot();
        const std::filesystem::path packsDir    = projectRoot / "packs";

        voxel::PackManager packManager;
        packManager.discover(packsDir);

        const voxel::Pack* basePack = packManager.findPack("base");
        if (!basePack) throw std::runtime_error("Required 'base' pack not found in packs/");

        voxel::AssetPackManager assetManager(packManager);

        // Derive legacy path strings from the base pack for Game and GameUI.
        // These will be replaced with AssetPackManager calls when ScriptManager
        // is integrated and the internals of Game/GameUI are migrated.
        const std::string assetsRoot = (basePack->path() / "assets").string();

        // ── Game data ─────────────────────────────────────────────────────────
        voxel::ScriptManager scriptManager;
        scriptManager.setHostKind(voxel::ScriptHost::Client);
        voxel::GameData gameData = scriptManager.loadGameData(
            packManager, projectRoot / "engine" / "scripts");

        // ── Game & UI ─────────────────────────────────────────────────────────
        int fbWidth = 0, fbHeight = 0;
        window.framebufferSize(fbWidth, fbHeight);

        voxel::NetworkManager network;
        voxel::NetworkManager* activeNetwork =
            voxel::startClientNetworkSession(network, clientOptions.network);

        voxel::OpenGLRenderBackend glBackend;
        voxel::Game   game(glBackend, std::move(gameData), assetsRoot, clientOptions.playerName, activeNetwork);
        voxel::GameUI ui(window.handle(), fbWidth, fbHeight, assetsRoot);
        voxel::ClientRuntimeBridge runtimeBridge(
            scriptManager, network, activeNetwork, game, ui, clientOptions.playerName);
        runtimeBridge.loadRuntimeScripts(packManager);
        voxel::ClientUiController uiController(window, ui);

        window.installUiCallbacks(ui);

        auto  lastTime   = window.time();
        voxel::GlfwInputCollector inputCollector;

        ui.setGameData(&game.gameData());
        {
            std::vector<voxel::RecipeDefinition> recipes;
            for (auto const& [id, recipe] : game.gameData().recipes) recipes.push_back(recipe);
            ui.setRecipes(recipes);
        }

        while (!window.shouldClose()) {
            const auto  currentTime = window.time();
            const float rawDelta    = currentTime - lastTime;
            const float deltaTime   = std::min(rawDelta, 0.05f);
            lastTime = currentTime;

            uiController.updateToggles();

            const voxel::ClientInputFrame inputFrame = inputCollector.poll(window.handle());
            game.update(inputFrame, deltaTime);

            runtimeBridge.tick(deltaTime);

            // Sync world time from network
            if (auto time = network.takePendingWorldTime()) {
                game.simulation().setTime(*time);
            }

            runtimeBridge.syncChat();
            uiController.syncCrafting(activeNetwork);

            ui.setDebugData(game.getDebugData());
            ui.setInventory(game.getInventory());
            ui.update();

            window.framebufferSize(fbWidth, fbHeight);
            game.render(fbWidth, fbHeight);
            game.renderHotbarIcons(fbWidth, fbHeight);
            ui.render();

            window.swapBuffers();
            window.pollEvents();
        }
    } catch (const std::exception& error) {
        std::cerr << "Fatal error: " << error.what() << '\n';
        return 1;
    }

    return 0;
}
