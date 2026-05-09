#include "app/client/ClientRuntimeBridge.hpp"

#include <cstdint>
#include <utility>

#include "client/game/Game.hpp"
#include "client/ui/GameUI.hpp"
#include "common/network/NetworkManager.hpp"
#include "common/pack/PackManager.hpp"
#include "common/pack/ScriptManager.hpp"

namespace voxel {

ClientRuntimeBridge::ClientRuntimeBridge(
    ScriptManager& scriptManager,
    NetworkManager& network,
    NetworkManager* activeNetwork,
    Game& game,
    GameUI& ui,
    std::string playerName)
    : scriptManager_(scriptManager),
      network_(network),
      activeNetwork_(activeNetwork),
      game_(game),
      ui_(ui),
      playerName_(std::move(playerName)),
      runLocalRuntimeScripts_(
          activeNetwork_ == nullptr || activeNetwork_->mode() == NetworkManager::Mode::Server) {}

void ClientRuntimeBridge::loadRuntimeScripts(const PackManager& packManager) {
    if (!runLocalRuntimeScripts_) {
        return;
    }

    scriptManager_.setHostKind(ScriptHost::Server);
    scriptManager_.setWorldSimulation(&game_.simulation());
    scriptManager_.setCurrentPlayer(&game_.player());
    scriptManager_.setGameData(&game_.gameData());
    scriptManager_.loadRuntimeScripts(packManager);
}

void ClientRuntimeBridge::tick(float deltaTime) {
    if (runLocalRuntimeScripts_) {
        scriptManager_.tick(deltaTime);
    }
}

void ClientRuntimeBridge::syncChat() {
    std::string chatInput = ui_.consumePendingChatInput();
    if (!chatInput.empty()) {
        if (activeNetwork_) {
            activeNetwork_->publishChatMessage(chatInput);
            if (activeNetwork_->mode() == NetworkManager::Mode::Client &&
                (chatInput.empty() || chatInput[0] != '/')) {
                ui_.addChatMessage(playerName_, chatInput);
            }
        } else if (chatInput[0] != '/') {
            ui_.addChatMessage(playerName_, chatInput);
        } else if (runLocalRuntimeScripts_) {
            executeCommand(network_.localPlayerId(), chatInput);
        }
    }

    for (auto& chat : network_.takePendingChatMessages()) {
        if (!chat.message.empty() && chat.message[0] == '/' && runLocalRuntimeScripts_) {
            for (const auto& reply : scriptManager_.executeCommand(chat.playerId, chat.message)) {
                if (activeNetwork_ && activeNetwork_->mode() == NetworkManager::Mode::Server) {
                    activeNetwork_->broadcastChatMessage(0, reply);
                }
                ui_.addChatMessage("Server", reply);
            }
            continue;
        }

        std::string sender = "Server";
        if (chat.playerId == network_.localPlayerId()) {
            sender = playerName_;
        } else {
            auto it = network_.remotePlayers().find(chat.playerId);
            if (it != network_.remotePlayers().end()) {
                sender = it->second.name;
            } else {
                sender = "Player " + std::to_string(chat.playerId);
            }
        }
        ui_.addChatMessage(sender, chat.message);
    }
}

void ClientRuntimeBridge::executeCommand(std::uint32_t playerId, const std::string& input) {
    for (const auto& reply : scriptManager_.executeCommand(playerId, input)) {
        ui_.addChatMessage("Server", reply);
    }
}

}  // namespace voxel
