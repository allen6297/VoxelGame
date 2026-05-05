#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
#include <thread>

#include "server/HeadlessServer.hpp"
#include "server/ServerBootstrap.hpp"

namespace {

constexpr std::uint16_t kDefaultPort = 27015;
std::atomic_bool gRunning {true};

void handleSignal(int) {
    gRunning = false;
}

std::uint16_t parsePort(const char* value) {
    const int port = std::stoi(value);
    if (port <= 0 || port > 65535) {
        throw std::runtime_error("Port must be between 1 and 65535.");
    }
    return static_cast<std::uint16_t>(port);
}

void printUsage(const char* exeName) {
    std::cout << "Usage: " << exeName << " [--port 27015]\n";
}

}  // namespace

int main(int argc, char** argv) {
    std::uint16_t port = kDefaultPort;

    try {
        for (int i = 1; i < argc; ++i) {
            const std::string arg = argv[i];
            if (arg == "--help" || arg == "-h") {
                printUsage(argv[0]);
                return 0;
            }
            if (arg == "--port") {
                if (i + 1 >= argc) {
                    throw std::runtime_error("--port requires a value.");
                }
                port = parsePort(argv[++i]);
                continue;
            }
            throw std::runtime_error("Unknown argument: " + arg);
        }

        std::signal(SIGINT, handleSignal);
        std::signal(SIGTERM, handleSignal);

        const std::filesystem::path projectRoot = voxel::findProjectRoot();
        voxel::ServerContext context = voxel::loadServerContext(projectRoot);
        voxel::HeadlessServer server(std::move(context.gameData));
        if (!server.start(port, projectRoot)) {
            return 1;
        }

        std::cout << "VoxelServer running. Press Ctrl+C to stop.\n";
        while (gRunning) {
            server.tick();
            std::this_thread::sleep_for(std::chrono::milliseconds(8));
        }

        std::cout << "VoxelServer stopping.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Fatal server error: " << error.what() << '\n';
        printUsage(argv[0]);
        return 1;
    }
}
