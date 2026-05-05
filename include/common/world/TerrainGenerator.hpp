#pragma once

#include "data/GameData.hpp"
#include "world/BiomeDefinition.hpp"
#include "world/Chunk.hpp"

namespace voxel {
struct TerrainGenerator {
    int seed = 12345;

    Chunk generateChunk(const ChunkCoord& coord, const GameData& gameData) const;

    // Sample the raw 6-axis climate at a world XZ position.
    ClimatePoint sampleClimateAt(float wx, float wz) const;

    // Select the best-fitting biome at a world XZ position.
    const BiomeDefinition* selectBiomeAt(
        float wx, float wz,
        const std::unordered_map<std::string, BiomeDefinition>& biomes) const;
};
}  // namespace voxel