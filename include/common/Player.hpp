#pragma once

#include <array>

#include "Input.hpp"
#include "Math.hpp"
#include "player/Inventory.hpp"
#include "data/GameData.hpp"
#include "world/World.hpp"

namespace voxel {
constexpr float kPlayerRadius = 0.32f;
constexpr float kPlayerHeight = 1.8f;
constexpr float kEyeHeight = 1.62f;

struct Player {
    std::string name = "Player";
    Vec3 position {0.5f, 200.0f, 0.5f};
    Vec3 velocity {0.0f, 0.0f, 0.0f};
    float yaw = 0.0f;
    float pitch = -20.0f;
    bool grounded = false;
    Inventory inventory {};
};

struct InputState {
    bool breakPressed = false;
    bool placePressed = false;
    bool jumpPressed = false;
    bool placeHeld = false;
    bool breakHeld = false;
    bool jumpHeld = false;
    std::array<bool, kInventorySlots> slotHeld {};
};

bool playerCollidesAt(const World& world, const GameData& gameData, const Vec3& position);
Vec3 getLookDirection(const Player& player);
Vec3 getEyePosition(const Player& player);
void updateMouseLook(const ClientInputFrame& input, Player& player);
void updateInput(const ClientInputFrame& frame, InputState& input);
void jump(Player& player, const InputState& input);
void updateMovement(const ClientInputFrame& input, const World& world, const GameData& gameData, Player& player, float deltaTime);
}  // namespace voxel
