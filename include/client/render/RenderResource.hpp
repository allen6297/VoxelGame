#pragma once

#include <cstdint>

namespace voxel {
struct RenderBufferHandle {
    std::uint32_t id = 0;
};

struct RenderTextureHandle {
    std::uint32_t id = 0;
};

struct RenderViewport {
    int x = 0;
    int y = 0;
    int width = 0;
    int height = 0;
};
}  // namespace voxel
