#include "Player.hpp"

#include <algorithm>
#include <cmath>

namespace voxel {

namespace {
constexpr float kGravity = 24.0f;
constexpr float kJumpVelocity = 8.5f;
constexpr float kMoveSpeed = 5.5f;
constexpr float kMouseSensitivity = 0.09f;
}  // namespace

namespace {
void movePlayerAxis(const World& world, const GameData& gameData, Player& player, const float dx, const float dy, const float dz) {
    if (dx != 0.0f) {
        Vec3 next = player.position;
        next.x += dx;
        if (!playerCollidesAt(world, gameData, next)) {
            player.position.x = next.x;
        } else {
            player.velocity.x = 0.0f;
        }
    }

    if (dz != 0.0f) {
        Vec3 next = player.position;
        next.z += dz;
        if (!playerCollidesAt(world, gameData, next)) {
            player.position.z = next.z;
        } else {
            player.velocity.z = 0.0f;
        }
    }

    if (dy != 0.0f) {
        Vec3 next = player.position;
        next.y += dy;
        if (!playerCollidesAt(world, gameData, next)) {
            player.position.y = next.y;
            player.grounded = false;
        } else {
            if (dy < 0.0f) {
                player.grounded = true;
            }
            player.velocity.y = 0.0f;
        }
    }
}
}  // namespace

void updateMouseLook(const ClientInputFrame& input, Player& player) {
    if (!input.cursorCaptured) {
        return;
    }

    player.yaw += input.mouseDeltaX * kMouseSensitivity;
    player.pitch = std::clamp(player.pitch - input.mouseDeltaY * kMouseSensitivity, -89.0f, 89.0f);
}

void updateInput(const ClientInputFrame& frame, InputState& input) {
    if (!frame.cursorCaptured) {
        input.breakPressed = false;
        input.placePressed = false;
        input.jumpPressed  = false;
        input.breakHeld    = false;
        input.placeHeld    = false;
        input.jumpHeld     = false;
        return;
    }

    input.placePressed = frame.placeDown && !input.placeHeld;
    input.breakPressed = frame.breakDown && !input.breakHeld;
    input.jumpPressed  = frame.jumpDown && !input.jumpHeld;

    input.placeHeld = frame.placeDown;
    input.breakHeld = frame.breakDown;
    input.jumpHeld  = frame.jumpDown;
}

void jump(Player& player, const InputState& input) {
    if (input.jumpPressed && player.grounded) {
        player.velocity.y = kJumpVelocity;
        player.grounded = false;
    }
}

void updateMovement(const ClientInputFrame& input, const World& world, const GameData& gameData, Player& player, const float deltaTime) {
    Vec3 moveIntent{0.0f, 0.0f, 0.0f};

    if (input.cursorCaptured) {
        const float yawRad  = toRadians(player.yaw);
        const Vec3  forward{std::sin(yawRad), 0.0f, -std::cos(yawRad)};
        const Vec3  right{std::cos(yawRad), 0.0f, std::sin(yawRad)};

        if (input.moveForward) {
            moveIntent = moveIntent + forward;
        }
        if (input.moveBack) {
            moveIntent = moveIntent - forward;
        }
        if (input.moveLeft) {
            moveIntent = moveIntent - right;
        }
        if (input.moveRight) {
            moveIntent = moveIntent + right;
        }
    }

    moveIntent        = normalize(moveIntent);
    player.velocity.x = moveIntent.x * kMoveSpeed;
    player.velocity.z = moveIntent.z * kMoveSpeed;

    player.velocity.y -= kGravity * deltaTime;

    movePlayerAxis(world, gameData, player, player.velocity.x * deltaTime, 0.0f, 0.0f);
    movePlayerAxis(world, gameData, player, 0.0f, 0.0f, player.velocity.z * deltaTime);
    movePlayerAxis(world, gameData, player, 0.0f, player.velocity.y * deltaTime, 0.0f);

    if (player.position.y < -10.0f) {
        player.position = {0.5f, 80.0f, 0.5f};
        player.velocity = {0.0f, 0.0f, 0.0f};
        player.yaw = 0.0f;
        player.pitch = -20.0f;
    }
}

}  // namespace voxel
