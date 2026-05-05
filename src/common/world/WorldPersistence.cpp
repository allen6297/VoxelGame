#include "world/WorldPersistence.hpp"
#include <fstream>
#include "Player.hpp"

namespace voxel {

WorldPersistence::WorldPersistence(std::filesystem::path saveDir) : saveDir_(std::move(saveDir)) {
    if (!std::filesystem::exists(saveDir_)) {
        std::filesystem::create_directories(saveDir_);
    }
}

bool WorldPersistence::saveChunk(const Chunk& chunk) {
    const auto path = getChunkPath(chunk.coord);
    std::ofstream out(path, std::ios::binary);
    if (!out) return false;

    // Write coordinate for sanity check
    out.write(reinterpret_cast<const char*>(&chunk.coord), sizeof(ChunkCoord));
    // Write blocks
    out.write(reinterpret_cast<const char*>(&chunk.blocks), sizeof(ChunkBlocks));
    // Write tint colors
    out.write(reinterpret_cast<const char*>(&chunk.tintColors), sizeof(ChunkTintColors));

    return true;
}

std::optional<Chunk> WorldPersistence::loadChunk(const ChunkCoord& coord) {
    const auto path = getChunkPath(coord);
    if (!std::filesystem::exists(path)) return std::nullopt;

    std::ifstream in(path, std::ios::binary);
    if (!in) return std::nullopt;

    Chunk chunk;
    ChunkCoord storedCoord;
    in.read(reinterpret_cast<char*>(&storedCoord), sizeof(ChunkCoord));
    
    if (storedCoord.x != coord.x || storedCoord.y != coord.y || storedCoord.z != coord.z) {
        return std::nullopt; // Corrupt or mismatched file
    }

    in.read(reinterpret_cast<char*>(&chunk.blocks), sizeof(ChunkBlocks));
    in.read(reinterpret_cast<char*>(&chunk.tintColors), sizeof(ChunkTintColors));
    
    chunk.coord = coord;
    return chunk;
}

bool WorldPersistence::savePlayer(const Player& player) {
    const auto path = getPlayerPath(player.name);
    std::ofstream out(path, std::ios::binary);
    if (!out) return false;

    // Write position
    out.write(reinterpret_cast<const char*>(&player.position), sizeof(Vec3));
    // Write rotation
    out.write(reinterpret_cast<const char*>(&player.yaw), sizeof(float));
    out.write(reinterpret_cast<const char*>(&player.pitch), sizeof(float));
    // Write inventory
    out.write(reinterpret_cast<const char*>(&player.inventory.selectedIndex), sizeof(int));
    for (const auto& slot : player.inventory.slots) {
        std::uint32_t idLen = static_cast<std::uint32_t>(slot.itemId.length());
        out.write(reinterpret_cast<const char*>(&idLen), sizeof(std::uint32_t));
        out.write(slot.itemId.data(), idLen);
        out.write(reinterpret_cast<const char*>(&slot.count), sizeof(int));
    }

    return true;
}

std::optional<Player> WorldPersistence::loadPlayer(const std::string& name) {
    const auto path = getPlayerPath(name);
    if (!std::filesystem::exists(path)) return std::nullopt;

    std::ifstream in(path, std::ios::binary);
    if (!in) return std::nullopt;

    Player player;
    player.name = name;

    in.read(reinterpret_cast<char*>(&player.position), sizeof(Vec3));
    in.read(reinterpret_cast<char*>(&player.yaw), sizeof(float));
    in.read(reinterpret_cast<char*>(&player.pitch), sizeof(float));
    
    in.read(reinterpret_cast<char*>(&player.inventory.selectedIndex), sizeof(int));
    for (auto& slot : player.inventory.slots) {
        std::uint32_t idLen = 0;
        in.read(reinterpret_cast<char*>(&idLen), sizeof(std::uint32_t));
        if (idLen > 256) return std::nullopt; // Safety break
        slot.itemId.assign(idLen, '\0');
        in.read(slot.itemId.data(), idLen);
        in.read(reinterpret_cast<char*>(&slot.count), sizeof(int));
    }

    return player;
}

std::filesystem::path WorldPersistence::getChunkPath(const ChunkCoord& coord) const {
    char filename[64];
    snprintf(filename, sizeof(filename), "chunk_%d_%d_%d.bin", coord.x, coord.y, coord.z);
    return saveDir_ / filename;
}

std::filesystem::path WorldPersistence::getPlayerPath(const std::string& name) const {
    // Basic sanitization
    std::string safeName = name;
    std::replace_if(safeName.begin(), safeName.end(), [](char c) {
        return !std::isalnum(c);
    }, '_');
    return saveDir_ / ("player_" + safeName + ".bin");
}

} // namespace voxel
