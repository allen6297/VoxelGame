#pragma once

#include <cstdint>
#include <vector>
#include <memory>

#include "data/GameData.hpp"
#include "Math.hpp"
#include "world/TerrainGenerator.hpp"
#include "world/World.hpp"
#include "world/SpatialHash.hpp"
#include "Entity.hpp"

namespace voxel {

struct BlockChangeResult {
    bool applied = false;
    bool changed = false;
};

class WorldSimulation {
public:
    World& world() { return world_; }
    const World& world() const { return world_; }

    TerrainGenerator& terrainGenerator() { return terrainGen_; }
    const TerrainGenerator& terrainGenerator() const { return terrainGen_; }

    BlockChangeResult applyBlockChange(const Int3& block, std::uint16_t stateId);

    void tick(const GameData& gameData, float deltaTime, const std::function<void(const Int3&, std::uint16_t)>& onBlockChanged = nullptr);

    void setBlockWithNotify(const Int3& pos, std::uint16_t stateId, const std::function<void(const Int3&, std::uint16_t)>& onBlockChanged);

    std::uint32_t spawnEntity(std::unique_ptr<Entity> entity);
    void removeEntity(std::uint32_t id);
    Entity* findEntity(std::uint32_t id);
    void queryEntities(const Vec3& center, float radius, std::vector<Entity*>& outEntities) const;
    const std::vector<std::unique_ptr<Entity>>& entities() const { return entities_; }

    void setTime(double time) { worldTime_ = time; }
    double time() const { return worldTime_; }

private:
    void tickEntities(const GameData& gameData, float deltaTime);
    void tickLiquids(const GameData& gameData, float deltaTime, const std::function<void(const Int3&, std::uint16_t)>& onBlockChanged);
    void tickChunks(const GameData& gameData, const std::function<void(const Int3&, std::uint16_t)>& onBlockChanged);
    void tickRandomInChunk(const GameData& gameData, Chunk& chunk, const ChunkCoord& coord, const std::function<void(const Int3&, std::uint16_t)>& onBlockChanged);
    void tickBlock(const GameData& gameData, const Int3& pos, std::uint16_t stateId, const std::function<void(const Int3&, std::uint16_t)>& onBlockChanged);

    TerrainGenerator terrainGen_;
    World world_;
    SpatialHash entityHash_;
    std::vector<std::unique_ptr<Entity>> entities_;
    std::uint32_t nextEntityId_ = 1000; // Start high to avoid conflicts with player IDs
    float liquidStepAccumulator_ = 0.0f;
    double worldTime_ = 0.0;
    std::unordered_map<ChunkCoord, double, ChunkCoordHash> lastChunkAccess_;
};

}  // namespace voxel
