#include <GLFW/glfw3.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <string>
#include <thread>

#include "common/data/GameData.hpp"
#include "client/game/Game.hpp"
#include "common/network/NetworkManager.hpp"
#include "client/pack/AssetPackManager.hpp"
#include "common/pack/PackManager.hpp"
#include "common/pack/ScriptManager.hpp"
#include "server/HeadlessServer.hpp"
#include "server/ServerBootstrap.hpp"
#include "client/ui/GameUI.hpp"

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

    std::cout << "VoxelGame dedicated server running on port " << port << ". Press Ctrl+C to stop.\n";
    while (gServerRunning) {
        server.tick();
        std::this_thread::sleep_for(std::chrono::milliseconds(8));
    }
    return 0;
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

    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW.\n";
        return 1;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
    glfwWindowHint(GLFW_DECORATED, GLFW_FALSE);

    GLFWmonitor*       monitor = nullptr;
    for (int i = 1; i < argc; ++i) {
        if (std::string(argv[i]) == "--fullscreen") {
            monitor = glfwGetPrimaryMonitor();
        }
    }

    GLFWmonitor*       primaryMonitor = glfwGetPrimaryMonitor();
    const GLFWvidmode* mode           = glfwGetVideoMode(primaryMonitor);
    glfwWindowHint(GLFW_RED_BITS,     mode->redBits);
    glfwWindowHint(GLFW_GREEN_BITS,   mode->greenBits);
    glfwWindowHint(GLFW_BLUE_BITS,    mode->blueBits);
    int swapInterval = 1;
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--limit-fps") {
            if (i + 1 < argc) {
                swapInterval = std::stoi(argv[++i]);
            }
        }
    }

    if (swapInterval > 0) {
        glfwWindowHint(GLFW_REFRESH_RATE, mode->refreshRate);
    }

    GLFWwindow* window = glfwCreateWindow(
        mode->width, mode->height, "Voxel Game", monitor, nullptr);
    if (window == nullptr) {
        std::cerr << "Failed to create the window. Make sure GLFW and an OpenGL driver are installed.\n";
        glfwTerminate();
        return 1;
    }

    int monitorX = 0, monitorY = 0;
    if (monitor == nullptr) {
        glfwGetMonitorPos(primaryMonitor, &monitorX, &monitorY);
        glfwSetWindowPos(window, monitorX, monitorY);
        glfwSetWindowAttrib(window, GLFW_DECORATED, GLFW_TRUE);
    }


    glfwMakeContextCurrent(window);
    glfwSwapInterval(swapInterval);
    std::cout << "[Main] glfwSwapInterval set to " << swapInterval << std::endl;
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    if (glfwRawMouseMotionSupported()) {
        glfwSetInputMode(window, GLFW_RAW_MOUSE_MOTION, GLFW_TRUE);
    }

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);

    std::string playerName = "Player";
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--name" && i + 1 < argc) {
            playerName = argv[++i];
        }
    }

    try {
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
        glfwGetFramebufferSize(window, &fbWidth, &fbHeight);

        voxel::NetworkManager network;
        voxel::NetworkManager* activeNetwork = nullptr;
        for (int i = 1; i < argc; ++i) {
            const std::string arg = argv[i];
            if (arg == "--host") {
                const std::uint16_t port =
                    (i + 1 < argc && argv[i + 1][0] != '-') ? parsePort(argv[++i]) : kDefaultMultiplayerPort;
                if (network.startServer(port)) {
                    activeNetwork = &network;
                }
            } else if (arg == "--connect") {
                if (i + 1 >= argc) {
                    throw std::runtime_error("--connect requires a host name or IP address.");
                }
                const std::string hostName = argv[++i];
                const std::uint16_t port =
                    (i + 1 < argc && argv[i + 1][0] != '-') ? parsePort(argv[++i]) : kDefaultMultiplayerPort;
                if (network.connectToServer(hostName, port)) {
                    activeNetwork = &network;
                }
            } else if (arg == "--limit-fps") {
                if (i + 1 < argc) {
                    i++; // Already handled above
                }
            } else {
                // Unknown argument or --server (which is handled by runDedicatedServer)
            }
        }

        voxel::Game   game(std::move(gameData), assetsRoot, playerName, activeNetwork);
        voxel::GameUI ui(window, fbWidth, fbHeight, assetsRoot);

        const bool runLocalRuntimeScripts =
            activeNetwork == nullptr || activeNetwork->mode() == voxel::NetworkManager::Mode::Server;
        if (runLocalRuntimeScripts) {
            scriptManager.setHostKind(voxel::ScriptHost::Server);
            scriptManager.setWorldSimulation(&game.simulation());
            scriptManager.setCurrentPlayer(&game.player());
            scriptManager.setGameData(&game.gameData());
            scriptManager.loadRuntimeScripts(packManager);
        }

        // Forward GLFW events to RmlUI — use window user pointer so lambdas
        // need no captures (required for GLFW's C-style callback signature).
        glfwSetWindowUserPointer(window, &ui);

        glfwSetFramebufferSizeCallback(window, [](GLFWwindow* w, int width, int height) {
            static_cast<voxel::GameUI*>(glfwGetWindowUserPointer(w))
                ->onFramebufferSize(width, height);
        });
        glfwSetWindowContentScaleCallback(window, [](GLFWwindow* w, float xscale, float yscale) {
            static_cast<voxel::GameUI*>(glfwGetWindowUserPointer(w))
                ->onContentScale(xscale, yscale);
        });
        glfwSetKeyCallback(window, [](GLFWwindow* w, int key, int sc, int action, int mods) {
            static_cast<voxel::GameUI*>(glfwGetWindowUserPointer(w))
                ->onKey(key, sc, action, mods);
        });
        glfwSetCharCallback(window, [](GLFWwindow* w, unsigned int cp) {
            static_cast<voxel::GameUI*>(glfwGetWindowUserPointer(w))->onChar(cp);
        });
        glfwSetCursorPosCallback(window, [](GLFWwindow* w, double x, double y) {
            static_cast<voxel::GameUI*>(glfwGetWindowUserPointer(w))->onCursorPos(x, y);
        });
        glfwSetMouseButtonCallback(window, [](GLFWwindow* w, int btn, int action, int mods) {
            static_cast<voxel::GameUI*>(glfwGetWindowUserPointer(w))
                ->onMouseButton(btn, action, mods);
        });
        glfwSetScrollCallback(window, [](GLFWwindow* w, double xoff, double yoff) {
            static_cast<voxel::GameUI*>(glfwGetWindowUserPointer(w))->onScroll(xoff, yoff);
        });

        auto  lastTime   = static_cast<float>(glfwGetTime());
        bool  escWasDown = false;
        bool  enterWasDown = false;
        bool  tWasDown = false;
        bool  gWasDown = false;

        ui.setGameData(&game.gameData());
        {
            std::vector<voxel::RecipeDefinition> recipes;
            for (auto const& [id, recipe] : game.gameData().recipes) recipes.push_back(recipe);
            ui.setRecipes(recipes);
        }

        while (!glfwWindowShouldClose(window)) {
            const auto  currentTime = static_cast<float>(glfwGetTime());
            const float rawDelta    = currentTime - lastTime;
            const float deltaTime   = std::min(rawDelta, 0.05f);
            lastTime = currentTime;

            auto syncCursorForUI = [&]() {
                if (ui.isChatOpen() || ui.isCraftingOpen()) {
                    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
                } else {
                    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
                }
            };

            // ESC closes chat if open; otherwise it edge-triggers cursor capture toggle.
            const bool escDown = glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS;
            if (escDown && !escWasDown) {
                if (ui.isChatOpen()) {
                    ui.setChatOpen(false);
                    syncCursorForUI();
                } else {
                    const bool captured =
                        glfwGetInputMode(window, GLFW_CURSOR) == GLFW_CURSOR_DISABLED;
                    glfwSetInputMode(window, GLFW_CURSOR,
                        captured ? GLFW_CURSOR_NORMAL : GLFW_CURSOR_DISABLED);
                    if (!captured && glfwRawMouseMotionSupported())
                        glfwSetInputMode(window, GLFW_RAW_MOUSE_MOTION, GLFW_TRUE);
                }
            }
            escWasDown = escDown;

            // ENTER opens chat; T opens chat.
            const bool enterDown = glfwGetKey(window, GLFW_KEY_ENTER) == GLFW_PRESS;
            const bool tDown = glfwGetKey(window, GLFW_KEY_T) == GLFW_PRESS;
            if (!ui.isChatOpen() && ((enterDown && !enterWasDown) || (tDown && !tWasDown))) {
                ui.setChatOpen(true);
                syncCursorForUI();
            }
            enterWasDown = enterDown;
            tWasDown = tDown;

            // G opens/closes crafting
            const bool gDown = glfwGetKey(window, GLFW_KEY_G) == GLFW_PRESS;
            if (gDown && !gWasDown) {
                ui.setCraftingOpen(!ui.isCraftingOpen());
                syncCursorForUI();
            }
            gWasDown = gDown;

            game.update(window, deltaTime);

            if (runLocalRuntimeScripts) {
                scriptManager.tick(deltaTime);
            }

            // Sync world time from network
            if (auto time = network.takePendingWorldTime()) {
                game.simulation().setTime(*time);
            }

            // Sync chat between UI and Network
            std::string chatInput = ui.consumePendingChatInput();
            if (!chatInput.empty()) {
                if (activeNetwork) {
                    activeNetwork->publishChatMessage(chatInput);
                    if (activeNetwork->mode() == voxel::NetworkManager::Mode::Client &&
                        (chatInput.empty() || chatInput[0] != '/')) {
                        ui.addChatMessage(playerName, chatInput);
                    }
                } else if (chatInput[0] != '/') {
                    ui.addChatMessage(playerName, chatInput);
                } else if (runLocalRuntimeScripts) {
                    for (const auto& reply : scriptManager.executeCommand(network.localPlayerId(), chatInput)) {
                        ui.addChatMessage("Server", reply);
                    }
                }
            }

            for (auto& chat : network.takePendingChatMessages()) {
                if (!chat.message.empty() && chat.message[0] == '/' && runLocalRuntimeScripts) {
                    for (const auto& reply : scriptManager.executeCommand(chat.playerId, chat.message)) {
                        if (activeNetwork && activeNetwork->mode() == voxel::NetworkManager::Mode::Server) {
                            activeNetwork->broadcastChatMessage(0, reply);
                        }
                        ui.addChatMessage("Server", reply);
                    }
                    continue;
                }

                std::string sender = "Server";
                if (chat.playerId == network.localPlayerId()) {
                    sender = playerName;
                } else {
                    auto it = network.remotePlayers().find(chat.playerId);
                    if (it != network.remotePlayers().end()) {
                        sender = it->second.name;
                    } else {
                        sender = "Player " + std::to_string(chat.playerId);
                    }
                }
                ui.addChatMessage(sender, chat.message);
            }

            // Sync crafting between UI and Network
            std::string craftReq = ui.consumePendingCraftRequest();
            if (!craftReq.empty() && activeNetwork) {
                activeNetwork->publishCraftRequest(craftReq);
            }

            ui.setDebugData(game.getDebugData());
            ui.setInventory(game.getInventory());
            ui.update();

            glfwGetFramebufferSize(window, &fbWidth, &fbHeight);
            game.render(fbWidth, fbHeight);
            game.renderHotbarIcons(fbWidth, fbHeight);
            ui.render();

            glfwSwapBuffers(window);
            glfwPollEvents();
        }
    } catch (const std::exception& error) {
        std::cerr << "Fatal error: " << error.what() << '\n';
        glfwDestroyWindow(window);
        glfwTerminate();
        return 1;
    }

    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
