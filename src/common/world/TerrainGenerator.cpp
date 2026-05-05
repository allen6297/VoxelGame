#include "world/TerrainGenerator.hpp"

#include <cmath>

#include "FastNoiseLite.h"
#include "world/BiomeDefinition.hpp"

namespace voxel {

namespace {

struct ClimateNoises {
    FastNoiseLite temperature;
    FastNoiseLite humidity;
    FastNoiseLite rainfall;
    FastNoiseLite elevation;
    FastNoiseLite drainage;
    FastNoiseLite waterTable;
};

ClimateNoises makeClimateNoises(int seed) {
    ClimateNoises cn;

    auto configure = [](FastNoiseLite& n, int s, float freq) {
        n.SetSeed(s);
        n.SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2);
        n.SetFractalType(FastNoiseLite::FractalType_FBm);
        n.SetFractalOctaves(4);
        n.SetFractalLacunarity(2.0f);
        n.SetFractalGain(0.5f);
        n.SetFrequency(freq);
    };

    configure(cn.temperature, seed + 0,  0.0015f);
    configure(cn.humidity,    seed + 11, 0.0020f);
    configure(cn.rainfall,    seed + 23, 0.0018f);
    configure(cn.elevation,   seed + 37, 0.0025f);
    configure(cn.drainage,    seed + 53, 0.0022f);
    configure(cn.waterTable,  seed + 71, 0.0017f);

    return cn;
}

inline float normalize(float v) { return (v + 1.0f) * 0.5f; }

ClimatePoint sampleClimate(const ClimateNoises& cn, float wx, float wz) {
    ClimatePoint p;
    p.temperature = normalize(cn.temperature.GetNoise(wx, wz));
    p.humidity    = normalize(cn.humidity.GetNoise(wx, wz));
    p.rainfall    = normalize(cn.rainfall.GetNoise(wx, wz));
    p.elevation   = normalize(cn.elevation.GetNoise(wx, wz));
    p.drainage    = normalize(cn.drainage.GetNoise(wx, wz));
    p.waterTable  = normalize(cn.waterTable.GetNoise(wx, wz));
    return p;
}

float columnHeight(const BiomeDefinition* biome, float elevation) {
    if (biome) {
        const float halfVar = biome->terrain.heightVariation * 0.5f;
        return biome->terrain.baseHeight + (elevation * 2.0f - 1.0f) * halfVar;
    }
    return 32.0f + (elevation * 2.0f - 1.0f) * 20.0f;
}

// Default fallback tint when a biome has no color for the requested key
static constexpr std::array<float, 3> kWhite {1.0f, 1.0f, 1.0f};

const std::array<float, 3>& biomeColor(const BiomeDefinition* biome, const std::string& key) {
    if (biome) {
        const auto it = biome->colors.find(key);
        if (it != biome->colors.end()) return it->second;
    }
    return kWhite;
}

// Result of neighbourhood blending for one column
struct BlendResult {
    float                   height;
    std::array<float, 3>    tintColor {1.0f, 1.0f, 1.0f};
    const BiomeDefinition*  dominantBiome = nullptr;
};

// Samples a 5×5 neighbourhood (step = 8 blocks) and returns blended height,
// blended grass tint, and the biome with the highest cumulative weight.
BlendResult blendColumn(
    const ClimateNoises& cn,
    const std::unordered_map<std::string, BiomeDefinition>& biomes,
    float wx, float wz)
{
    constexpr int   kSteps     = 2;
    constexpr float kBlendStep = 8.0f;
    constexpr float kSigmaSq   = (kBlendStep * kSteps * 0.5f) * (kBlendStep * kSteps * 0.5f);

    float totalWeight  = 0.0f;
    float totalHeight  = 0.0f;
    std::array<float, 3> totalTint {0.0f, 0.0f, 0.0f};

    // For dominant-biome selection: track weight accumulated per biome
    std::unordered_map<const BiomeDefinition*, float> biomeWeights;

    for (int di = -kSteps; di <= kSteps; ++di) {
        for (int dj = -kSteps; dj <= kSteps; ++dj) {
            const float sx = wx + di * kBlendStep;
            const float sz = wz + dj * kBlendStep;

            const ClimatePoint sc = sampleClimate(cn, sx, sz);
            const BiomeDefinition* sb = selectBiome(biomes, sc);

            const float h = columnHeight(sb, sc.elevation);
            const auto& tc = biomeColor(sb, "grass");

            const float dist2 = (di * kBlendStep) * (di * kBlendStep)
                              + (dj * kBlendStep) * (dj * kBlendStep);
            const float w = std::exp(-dist2 / (2.0f * kSigmaSq));

            totalHeight   += h * w;
            totalTint[0]  += tc[0] * w;
            totalTint[1]  += tc[1] * w;
            totalTint[2]  += tc[2] * w;
            totalWeight   += w;

            biomeWeights[sb] += w;
        }
    }

    BlendResult result;
    result.height      = totalHeight / totalWeight;
    result.tintColor   = {totalTint[0] / totalWeight,
                          totalTint[1] / totalWeight,
                          totalTint[2] / totalWeight};

    // Dominant biome = highest cumulative weight
    float bestWeight = -1.0f;
    for (const auto& [biome, w] : biomeWeights) {
        if (w > bestWeight) {
            bestWeight           = w;
            result.dominantBiome = biome;
        }
    }

    return result;
}

}  // namespace

