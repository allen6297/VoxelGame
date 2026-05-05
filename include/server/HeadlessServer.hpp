#pragma once

#include <chrono>
#include <cstdint>
#include <unordered_map>
#include <unordered_set>

#include "common/data/GameData.hpp"
#include "common/network/NetworkManager.hpp"
#include "common/world/WorldSimulation.hpp"
#include "common/world/ChunkTaskQueue.hpp"
#include "common/Player.hpp"
#include "common/world/WorldPersistence.hpp"
#include "common/pack/ScriptManager.hpp"

namespace voxel {

class HeadlessServer {
public:
    explicit HeadlessServer(GameData gameData);
    ~HeadlessServer();

    bool start(std::uint16_t port, const std::filesystem::path& projectRoot);
    void tick();

private:
    void processChunkRequests();
    void processBlockEditRequests();
    void processSelectSlotRequests();
    void processCraftRequests();
    void ensureChunksAroundPlayers();
    void ensureChunksAround(const ChunkCoord& center);
    void ensureChunksAroundPlayer(std::uint32_t playerId, const ChunkCoord& center, std::uint8_t radius);
    void expireUnusedChunks();
    void sendLoadedChunksToPlayer(std::uint32_t playerId, const ChunkCoord& center);
    bool ensureChunkLoaded(const ChunkCoord& coord);
    bool isKnownState(std::uint16_t stateId) const;
    bool isWithinReach(const NetworkBlockChange& request) const;
    bool isValidBlockEditTarget(const NetworkBlockChange& request);

    GameData gameData_;
    NetworkManager network_;
    WorldSimulation simulation_;
    std::unique_ptr<WorldPersistence> persistence_;
    std::unique_ptr<ChunkTaskQueue> chunkTasks_;
    std::unordered_set<ChunkCoord, ChunkCoordHash> pendingChunks_;
    std::chrono::steady_clock::time_point lastChunkMaintenance_;
    std::chrono::steady_clock::time_point lastTick_;
    std::unordered_map<std::uint32_t, Player> players_;
    std::unique_ptr<ScriptManager> scriptManager_;
    std::unordered_map<std::uint32_t, std::unordered_set<ChunkCoord, ChunkCoordHash>> sentChunksByPlayer_;
    std::unordered_map<std::uint32_t, std::unordered_map<ChunkCoord, Chunk, ChunkCoordHash>> lastSentChunkStateByPlayer_;
    std::unordered_map<std::uint32_t, std::unordered_set<std::uint32_t>> knownEntitiesByPlayer_;
};

}  // namespace voxel
