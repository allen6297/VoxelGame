#include "world/BiomeDefinition.hpp"

#include <algorithm>
#include <cmath>

namespace voxel {
namespace {

// Returns 1.0 when the sample is inside [min, max].
// Falls off smoothly to 0.0 over 0.2 units outside the range.
float axisFitness(const float sample, const float min, const float max) {
    if (sample >= min && sample <= max) {
        return 1.0f;
    }
    const float dist = sample < min ? min - sample : sample - max;
    return std::max(0.0f, 1.0f - dist / 0.2f);
}

float scoreBiome(const BiomeDefinition& biome, const ClimatePoint& climate) {
    float score = 0.0f;
    score += axisFitness(climate.temperature, biome.climate.temperature.min, biome.climate.temperature.max);
    score += axisFitness(climate.humidity,    biome.climate.humidity.min,    biome.climate.humidity.max);
    score += axisFitness(climate.rainfall,    biome.climate.rainfall.min,    biome.climate.rainfall.max);
    score += axisFitness(climate.elevation,   biome.climate.elevation.min,   biome.climate.elevation.max);
    score += axisFitness(climate.drainage,    biome.climate.drainage.min,    biome.climate.drainage.max);
    score += axisFitness(climate.waterTable,  biome.climate.waterTable.min,  biome.climate.waterTable.max);
    return score * biome.rarity;
}

}  // namespace

const BiomeDefinition* selectBiome(
    const std::unordered_map<std::string, BiomeDefinition>& biomes,
    const ClimatePoint& climate
) {
    const BiomeDefinition* best = nullptr;
    float bestScore = -1.0f;

    for (const auto& [id, biome] : biomes) {
        const float score = scoreBiome(biome, climate);
        const bool better = score > bestScore ||
            (score == bestScore && best != nullptr && biome.priority > best->priority);
        if (better) {
            bestScore = score;
            best = &biome;
        }
    }

    return best;
}

}  // namespace voxel
