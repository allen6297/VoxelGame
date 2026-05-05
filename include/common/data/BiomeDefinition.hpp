#pragma once

#include <array>
#include <string>
#include <unordered_map>

namespace voxel {

// Climate values sampled at a world column — all normalized to [0, 1]
struct ClimatePoint {
    float temperature = 0.5f;
    float humidity    = 0.5f;
    float rainfall    = 0.5f;
    float elevation   = 0.5f;
    float drainage    = 0.5f;
    float waterTable  = 0.5f;
};

struct BiomeClimateRange {
    float min = 0.0f;
    float max = 1.0f;
};

struct BiomeDefinition {
    std::string id;
    std::string name;
    float priority = 0.0f;  // tie-breaker when scores are equal
    float rarity   = 1.0f;  // score multiplier — <1 makes a biome rarer

    struct Climate {
        BiomeClimateRange temperature;
        BiomeClimateRange humidity;
        BiomeClimateRange rainfall;
        BiomeClimateRange elevation;
        BiomeClimateRange drainage;
        BiomeClimateRange waterTable;
    } climate;

    struct Terrain {
        float baseHeight      = 48.0f;
        float heightVariation = 12.0f;
    } terrain;

    struct Surface {
        std::string top    = "grass";
        std::string middle = "dirt";
        std::string base   = "stone";
        int middleDepth    = 3;
    } surface;

    struct Atmosphere {
        std::array<float, 3> skyColor   { 0.58f, 0.78f, 0.98f };
        std::array<float, 3> fogColor   { 0.75f, 0.85f, 0.95f };
        std::array<float, 3> waterColor { 0.20f, 0.45f, 0.80f };
    } atmosphere;

    struct Fertility {
        float nitrogen   = 0.5f;
        float phosphorus = 0.5f;
        float potassium  = 0.5f;
        float magnesium  = 0.5f;
        float calcium    = 0.5f;
        float sulfur     = 0.2f;
    } fertility;

    // Named tint colors applied to blocks that opt in via their tintKey
    // e.g. "grass" -> [0.45, 0.75, 0.25], "foliage" -> [0.35, 0.65, 0.20]
    std::unordered_map<std::string, std::array<float, 3>> colors;
};

}  // namespace voxel