Chunk TerrainGenerator::generateChunk(const ChunkCoord& coord, const GameData& gameData) const {
    Chunk chunk;
    chunk.coord = coord;

    const ClimateNoises cn = makeClimateNoises(seed);

    const int baseX = coord.x * kChunkX;
    const int baseY = coord.y * kChunkY;
    const int baseZ = coord.z * kChunkZ;

    const std::uint16_t grassId = runtimeIdForBlock(gameData, "grass");
    const std::uint16_t dirtId  = runtimeIdForBlock(gameData, "dirt");
    const std::uint16_t stoneId = runtimeIdForBlock(gameData, "stone");

    for (int lx = 0; lx < kChunkX; ++lx) {
        for (int lz = 0; lz < kChunkZ; ++lz) {
            const float wx = static_cast<float>(baseX + lx);
            const float wz = static_cast<float>(baseZ + lz);

            const BlendResult blend = blendColumn(cn, gameData.biomes, wx, wz);

            // Store blended tint for this column
            chunk.tintColors[lx][lz] = blend.tintColor;

            const int surfaceY = static_cast<int>(blend.height);

            // Surface blocks from the dominant biome in the neighbourhood
            std::uint16_t topId     = grassId;
            std::uint16_t middleId  = dirtId;
            std::uint16_t baseId    = stoneId;
            int           middleDepth = 3;

            const BiomeDefinition* biome = blend.dominantBiome;
            if (biome) {
                const auto resolveBlock = [&](const std::string& blockName,
                                              std::uint16_t fallback) -> std::uint16_t {
                    if (blockName.empty()) return fallback;
                    const std::uint16_t id = runtimeIdForBlock(gameData, blockName);
                    return id != 0 ? id : fallback;
                };
                topId       = resolveBlock(biome->surface.top,    grassId);
                middleId    = resolveBlock(biome->surface.middle, dirtId);
                baseId      = resolveBlock(biome->surface.base,   stoneId);
                middleDepth = biome->surface.middleDepth;
            }

            for (int ly = 0; ly < kChunkY; ++ly) {
                const int wy = baseY + ly;

                std::uint16_t blockType = 0;
                if (wy == surfaceY) {
                    blockType = topId;
                } else if (wy < surfaceY && wy >= surfaceY - middleDepth) {
                    blockType = middleId;
                } else if (wy < surfaceY - middleDepth) {
                    blockType = baseId;
                }
                chunk.blocks[lx][ly][lz] = blockType;
            }
        }
    }

    return chunk;
}

ClimatePoint TerrainGenerator::sampleClimateAt(const float wx, const float wz) const {
    const ClimateNoises cn = makeClimateNoises(seed);
    return sampleClimate(cn, wx, wz);
}

const BiomeDefinition* TerrainGenerator::selectBiomeAt(
    const float wx, const float wz,
    const std::unordered_map<std::string, BiomeDefinition>& biomes) const
{
    const ClimateNoises cn = makeClimateNoises(seed);
    return selectBiome(biomes, sampleClimate(cn, wx, wz));
}

}  // namespace voxel
