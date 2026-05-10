#pragma once

#include <cstdint>
#include <string>

#include "platform/glfw/GlfwClientWindow.hpp"

namespace voxel {

constexpr std::uint16_t kDefaultClientMultiplayerPort = 27015;

enum class ClientNetworkMode {
    Offline,
    Host,
    Connect,
};

struct ClientNetworkOptions {
    ClientNetworkMode mode = ClientNetworkMode::Offline;
    std::string hostName;
    std::uint16_t port = kDefaultClientMultiplayerPort;
};

struct ClientOptions {
    std::string playerName = "Player";
    GlfwClientWindowConfig window;
    ClientNetworkOptions network;
    bool diligent = false;
    bool diligentProof = false;
    int diligentProofFrames = 0;
};

ClientOptions parseClientOptions(int argc, char** argv);

}  // namespace voxel
