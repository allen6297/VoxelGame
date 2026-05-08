#include "world/World.hpp"

#include <algorithm>

namespace voxel {
namespace {
int floorDiv(const int a, const int b) {
    return a / b - (a % b != 0 && (a ^ b) < 0 ? 1 : 0);
}

int floorMod(const int a, const int b) {
    return ((a % b) + b) % b;
}
}  // namespace

bool isYInBounds(const int y) {
    return y >= 0 && y < kWorldY;
}

bool inChunkBounds(const ChunkCoord& coord) {
    return coord.y >= 0 && coord.y < kChunkCountY;
}

ChunkCoord worldToChunkCoord(const int x, const int y, const int z) {
    return {floorDiv(x, kChunkX), floorDiv(y, kChunkY), floorDiv(z, kChunkZ)};
}

Int3 worldToLocalBlock(const int x, const int y, const int z) {
    return {floorMod(x, kChunkX), floorMod(y, kChunkY), floorMod(z, kChunkZ)};
}

bool chunkLoaded(const World& world, const ChunkCoord& coord) {
    return world.chunks.count(coord) > 0;
}

Chunk& getChunk(World& world, const ChunkCoord& coord) {
    return world.chunks.at(coord);
}

const Chunk& getChunk(const World& world, const ChunkCoord& coord) {
    return world.chunks.at(coord);
}

std::uint16_t getBlock(const World& world, const int x, const int y, const int z) {
    if (!isYInBounds(y)) {
        return 0;
    }
    const ChunkCoord coord = worldToChunkCoord(x, y, z);
    const auto it = world.chunks.find(coord);
    if (it == world.chunks.end()) {
        return 0;
    }
    const Int3 local = worldToLocalBlock(x, y, z);
    return it->second.blocks[local.x][local.y][local.z];
}

void setBlock(World& world, const int x, const int y, const int z, const std::uint16_t block) {
    if (!isYInBounds(y)) {
        return;
    }
    const ChunkCoord coord = worldToChunkCoord(x, y, z);
    const auto it = world.chunks.find(coord);
    if (it == world.chunks.end()) {
        return;
    }
    const Int3 local = worldToLocalBlock(x, y, z);
    std::uint16_t& current = it->second.blocks[local.x][local.y][local.z];
    if (current != block) {
        current = block;
        it->second.dirty = true;
    }
}

bool isSolid(const World& world, const GameData& gameData, const int x, const int y, const int z) {
    const std::uint8_t block = getBlock(world, x, y, z);
    return block != 0 && gameData.solidByRuntimeId[block];
}

bool isOccupied(const World& world, const int x, const int y, const int z) {
    return getBlock(world, x, y, z) != 0;
}

bool isObstructedByModel(const World& world, const GameData& gameData, const Int3& target) {
    // Target block occupies [target, target+1] in each axis.
    const float tMinX = static_cast<float>(target.x);
    const float tMaxX = tMinX + 1.0f;
    const float tMinY = static_cast<float>(target.y);
    const float tMaxY = tMinY + 1.0f;
    const float tMinZ = static_cast<float>(target.z);
    const float tMaxZ = tMinZ + 1.0f;

    // Expand search by collisionSearchExpansion so oversized model elements
    // that extend beyond their block's unit cube are caught.
    const int exp = gameData.collisionSearchExpansion;
    for (int x = target.x - exp; x <= target.x + exp; ++x) {
        for (int y = target.y - exp; y <= target.y + exp; ++y) {
            for (int z = target.z - exp; z <= target.z + exp; ++z) {
                if (x == target.x && y == target.y && z == target.z) continue;
                const std::uint16_t stateId = getBlock(world, x, y, z);
                if (stateId == 0) continue;
                const BlockDefinition* def = findBlockDefinitionForBlockType(gameData, stateId);
                const std::vector<CollisionBox>* stateBoxes = collisionBoxesForState(gameData, stateId);
                // Blocks without model collision boxes are full cubes — they
                // can't overhang into a neighboring position.
                if (def == nullptr || ((stateBoxes == nullptr || stateBoxes->empty()) && def->collisionBoxes.empty())) continue;
                const float bx = static_cast<float>(x);
                const float by = static_cast<float>(y);
                const float bz = static_cast<float>(z);
                const std::vector<CollisionBox>& boxes =
                    (stateBoxes != nullptr && !stateBoxes->empty()) ? *stateBoxes : def->collisionBoxes;
                for (const auto& box : boxes) {
                    if (bx + box.maxX > tMinX && bx + box.minX < tMaxX &&
                        by + box.maxY > tMinY && by + box.minY < tMaxY &&
                        bz + box.maxZ > tMinZ && bz + box.minZ < tMaxZ) {
                        return true;
                    }
                }
            }
        }
    }
    return false;
}

std::optional<RaycastHit> raycastWorld(const World& world, const GameData& gameData, const Vec3& origin, const Vec3& direction) {
    static const std::vector<CollisionBox> kFullBox{{0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f}};

    float bestT = kReach + 1.0f;
    std::optional<RaycastHit> bestHit;
    const int exp = gameData.collisionSearchExpansion;

    for (float distance = 0.0f; distance <= kReach; distance += kRayStep) {
        const Vec3 point = origin + direction * distance;
        const int px = static_cast<int>(std::floor(point.x));
        const int py = static_cast<int>(std::floor(point.y));
        const int pz = static_cast<int>(std::floor(point.z));

        for (int nx = px - exp; nx <= px + exp; ++nx) {
            for (int ny = py - exp; ny <= py + exp; ++ny) {
                for (int nz = pz - exp; nz <= pz + exp; ++nz) {
                    if (!isYInBounds(ny)) continue;
                    const std::uint16_t stateId = getBlock(world, nx, ny, nz);
                    const BlockDefinition* def = findBlockDefinitionForBlockType(gameData, stateId);
                    if (stateId == 0 || def == nullptr || def->material == "liquid") continue;
                    const std::vector<CollisionBox>* stateBoxes = collisionBoxesForState(gameData, stateId);
                    const std::vector<CollisionBox>& boxes =
                        (stateBoxes != nullptr && !stateBoxes->empty()) ? *stateBoxes :
                        ((def && !def->collisionBoxes.empty()) ? def->collisionBoxes : kFullBox);

                    const float bx = static_cast<float>(nx);
                    const float by = static_cast<float>(ny);
                    const float bz = static_cast<float>(nz);

                    for (const auto& box : boxes) {
                        const Vec3 boxMin{bx + box.minX, by + box.minY, bz + box.minZ};
                        const Vec3 boxMax{bx + box.maxX, by + box.maxY, bz + box.maxZ};

                        // Ray-AABB slab intersection
                        float tmin = 1e-4f, tmax = 1e30f;
                        Vec3 normal{0.0f, 0.0f, 0.0f};
                        bool hit = true;

                        auto testAxis = [&](float orig, float d, float bmin, float bmax, Vec3 nFace) {
                            if (std::abs(d) < 1e-8f) {
                                if (orig < bmin || orig > bmax) hit = false;
                                return;
                            }
                            float t0 = (bmin - orig) / d;
                            float t1 = (bmax - orig) / d;
                            if (t0 > t1) { std::swap(t0, t1); nFace = {-nFace.x, -nFace.y, -nFace.z}; }
                            if (t0 > tmin) { tmin = t0; normal = nFace; }
                            if (t1 < tmax) tmax = t1;
                        };

                        testAxis(origin.x, direction.x, boxMin.x, boxMax.x, {-1.0f, 0.0f, 0.0f});
                        testAxis(origin.y, direction.y, boxMin.y, boxMax.y, {0.0f, -1.0f, 0.0f});
                        testAxis(origin.z, direction.z, boxMin.z, boxMax.z, {0.0f, 0.0f, -1.0f});

                        if (!hit || tmin > tmax || tmax < 0.0f || tmin >= bestT || tmin > kReach) continue;

                        bestT = tmin;
                        const Vec3 hitPoint = origin + direction * tmin;
                        auto snapOut = [](float pos, float n) -> int {
                            if (n > 0.5f)  return static_cast<int>(std::floor(pos + 0.5f));
                            if (n < -0.5f) return static_cast<int>(std::floor(pos - 0.5f));
                            return static_cast<int>(std::floor(pos));
                        };
                        bestHit = RaycastHit{
                            {nx, ny, nz},
                            {
                                snapOut(hitPoint.x, normal.x),
                                snapOut(hitPoint.y, normal.y),
                                snapOut(hitPoint.z, normal.z)
                            },
                            stateId,
                            normal,
                            box
                        };
                    }
                }
            }
        }
    }

    return bestHit;
}
}  // namespace voxel
