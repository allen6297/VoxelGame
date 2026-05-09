#include "app/client/ClientUiController.hpp"

#include <string>

#include "client/ui/GameUI.hpp"
#include "common/network/NetworkManager.hpp"
#include "platform/glfw/GlfwClientWindow.hpp"

namespace voxel {

ClientUiController::ClientUiController(GlfwClientWindow& window, GameUI& ui)
    : window_(window), ui_(ui) {}

void ClientUiController::updateToggles() {
    const bool escDown = window_.escapeDown();
    if (escDown && !escWasDown_) {
        if (ui_.isChatOpen()) {
            ui_.setChatOpen(false);
            syncCursorForUi();
        } else {
            window_.toggleCursorCaptured();
        }
    }
    escWasDown_ = escDown;

    const bool enterDown = window_.enterDown();
    const bool chatToggleDown = window_.chatToggleDown();
    if (!ui_.isChatOpen() &&
        ((enterDown && !enterWasDown_) || (chatToggleDown && !chatToggleWasDown_))) {
        ui_.setChatOpen(true);
        syncCursorForUi();
    }
    enterWasDown_ = enterDown;
    chatToggleWasDown_ = chatToggleDown;

    const bool craftingToggleDown = window_.craftingToggleDown();
    if (craftingToggleDown && !craftingToggleWasDown_) {
        ui_.setCraftingOpen(!ui_.isCraftingOpen());
        syncCursorForUi();
    }
    craftingToggleWasDown_ = craftingToggleDown;
}

void ClientUiController::syncCrafting(NetworkManager* activeNetwork) {
    std::string craftReq = ui_.consumePendingCraftRequest();
    if (!craftReq.empty() && activeNetwork != nullptr) {
        activeNetwork->publishCraftRequest(craftReq);
    }
}

void ClientUiController::syncCursorForUi() {
    window_.setCursorCaptured(!ui_.isChatOpen() && !ui_.isCraftingOpen());
}

}  // namespace voxel
