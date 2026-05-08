#pragma once

#include <filesystem>
#include <optional>
#include "world/Chunk.hpp"
#include "Player.hpp"

namespace voxel {

class WorldPersistence {
public:
    WorldPersistence(std::filesystem::path saveDir);

    bool saveChunk(Chunk& chunk);
    std::optional<Chunk> loadChunk(const ChunkCoord& coord);

    bool savePlayer(const Player& player);
    std::optional<Player> loadPlayer(const std::string& name);

private:
    std::filesystem::path getLegacyChunkPath(const ChunkCoord& coord) const;
    std::filesystem::path getRegionPath(const ChunkCoord& regionCoord) const;
    std::filesystem::path getPlayerPath(const std::string& name) const;
    std::filesystem::path saveDir_;
};

} // namespace voxel
