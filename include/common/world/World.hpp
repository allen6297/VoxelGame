#pragma once

#include <optional>
#include <unordered_map>

#include "data/GameData.hpp"
#include "Math.hpp"
#include "world/Chunk.hpp"

namespace voxel {
constexpr float kReach   = 6.0f;
constexpr float kRayStep = 0.1f;

struct RaycastHit {
    Int3 block;
    Int3 previousEmpty;
    std::uint16_t type;
    Vec3 hitNormal;      // face normal at hit point (±1 on one axis, 0 on others)
    CollisionBox hitBox; // 0-1 block-local collision box that was hit
};

struct World {
    std::unordered_map<ChunkCoord, Chunk, ChunkCoordHash> chunks;
};

bool isYInBounds(int y);
bool inChunkBounds(const ChunkCoord& coord);
ChunkCoord worldToChunkCoord(int x, int y, int z);
Int3 worldToLocalBlock(int x, int y, int z);
bool chunkLoaded(const World& world, const ChunkCoord& coord);
Chunk& getChunk(World& world, const ChunkCoord& coord);
const Chunk& getChunk(const World& world, const ChunkCoord& coord);
std::uint16_t getBlock(const World& world, int x, int y, int z);
void setBlock(World& world, int x, int y, int z, std::uint16_t block);
bool isSolid(const World& world, const GameData& gameData, int x, int y, int z);
bool isOccupied(const World& world, int x, int y, int z);
bool isObstructedByModel(const World& world, const GameData& gameData, const Int3& target);
std::optional<RaycastHit> raycastWorld(const World& world, const GameData& gameData, const Vec3& origin, const Vec3& direction);
}  // namespace voxel
