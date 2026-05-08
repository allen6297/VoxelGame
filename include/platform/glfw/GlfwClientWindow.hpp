#pragma once

#include <string>

struct GLFWwindow;

namespace voxel {

class GameUI;

struct GlfwClientWindowConfig {
    bool fullscreen = false;
    int swapInterval = 1;
    std::string title = "Voxel Game";
};

class GlfwClientWindow {
public:
    explicit GlfwClientWindow(const GlfwClientWindowConfig& config);
    ~GlfwClientWindow();

    GlfwClientWindow(const GlfwClientWindow&) = delete;
    GlfwClientWindow& operator=(const GlfwClientWindow&) = delete;

    GLFWwindow* handle() const { return window_; }

    bool shouldClose() const;
    float time() const;
    void framebufferSize(int& width, int& height) const;
    void swapBuffers();
    void pollEvents();

    void installUiCallbacks(GameUI& ui);

    bool escapeDown() const;
    bool enterDown() const;
    bool chatToggleDown() const;
    bool craftingToggleDown() const;

    bool cursorCaptured() const;
    void setCursorCaptured(bool captured);
    void toggleCursorCaptured();

private:
    GLFWwindow* window_ = nullptr;
};

}  // namespace voxel
