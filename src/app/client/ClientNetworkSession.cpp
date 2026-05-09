#include "app/client/ClientNetworkSession.hpp"

namespace voxel {

NetworkManager* startClientNetworkSession(NetworkManager& network, const ClientNetworkOptions& options) {
    switch (options.mode) {
        case ClientNetworkMode::Host:
            return network.startServer(options.port) ? &network : nullptr;
        case ClientNetworkMode::Connect:
            return network.connectToServer(options.hostName, options.port) ? &network : nullptr;
        case ClientNetworkMode::Offline:
            return nullptr;
    }

    return nullptr;
}

}  // namespace voxel
