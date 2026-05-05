#pragma once

#include "data/BiomeDefinition.hpp"

#include <unordered_map>
#include <string>

namespace voxel {

// Returns the best-matching biome for the given climate point.
// Returns nullptr only if the biomes map is empty.
const BiomeDefinition* selectBiome(
    const std::unordered_map<std::string, BiomeDefinition>& biomes,
    const ClimatePoint& climate
);

}  // namespace voxel
