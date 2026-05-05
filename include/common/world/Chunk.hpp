#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>

#include "world/Config.hpp"

namespace voxel {
constexpr int kChunkX = ChunkDimensions::x;
constexpr int kChunkY = ChunkDimensions::y;
constexpr int kChunkZ = ChunkDimensions::z;

constexpr int kChunkCountY  = WorldLayout::chunksY;
constexpr int kWorldY       = kChunkY * kChunkCountY;
constexpr int kViewDistance = WorldLayout::viewDistance;

struct ChunkCoord {
    int x = 0;
    int y = 0;
    int z = 0;

    bool operator==(const ChunkCoord&) const = default;
};

struct ChunkCoordHash {
    std::size_t operator()(const ChunkCoord& c) const noexcept {
        std::size_t h = std::hash<int>{}(c.x);
        h ^= std::hash<int>{}(c.y) + 0x9e3779b9u + (h << 6) + (h >> 2);
        h ^= std::hash<int>{}(c.z) + 0x9e3779b9u + (h << 6) + (h >> 2);
        return h;
    }
};

using ChunkBlocks     = std::array<std::array<std::array<std::uint16_t, kChunkZ>, kChunkY>, kChunkX>;
using ChunkTintColors = std::array<std::array<std::array<float, 3>, kChunkZ>, kChunkX>;

struct Chunk {
    ChunkCoord    coord;
    ChunkBlocks   blocks {};
    // Per-column blended biome tint color [lx][lz], filled by TerrainGenerator.
    // Defaults to white so untinted blocks render unmodified.
    ChunkTintColors tintColors;

    Chunk() {
        for (auto& row : tintColors)
            for (auto& cell : row)
                cell = {1.0f, 1.0f, 1.0f};
    }
};
}  // namespace voxel