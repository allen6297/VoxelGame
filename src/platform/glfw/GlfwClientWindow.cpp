#include "platform/glfw/GlfwClientWindow.hpp"

#include <stdexcept>

#include <GLFW/glfw3.h>

#include "ui/GameUI.hpp"

namespace voxel {
namespace {
bool keyDown(GLFWwindow* window, int key) {
    return glfwGetKey(window, key) == GLFW_PRESS;
}
}  // namespace

GlfwClientWindow::GlfwClientWindow(const GlfwClientWindowConfig& config) {
    if (!glfwInit()) {
        throw std::runtime_error("Failed to initialize GLFW.");
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
    glfwWindowHint(GLFW_DECORATED, GLFW_FALSE);

    GLFWmonitor* primaryMonitor = glfwGetPrimaryMonitor();
    const GLFWvidmode* mode = glfwGetVideoMode(primaryMonitor);
    glfwWindowHint(GLFW_RED_BITS, mode->redBits);
    glfwWindowHint(GLFW_GREEN_BITS, mode->greenBits);
    glfwWindowHint(GLFW_BLUE_BITS, mode->blueBits);

    if (config.swapInterval > 0) {
        glfwWindowHint(GLFW_REFRESH_RATE, mode->refreshRate);
    }

    GLFWmonitor* monitor = config.fullscreen ? primaryMonitor : nullptr;
    window_ = glfwCreateWindow(mode->width, mode->height, config.title.c_str(), monitor, nullptr);
    if (window_ == nullptr) {
        glfwTerminate();
        throw std::runtime_error("Failed to create the window. Make sure GLFW and an OpenGL driver are installed.");
    }

    if (monitor == nullptr) {
        int monitorX = 0;
        int monitorY = 0;
        glfwGetMonitorPos(primaryMonitor, &monitorX, &monitorY);
        glfwSetWindowPos(window_, monitorX, monitorY);
        glfwSetWindowAttrib(window_, GLFW_DECORATED, GLFW_TRUE);
    }

    glfwMakeContextCurrent(window_);
    glfwSwapInterval(config.swapInterval);
    glfwSetInputMode(window_, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    if (glfwRawMouseMotionSupported()) {
        glfwSetInputMode(window_, GLFW_RAW_MOUSE_MOTION, GLFW_TRUE);
    }

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
}

GlfwClientWindow::~GlfwClientWindow() {
    if (window_ != nullptr) {
        glfwDestroyWindow(window_);
    }
    glfwTerminate();
}

bool GlfwClientWindow::shouldClose() const {
    return glfwWindowShouldClose(window_);
}

float GlfwClientWindow::time() const {
    return static_cast<float>(glfwGetTime());
}

void GlfwClientWindow::framebufferSize(int& width, int& height) const {
    glfwGetFramebufferSize(window_, &width, &height);
}

void GlfwClientWindow::swapBuffers() {
    glfwSwapBuffers(window_);
}

void GlfwClientWindow::pollEvents() {
    glfwPollEvents();
}

void GlfwClientWindow::installUiCallbacks(GameUI& ui) {
    glfwSetWindowUserPointer(window_, &ui);

    glfwSetFramebufferSizeCallback(window_, [](GLFWwindow* w, int width, int height) {
        static_cast<GameUI*>(glfwGetWindowUserPointer(w))->onFramebufferSize(width, height);
    });
    glfwSetWindowContentScaleCallback(window_, [](GLFWwindow* w, float xscale, float yscale) {
        static_cast<GameUI*>(glfwGetWindowUserPointer(w))->onContentScale(xscale, yscale);
    });
    glfwSetKeyCallback(window_, [](GLFWwindow* w, int key, int scancode, int action, int mods) {
        static_cast<GameUI*>(glfwGetWindowUserPointer(w))->onKey(key, scancode, action, mods);
    });
    glfwSetCharCallback(window_, [](GLFWwindow* w, unsigned int codepoint) {
        static_cast<GameUI*>(glfwGetWindowUserPointer(w))->onChar(codepoint);
    });
    glfwSetCursorPosCallback(window_, [](GLFWwindow* w, double x, double y) {
        static_cast<GameUI*>(glfwGetWindowUserPointer(w))->onCursorPos(x, y);
    });
    glfwSetMouseButtonCallback(window_, [](GLFWwindow* w, int button, int action, int mods) {
        static_cast<GameUI*>(glfwGetWindowUserPointer(w))->onMouseButton(button, action, mods);
    });
    glfwSetScrollCallback(window_, [](GLFWwindow* w, double xoffset, double yoffset) {
        static_cast<GameUI*>(glfwGetWindowUserPointer(w))->onScroll(xoffset, yoffset);
    });
}

bool GlfwClientWindow::escapeDown() const {
    return keyDown(window_, GLFW_KEY_ESCAPE);
}

bool GlfwClientWindow::enterDown() const {
    return keyDown(window_, GLFW_KEY_ENTER);
}

bool GlfwClientWindow::chatToggleDown() const {
    return keyDown(window_, GLFW_KEY_T);
}

bool GlfwClientWindow::craftingToggleDown() const {
    return keyDown(window_, GLFW_KEY_G);
}

bool GlfwClientWindow::cursorCaptured() const {
    return glfwGetInputMode(window_, GLFW_CURSOR) == GLFW_CURSOR_DISABLED;
}

void GlfwClientWindow::setCursorCaptured(bool captured) {
    glfwSetInputMode(window_, GLFW_CURSOR, captured ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL);
    if (captured && glfwRawMouseMotionSupported()) {
        glfwSetInputMode(window_, GLFW_RAW_MOUSE_MOTION, GLFW_TRUE);
    }
}

void GlfwClientWindow::toggleCursorCaptured() {
    setCursorCaptured(!cursorCaptured());
}

}  // namespace voxel
