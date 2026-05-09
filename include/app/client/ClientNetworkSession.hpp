#pragma once

#include "app/client/ClientOptions.hpp"
#include "common/network/NetworkManager.hpp"

namespace voxel {

NetworkManager* startClientNetworkSession(NetworkManager& network, const ClientNetworkOptions& options);

}  // namespace voxel
