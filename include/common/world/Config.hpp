#pragma once

namespace voxel {
struct ChunkDimensions {
    static constexpr int x = 16;
    static constexpr int y = 16;
    static constexpr int z = 16;
};

struct WorldLayout {
    static constexpr int chunksY      = 8;  // fixed vertical extent (128 blocks)
    static constexpr int viewDistance = 4;  // chunk columns loaded in each XZ direction
};
}  // namespace voxel
