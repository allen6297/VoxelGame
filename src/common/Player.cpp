#include "Player.hpp"

#include <algorithm>
#include <cmath>
#ifdef VOXEL_CLIENT
#include <GLFW/glfw3.h>
#endif

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

void updateMouseLook(GLFWwindow* window, Player& player) {
#ifdef VOXEL_CLIENT
    static bool   firstMouse = true;
    static double lastX      = 0.0;
    static double lastY      = 0.0;

    // Skip mouse look when cursor is visible (ESC unlocked)
    if (glfwGetInputMode(window, GLFW_CURSOR) != GLFW_CURSOR_DISABLED) {
        firstMouse = true;  // reset so re-capture has no jump
        return;
    }

    double cursorX = 0.0;
    double cursorY = 0.0;
    glfwGetCursorPos(window, &cursorX, &cursorY);

    if (firstMouse) {
        lastX = cursorX;
        lastY = cursorY;
        firstMouse = false;
    }

    const double deltaX = cursorX - lastX;
    const double deltaY = cursorY - lastY;
    lastX = cursorX;
    lastY = cursorY;

    player.yaw += static_cast<float>(deltaX) * kMouseSensitivity;
    player.pitch = std::clamp(player.pitch - static_cast<float>(deltaY) * kMouseSensitivity, -89.0f, 89.0f);
#endif
}

void updateInput(GLFWwindow* window, InputState& input) {
#ifdef VOXEL_CLIENT
    // If the cursor is not captured, suppress all action inputs
    if (glfwGetInputMode(window, GLFW_CURSOR) != GLFW_CURSOR_DISABLED) {
        input.breakPressed = false;
        input.placePressed = false;
        input.jumpPressed  = false;
        input.breakHeld    = false;
        input.placeHeld    = false;
        input.jumpHeld     = false;
        return;
    }

    const bool placeDown = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS;
    const bool breakDown = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;
    const bool jumpDown  = glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS;

    input.placePressed = placeDown && !input.placeHeld;
    input.breakPressed = breakDown && !input.breakHeld;
    input.jumpPressed  = jumpDown && !input.jumpHeld;

    input.placeHeld = placeDown;
    input.breakHeld = breakDown;
    input.jumpHeld  = jumpDown;
#endif
}

void jump(Player& player, const InputState& input) {
    if (input.jumpPressed && player.grounded) {
        player.velocity.y = kJumpVelocity;
        player.grounded = false;
    }
}

void updateMovement(GLFWwindow* window, const World& world, const GameData& gameData, Player& player, const float deltaTime) {
#ifdef VOXEL_CLIENT
    Vec3 moveIntent{0.0f, 0.0f, 0.0f};

    // Only process WASD movement if cursor is captured
    if (glfwGetInputMode(window, GLFW_CURSOR) == GLFW_CURSOR_DISABLED) {
        const float yawRad  = toRadians(player.yaw);
        const Vec3  forward{std::sin(yawRad), 0.0f, -std::cos(yawRad)};
        const Vec3  right{std::cos(yawRad), 0.0f, std::sin(yawRad)};

        if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) {
            moveIntent = moveIntent + forward;
        }
        if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) {
            moveIntent = moveIntent - forward;
        }
        if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) {
            moveIntent = moveIntent - right;
        }
        if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) {
            moveIntent = moveIntent + right;
        }
    }

    moveIntent        = normalize(moveIntent);
    player.velocity.x = moveIntent.x * kMoveSpeed;
    player.velocity.z = moveIntent.z * kMoveSpeed;
#endif

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
