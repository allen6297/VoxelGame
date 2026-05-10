#include "network/NetworkManager.hpp"

#include <chrono>
#include <cstdint>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

namespace {

bool expect(bool condition, const std::string& message) {
    if (!condition) {
        std::cerr << "[FAIL] " << message << '\n';
        return false;
    }
    return true;
}

void pump(voxel::NetworkManager& server, voxel::NetworkManager& client, int iterations = 1) {
    for (int i = 0; i < iterations; ++i) {
        server.poll();
        client.poll();
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

bool waitForClientId(voxel::NetworkManager& server, voxel::NetworkManager& client) {
    for (int i = 0; i < 100; ++i) {
        pump(server, client);
        if (client.localPlayerId() != 0) {
            return true;
        }
    }
    return false;
}

bool waitForServerJoin(voxel::NetworkManager& server, voxel::NetworkManager& client, std::uint32_t expectedId) {
    for (int i = 0; i < 100; ++i) {
        pump(server, client);
        const std::vector<std::uint32_t> joins = server.takePendingPlayerJoins();
        for (const std::uint32_t playerId : joins) {
            if (playerId == expectedId) {
                return true;
            }
        }
    }
    return false;
}

bool waitForServerPlayerState(voxel::NetworkManager& server, voxel::NetworkManager& client, std::uint32_t expectedId) {
    for (int i = 0; i < 100; ++i) {
        pump(server, client);
        const std::vector<voxel::RemotePlayerState> states = server.takePendingPlayerStates();
        for (const voxel::RemotePlayerState& state : states) {
            if (state.id == expectedId && state.name == "ConnectionTest") {
                return true;
            }
        }
    }
    return false;
}

}  // namespace

int main() {
    bool ok = true;

    voxel::NetworkManager server;
    voxel::NetworkManager client;

    std::uint16_t port = 0;
    for (std::uint16_t candidate = 38000; candidate < 38100; ++candidate) {
        if (server.startServer(candidate)) {
            port = candidate;
            break;
        }
    }

    ok &= expect(port != 0, "server should bind to an available test port");
    if (port == 0) {
        return 1;
    }

    ok &= expect(server.mode() == voxel::NetworkManager::Mode::Server, "server should enter server mode");
    ok &= expect(server.localPlayerId() == 1, "server local player id should be host id");
    ok &= expect(client.connectToServer("127.0.0.1", port), "client should start connecting to local server");
    ok &= expect(client.mode() == voxel::NetworkManager::Mode::Client, "client should enter client mode");

    ok &= expect(waitForClientId(server, client), "client should receive an assigned player id");
    const std::uint32_t clientId = client.localPlayerId();
    ok &= expect(clientId != 0 && clientId != server.localPlayerId(), "client id should be assigned separately from host id");
    ok &= expect(waitForServerJoin(server, client, clientId), "server should report a pending player join");

    client.publishLocalPlayer("ConnectionTest", {1.0f, 2.0f, 3.0f}, 4.0f, 5.0f);
    ok &= expect(waitForServerPlayerState(server, client, clientId), "server should receive client player state");

    client.shutdown();
    ok &= expect(!client.isConnected(), "client shutdown should clear connection state");

    server.shutdown();
    ok &= expect(server.mode() == voxel::NetworkManager::Mode::None, "server shutdown should clear mode");
    ok &= expect(server.remotePlayers().empty(), "server shutdown should clear remote player state");

    if (!ok) {
        return 1;
    }

    std::cout << "Network connection tests passed.\n";
    return 0;
}

