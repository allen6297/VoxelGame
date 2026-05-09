#pragma once

#include <string>

namespace voxel {

class Game;
class GameUI;
class NetworkManager;
class PackManager;
class ScriptManager;

class ClientRuntimeBridge {
public:
    ClientRuntimeBridge(
        ScriptManager& scriptManager,
        NetworkManager& network,
        NetworkManager* activeNetwork,
        Game& game,
        GameUI& ui,
        std::string playerName);

    bool localRuntimeEnabled() const { return runLocalRuntimeScripts_; }

    void loadRuntimeScripts(const PackManager& packManager);
    void tick(float deltaTime);
    void syncChat();

private:
    ScriptManager& scriptManager_;
    NetworkManager& network_;
    NetworkManager* activeNetwork_ = nullptr;
    Game& game_;
    GameUI& ui_;
    std::string playerName_;
    bool runLocalRuntimeScripts_ = false;

    void executeCommand(std::uint32_t playerId, const std::string& input);
};

}  // namespace voxel
