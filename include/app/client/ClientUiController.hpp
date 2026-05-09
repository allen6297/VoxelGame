#pragma once

namespace voxel {

class GameUI;
class GlfwClientWindow;
class NetworkManager;

class ClientUiController {
public:
    ClientUiController(GlfwClientWindow& window, GameUI& ui);

    void updateToggles();
    void syncCrafting(NetworkManager* activeNetwork);

private:
    GlfwClientWindow& window_;
    GameUI& ui_;
    bool escWasDown_ = false;
    bool enterWasDown_ = false;
    bool chatToggleWasDown_ = false;
    bool craftingToggleWasDown_ = false;

    void syncCursorForUi();
};

}  // namespace voxel
