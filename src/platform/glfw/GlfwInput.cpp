#include "platform/glfw/GlfwInput.hpp"

#include <GLFW/glfw3.h>

#include "player/Inventory.hpp"

namespace voxel {

ClientInputFrame GlfwInputCollector::poll(GLFWwindow* window) {
    ClientInputFrame frame;
    if (window == nullptr) {
        firstMouse_ = true;
        return frame;
    }

    frame.cursorCaptured = glfwGetInputMode(window, GLFW_CURSOR) == GLFW_CURSOR_DISABLED;
    if (frame.cursorCaptured) {
        double cursorX = 0.0;
        double cursorY = 0.0;
        glfwGetCursorPos(window, &cursorX, &cursorY);
        if (firstMouse_) {
            lastMouseX_ = cursorX;
            lastMouseY_ = cursorY;
            firstMouse_ = false;
        }

        frame.mouseDeltaX = static_cast<float>(cursorX - lastMouseX_);
        frame.mouseDeltaY = static_cast<float>(cursorY - lastMouseY_);
        lastMouseX_ = cursorX;
        lastMouseY_ = cursorY;

        frame.moveForward = glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS;
        frame.moveBack = glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS;
        frame.moveLeft = glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS;
        frame.moveRight = glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS;
        frame.breakDown = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;
        frame.placeDown = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS;
        frame.jumpDown = glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS;
        for (int i = 0; i < kInventorySlots; ++i) {
            frame.slotDown[static_cast<std::size_t>(i)] =
                glfwGetKey(window, GLFW_KEY_1 + i) == GLFW_PRESS;
        }
    } else {
        firstMouse_ = true;
    }

    frame.reloadContentDown = glfwGetKey(window, GLFW_KEY_F5) == GLFW_PRESS;
    return frame;
}

}  // namespace voxel
