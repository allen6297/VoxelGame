#include "world/WorldSimulation.hpp"
#include <random>
#include <algorithm>
#include <iostream>
#include "Player.hpp"

namespace voxel {
namespace {

// GameInternal-like helpers but local to avoid heavy include or move them later
bool boolProperty(const BlockDefinition& def, const std::string& key, const bool fallback = false) {
    const auto it = def.properties.find(key);
    if (it == def.properties.end() || !std::holds_alternative<bool>(it->second)) {
        return fallback;
    }
    return std::get<bool>(it->second);
}

bool isCropBlock(const BlockDefinition& def) {
    return boolProperty(def, "crop");
}

bool isCropSoil(const GameData& gameData, const std::uint16_t stateId) {
    const BlockDefinition* def = findBlockDefinitionForBlockType(gameData, stateId);
    return def != nullptr && (def->id == "base:dirt" || def->id == "base:grass" ||
                              def->id == "dirt" || def->id == "grass");
}

int cropAge(const GameData& gameData, const std::uint16_t stateId) {
    const auto age = getStateProperty(gameData, stateId, "age");
    if (!age.has_value() || !std::holds_alternative<int>(*age)) {
        return 0;
    }
    return std::get<int>(*age);
}

int cropMaxAge(const BlockDefinition& def) {
    const auto it = def.properties.find("maxAge");
    if (it == def.properties.end() || !std::holds_alternative<int>(it->second)) {
        return 0;
    }
    return std::get<int>(it->second);
}

} // namespace

BlockChangeResult WorldSimulation::applyBlockChange(const Int3& block, const std::uint16_t stateId) {
    if (!isYInBounds(block.y)) {
        return {};
    }

    const ChunkCoord coord = worldToChunkCoord(block.x, block.y, block.z);
    if (!chunkLoaded(world_, coord)) {
        return {};
    }

    const std::uint16_t current = getBlock(world_, block.x, block.y, block.z);
    if (current == stateId) {
        return {true, false};
    }

    setBlock(world_, block.x, block.y, block.z, stateId);
    return {true, true};
}

void WorldSimulation::tick(const GameData& gameData, float deltaTime, const std::function<void(const Int3&, std::uint16_t)>& onBlockChanged) {
    worldTime_ += deltaTime;
    tickEntities(gameData, deltaTime);
    tickLiquids(gameData, deltaTime, onBlockChanged);
    tickChunks(gameData, onBlockChanged);
}

void WorldSimulation::setBlockWithNotify(const Int3& pos, std::uint16_t stateId, const std::function<void(const Int3&, std::uint16_t)>& onBlockChanged) {
    const BlockChangeResult result = applyBlockChange(pos, stateId);
    if (result.applied && result.changed && onBlockChanged) {
        onBlockChanged(pos, stateId);
    }
}

std::uint32_t WorldSimulation::spawnEntity(std::unique_ptr<Entity> entity) {
    if (entity->id == 0) {
        entity->id = nextEntityId_++;
    }
    const std::uint32_t id = entity->id;
    entityHash_.add(entity.get());
    entities_.push_back(std::move(entity));
    return id;
}

void WorldSimulation::removeEntity(std::uint32_t id) {
    auto it = std::find_if(entities_.begin(), entities_.end(), [id](const auto& e) { return e->id == id; });
    if (it != entities_.end()) {
        entityHash_.remove(it->get());
        entities_.erase(it);
    }
}

Entity* WorldSimulation::findEntity(std::uint32_t id) {
    for (auto& entity : entities_) {
        if (entity->id == id) return entity.get();
    }
    return nullptr;
}

void WorldSimulation::queryEntities(const Vec3& center, float radius, std::vector<Entity*>& outEntities) const {
    entityHash_.query(center, radius, outEntities);
}

void WorldSimulation::tickEntities(const GameData& gameData, float deltaTime) {
    for (auto& entity : entities_) {
        const Vec3 oldPos = entity->position;
        if (entity->type == EntityType::Item) {
            auto* item = static_cast<ItemEntity*>(entity.get());
            item->age += deltaTime;

            // Simple gravity for items
            if (!item->grounded) {
                item->velocity.y -= 9.81f * deltaTime;
            }

            // Simple movement and collision
            const Vec3 nextPos = item->position + item->velocity * deltaTime;
            if (!playerCollidesAt(world_, gameData, nextPos)) {
                item->position = nextPos;
                item->grounded = false;
            } else {
                item->velocity = {0, 0, 0};
                item->grounded = true;
            }
        }
        entityHash_.update(entity.get(), oldPos);
    }
}

void WorldSimulation::tickLiquids(const GameData& gameData, float deltaTime, const std::function<void(const Int3&, std::uint16_t)>& onBlockChanged) {
    constexpr float kLiquidStepSeconds = 0.20f;
    liquidStepAccumulator_ += deltaTime;
    if (liquidStepAccumulator_ < kLiquidStepSeconds) {
        return;
    }
    liquidStepAccumulator_ = 0.0f;

    struct LiquidMove {
        Int3 from;
        Int3 to;
        std::uint16_t blockType;
    };

    std::vector<LiquidMove> moves;
    moves.reserve(64);

    for (const auto& [coord, chunk] : world_.chunks) {
        const int baseX = coord.x * kChunkX;
        const int baseY = coord.y * kChunkY;
        const int baseZ = coord.z * kChunkZ;
        const int yStart = (baseY == 0) ? 1 : 0;

        for (int lx = 0; lx < kChunkX; ++lx) {
            for (int ly = yStart; ly < kChunkY; ++ly) {
                for (int lz = 0; lz < kChunkZ; ++lz) {
                    const std::uint16_t blockType = chunk.blocks[lx][ly][lz];
                    if (blockType == 0 || !isLiquidBlockType(gameData, blockType)) {
                        continue;
                    }

                    const int wx = baseX + lx;
                    const int wy = baseY + ly;
                    const int wz = baseZ + lz;

                    // Try to flow down
                    if (!isOccupied(world_, wx, wy - 1, wz)) {
                        moves.push_back({{wx, wy, wz}, {wx, wy - 1, wz}, blockType});
                    }
                }
            }
        }
    }

    for (const auto& move : moves) {
        if (getBlock(world_, move.from.x, move.from.y, move.from.z) != move.blockType) {
            continue;
        }
        if (isOccupied(world_, move.to.x, move.to.y, move.to.z)) {
            continue;
        }

        setBlockWithNotify(move.from, 0, onBlockChanged);
        setBlockWithNotify(move.to, move.blockType, onBlockChanged);
    }

    // Spreading (simplified: only water for now)
    for (const auto& [coord, chunk] : world_.chunks) {
        const int baseX = coord.x * kChunkX;
        const int baseY = coord.y * kChunkY;
        const int baseZ = coord.z * kChunkZ;

        for (int lx = 0; lx < kChunkX; ++lx) {
            for (int ly = 0; ly < kChunkY; ++ly) {
                for (int lz = 0; lz < kChunkZ; ++lz) {
                    const std::uint16_t blockType = chunk.blocks[lx][ly][lz];
                    if (blockType == 0 || !isLiquidBlockType(gameData, blockType)) {
                        continue;
                    }

                    const int wx = baseX + lx;
                    const int wy = baseY + ly;
                    const int wz = baseZ + lz;

                    // If we can't flow down, try to spread sideways
                    if (isOccupied(world_, wx, wy - 1, wz)) {
                        static const Int3 dirs[] = {{1,0,0}, {-1,0,0}, {0,0,1}, {0,0,-1}};
                        for (const auto& d : dirs) {
                            if (!isOccupied(world_, wx + d.x, wy, wz + d.z)) {
                                setBlockWithNotify({wx + d.x, wy, wz + d.z}, blockType, onBlockChanged);
                            }
                        }
                    }
                }
            }
        }
    }
}

void WorldSimulation::tickChunks(const GameData& gameData, const std::function<void(const Int3&, std::uint16_t)>& onBlockChanged) {
    for (auto& [coord, chunk] : world_.chunks) {
        tickRandomInChunk(gameData, chunk, coord, onBlockChanged);
    }
}

void WorldSimulation::tickRandomInChunk(const GameData& gameData, Chunk& chunk, const ChunkCoord& coord, const std::function<void(const Int3&, std::uint16_t)>& onBlockChanged) {
    static std::mt19937 gen(std::random_device{}());
    std::uniform_int_distribution<int> dist(0, kChunkX - 1);
    std::uniform_int_distribution<int> distY(0, kWorldY - 1);
    std::uniform_int_distribution<int> distZ(0, kChunkZ - 1);

    for (int i = 0; i < 6; ++i) {
        const int lx = dist(gen);
        const int ly = distY(gen);
        const int lz = distZ(gen);

        const std::uint16_t stateId = chunk.blocks[lx][ly][lz];
        if (stateId == 0) continue;

        const Int3 worldPos {
            coord.x * kChunkX + lx,
            ly,
            coord.z * kChunkZ + lz
        };

        tickBlock(gameData, worldPos, stateId, onBlockChanged);
    }
}

void WorldSimulation::tickBlock(const GameData& gameData, const Int3& pos, const std::uint16_t stateId, const std::function<void(const Int3&, std::uint16_t)>& onBlockChanged) {
    const BlockDefinition* def = findBlockDefinitionForBlockType(gameData, stateId);
    if (!def) return;

    if (isCropBlock(*def)) {
        const int age = cropAge(gameData, stateId);
        const int maxAge = cropMaxAge(*def);

        if (age < maxAge) {
            // Check soil
            const std::uint16_t soilId = getBlock(world_, pos.x, pos.y - 1, pos.z);
            if (!isCropSoil(gameData, soilId)) {
                return;
            }

            // Random chance to grow
            static std::mt19937 gen(std::random_device{}());
            std::uniform_int_distribution<int> growDist(0, 10);
            if (growDist(gen) == 0) {
                // Find next state with age + 1
                std::unordered_map<std::string, BlockProperty> props;
                props["age"] = age + 1;
                const auto nextState = runtimeIdForBlockState(gameData, def->id, props);
                if (nextState.has_value()) {
                    setBlockWithNotify(pos, *nextState, onBlockChanged);
                }
            }
        }
    }
}

}  // namespace voxel
