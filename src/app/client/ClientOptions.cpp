#include "app/client/ClientOptions.hpp"

#include <stdexcept>

namespace voxel {
namespace {
std::uint16_t parsePort(const char* value) {
    const int port = std::stoi(value);
    if (port < 0 || port > 65535) {
        throw std::runtime_error("Port must be between 0 and 65535.");
    }
    return static_cast<std::uint16_t>(port);
}

bool hasValue(int index, int argc, char** argv) {
    return index + 1 < argc && argv[index + 1][0] != '-';
}
}  // namespace

ClientOptions parseClientOptions(int argc, char** argv) {
    ClientOptions options;

    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--name" && i + 1 < argc) {
            options.playerName = argv[++i];
        } else if (arg == "--fullscreen") {
            options.window.fullscreen = true;
        } else if (arg == "--limit-fps" && i + 1 < argc) {
            options.window.swapInterval = std::stoi(argv[++i]);
        } else if (arg == "--diligent") {
            options.diligent = true;
        } else if (arg == "--diligent-proof") {
            options.diligentProof = true;
        } else if (arg == "--diligent-proof-frames" && i + 1 < argc) {
            options.diligentProof = true;
            options.diligentProofFrames = std::stoi(argv[++i]);
        } else if (arg == "--host") {
            options.network.mode = ClientNetworkMode::Host;
            options.network.hostName.clear();
            options.network.port = hasValue(i, argc, argv)
                ? parsePort(argv[++i])
                : kDefaultClientMultiplayerPort;
        } else if (arg == "--connect") {
            if (i + 1 >= argc) {
                throw std::runtime_error("--connect requires a host name or IP address.");
            }
            options.network.mode = ClientNetworkMode::Connect;
            options.network.hostName = argv[++i];
            options.network.port = hasValue(i, argc, argv)
                ? parsePort(argv[++i])
                : kDefaultClientMultiplayerPort;
        }
    }

    return options;
}

}  // namespace voxel
