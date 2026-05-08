#pragma once

#include "Input.hpp"

struct GLFWwindow;

namespace voxel {

class GlfwInputCollector {
public:
    ClientInputFrame poll(GLFWwindow* window);

private:
    bool firstMouse_ = true;
    double lastMouseX_ = 0.0;
    double lastMouseY_ = 0.0;
};

}  // namespace voxel
