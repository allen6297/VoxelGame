#pragma once

#include <array>

#include "player/Inventory.hpp"

namespace voxel {

struct ClientInputFrame {
    bool cursorCaptured = false;

    float mouseDeltaX = 0.0f;
    float mouseDeltaY = 0.0f;

    bool moveForward = false;
    bool moveBack = false;
    bool moveLeft = false;
    bool moveRight = false;

    bool breakDown = false;
    bool placeDown = false;
    bool jumpDown = false;

    std::array<bool, kInventorySlots> slotDown {};
    bool reloadContentDown = false;
};

}  // namespace voxel
