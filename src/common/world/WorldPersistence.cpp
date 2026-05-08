#include "world/WorldPersistence.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstring>
#include <fstream>
#include <unordered_map>
#include <utility>
#include <vector>

#include "Player.hpp"

namespace voxel {
namespace {

constexpr std::array<char, 4> kRegionMagic {'T', 'R', 'R', '1'};
constexpr std::uint32_t kRegionVersion = 1;
constexpr std::uint32_t kMaxChunkPayloadBytes =
    static_cast<std::uint32_t>(sizeof(ChunkBlocks) + sizeof(ChunkTintColors) + 4096);

struct RegionCoord {
    int x = 0;
    int y = 0;
    int z = 0;

    bool operator==(const RegionCoord&) const = default;
};

struct ChunkRecord {
    ChunkCoord coord;
    ChunkEncoding encoding = ChunkEncoding::Raw;
    std::vector<std::uint8_t> payload;
};

int floorDiv(const int a, const int b) {
    return a / b - (a % b != 0 && (a ^ b) < 0 ? 1 : 0);
}

int floorMod(const int a, const int b) {
    return ((a % b) + b) % b;
}

RegionCoord chunkToRegionCoord(const ChunkCoord& coord) {
    return {
        floorDiv(coord.x, kRegionChunkX),
        floorDiv(coord.y, kRegionChunkY),
        floorDiv(coord.z, kRegionChunkZ)
    };
}

std::uint16_t firstBlock(const ChunkBlocks& blocks) {
    return blocks[0][0][0];
}

bool isUniformChunk(const ChunkBlocks& blocks) {
    const std::uint16_t value = firstBlock(blocks);
    for (const auto& x : blocks) {
        for (const auto& y : x) {
            for (const std::uint16_t block : y) {
                if (block != value) {
                    return false;
                }
            }
        }
    }
    return true;
}

template <typename T>
void appendValue(std::vector<std::uint8_t>& bytes, const T& value) {
    const auto* begin = reinterpret_cast<const std::uint8_t*>(&value);
    bytes.insert(bytes.end(), begin, begin + sizeof(T));
}

template <typename T>
bool readValue(const std::vector<std::uint8_t>& bytes, std::size_t& offset, T& value) {
    if (offset + sizeof(T) > bytes.size()) {
        return false;
    }
    std::memcpy(&value, bytes.data() + offset, sizeof(T));
    offset += sizeof(T);
    return true;
}

std::vector<std::uint8_t> encodeUniformBlocks(const ChunkBlocks& blocks) {
    std::vector<std::uint8_t> payload;
    appendValue(payload, firstBlock(blocks));
    return payload;
}

std::vector<std::uint8_t> encodeRawBlocks(const ChunkBlocks& blocks) {
    std::vector<std::uint8_t> payload;
    payload.reserve(sizeof(ChunkBlocks));
    const auto* begin = reinterpret_cast<const std::uint8_t*>(&blocks);
    payload.insert(payload.end(), begin, begin + sizeof(ChunkBlocks));
    return payload;
}

bool tryEncodePaletteBlocks(const ChunkBlocks& blocks, std::vector<std::uint8_t>& payload) {
    std::vector<std::uint16_t> palette;
    std::unordered_map<std::uint16_t, std::uint16_t> paletteIndexByBlock;
    std::array<std::uint16_t, kChunkX * kChunkY * kChunkZ> indices {};
    std::size_t indexOffset = 0;

    for (const auto& x : blocks) {
        for (const auto& y : x) {
            for (const std::uint16_t block : y) {
                auto [it, inserted] = paletteIndexByBlock.emplace(block, static_cast<std::uint16_t>(palette.size()));
                if (inserted) {
                    palette.push_back(block);
                }
                indices[indexOffset++] = it->second;
            }
        }
    }

    payload.clear();
    appendValue(payload, static_cast<std::uint16_t>(palette.size()));
    for (const std::uint16_t block : palette) {
        appendValue(payload, block);
    }
    const auto* begin = reinterpret_cast<const std::uint8_t*>(indices.data());
    payload.insert(payload.end(), begin, begin + indices.size() * sizeof(std::uint16_t));
    return payload.size() < sizeof(ChunkBlocks);
}

ChunkRecord encodeChunk(const Chunk& chunk) {
    ChunkRecord record;
    record.coord = chunk.coord;

    if (isUniformChunk(chunk.blocks)) {
        record.encoding = ChunkEncoding::Uniform;
        record.payload = encodeUniformBlocks(chunk.blocks);
    } else if (tryEncodePaletteBlocks(chunk.blocks, record.payload)) {
        record.encoding = ChunkEncoding::Palette;
    } else {
        record.encoding = ChunkEncoding::Raw;
        record.payload = encodeRawBlocks(chunk.blocks);
    }

    const auto* tintBegin = reinterpret_cast<const std::uint8_t*>(&chunk.tintColors);
    record.payload.insert(record.payload.end(), tintBegin, tintBegin + sizeof(ChunkTintColors));
    return record;
}

bool decodeChunkRecord(const ChunkRecord& record, Chunk& chunk) {
    std::size_t offset = 0;
    chunk.coord = record.coord;

    if (record.encoding == ChunkEncoding::Uniform) {
        std::uint16_t block = 0;
        if (!readValue(record.payload, offset, block)) {
            return false;
        }
        for (auto& x : chunk.blocks) {
            for (auto& y : x) {
                for (std::uint16_t& cell : y) {
                    cell = block;
                }
            }
        }
    } else if (record.encoding == ChunkEncoding::Palette) {
        std::uint16_t paletteSize = 0;
        if (!readValue(record.payload, offset, paletteSize) || paletteSize == 0) {
            return false;
        }

        std::vector<std::uint16_t> palette(paletteSize);
        for (std::uint16_t& block : palette) {
            if (!readValue(record.payload, offset, block)) {
                return false;
            }
        }

        for (auto& x : chunk.blocks) {
            for (auto& y : x) {
                for (std::uint16_t& cell : y) {
                    std::uint16_t paletteIndex = 0;
                    if (!readValue(record.payload, offset, paletteIndex) || paletteIndex >= palette.size()) {
                        return false;
                    }
                    cell = palette[paletteIndex];
                }
            }
        }
    } else if (record.encoding == ChunkEncoding::Raw) {
        if (offset + sizeof(ChunkBlocks) > record.payload.size()) {
            return false;
        }
        std::memcpy(&chunk.blocks, record.payload.data() + offset, sizeof(ChunkBlocks));
        offset += sizeof(ChunkBlocks);
    } else {
        return false;
    }

    if (offset + sizeof(ChunkTintColors) > record.payload.size()) {
        return false;
    }
    std::memcpy(&chunk.tintColors, record.payload.data() + offset, sizeof(ChunkTintColors));
    chunk.dirty = false;
    return true;
}

bool readRegionFile(const std::filesystem::path& path, std::vector<ChunkRecord>& records) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        return false;
    }

    std::array<char, 4> magic {};
    std::uint32_t version = 0;
    RegionCoord regionCoord;
    std::uint32_t recordCount = 0;
    in.read(magic.data(), magic.size());
    in.read(reinterpret_cast<char*>(&version), sizeof(version));
    in.read(reinterpret_cast<char*>(&regionCoord), sizeof(regionCoord));
    in.read(reinterpret_cast<char*>(&recordCount), sizeof(recordCount));
    if (!in || magic != kRegionMagic || version != kRegionVersion) {
        return false;
    }

    records.clear();
    records.reserve(recordCount);
    for (std::uint32_t i = 0; i < recordCount; ++i) {
        ChunkRecord record;
        std::uint8_t encoding = 0;
        std::uint32_t payloadSize = 0;
        in.read(reinterpret_cast<char*>(&record.coord), sizeof(record.coord));
        in.read(reinterpret_cast<char*>(&encoding), sizeof(encoding));
        in.read(reinterpret_cast<char*>(&payloadSize), sizeof(payloadSize));
        if (!in || payloadSize > kMaxChunkPayloadBytes) {
            return false;
        }

        record.encoding = static_cast<ChunkEncoding>(encoding);
        record.payload.resize(payloadSize);
        in.read(reinterpret_cast<char*>(record.payload.data()), payloadSize);
        if (!in) {
            return false;
        }
        records.push_back(std::move(record));
    }

    return true;
}

bool writeRegionFile(const std::filesystem::path& path, const RegionCoord& regionCoord, const std::vector<ChunkRecord>& records) {
    std::filesystem::create_directories(path.parent_path());

    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out) {
        return false;
    }

    const std::uint32_t recordCount = static_cast<std::uint32_t>(records.size());
    out.write(kRegionMagic.data(), kRegionMagic.size());
    out.write(reinterpret_cast<const char*>(&kRegionVersion), sizeof(kRegionVersion));
    out.write(reinterpret_cast<const char*>(&regionCoord), sizeof(regionCoord));
    out.write(reinterpret_cast<const char*>(&recordCount), sizeof(recordCount));

    for (const ChunkRecord& record : records) {
        const std::uint8_t encoding = static_cast<std::uint8_t>(record.encoding);
        const std::uint32_t payloadSize = static_cast<std::uint32_t>(record.payload.size());
        out.write(reinterpret_cast<const char*>(&record.coord), sizeof(record.coord));
        out.write(reinterpret_cast<const char*>(&encoding), sizeof(encoding));
        out.write(reinterpret_cast<const char*>(&payloadSize), sizeof(payloadSize));
        out.write(reinterpret_cast<const char*>(record.payload.data()), payloadSize);
    }

    return static_cast<bool>(out);
}

bool sameCoord(const ChunkCoord& a, const ChunkCoord& b) {
    return a.x == b.x && a.y == b.y && a.z == b.z;
}

}  // namespace

WorldPersistence::WorldPersistence(std::filesystem::path saveDir) : saveDir_(std::move(saveDir)) {
    if (!std::filesystem::exists(saveDir_)) {
        std::filesystem::create_directories(saveDir_);
    }
    std::filesystem::create_directories(saveDir_ / "regions");
}

bool WorldPersistence::saveChunk(Chunk& chunk) {
    const RegionCoord regionCoord = chunkToRegionCoord(chunk.coord);
    const auto path = getRegionPath({regionCoord.x, regionCoord.y, regionCoord.z});

    std::vector<ChunkRecord> records;
    if (std::filesystem::exists(path) && !readRegionFile(path, records)) {
        return false;
    }

    auto existing = std::find_if(records.begin(), records.end(), [&chunk](const ChunkRecord& record) {
        return sameCoord(record.coord, chunk.coord);
    });
    const bool alreadySaved = existing != records.end();
    if (!chunk.dirty && alreadySaved) {
        return true;
    }

    ChunkRecord encoded = encodeChunk(chunk);
    if (existing == records.end()) {
        records.push_back(std::move(encoded));
    } else {
        *existing = std::move(encoded);
    }

    if (!writeRegionFile(path, regionCoord, records)) {
        return false;
    }

    chunk.dirty = false;
    return true;
}

std::optional<Chunk> WorldPersistence::loadChunk(const ChunkCoord& coord) {
    const RegionCoord regionCoord = chunkToRegionCoord(coord);
    const auto regionPath = getRegionPath({regionCoord.x, regionCoord.y, regionCoord.z});

    if (std::filesystem::exists(regionPath)) {
        std::vector<ChunkRecord> records;
        if (readRegionFile(regionPath, records)) {
            const auto record = std::find_if(records.begin(), records.end(), [&coord](const ChunkRecord& candidate) {
                return sameCoord(candidate.coord, coord);
            });
            if (record != records.end()) {
                Chunk chunk;
                if (decodeChunkRecord(*record, chunk)) {
                    return chunk;
                }
            }
        }
    }

    const auto legacyPath = getLegacyChunkPath(coord);
    if (!std::filesystem::exists(legacyPath)) {
        return std::nullopt;
    }

    std::ifstream in(legacyPath, std::ios::binary);
    if (!in) {
        return std::nullopt;
    }

    Chunk chunk;
    ChunkCoord storedCoord;
    in.read(reinterpret_cast<char*>(&storedCoord), sizeof(ChunkCoord));

    if (!sameCoord(storedCoord, coord)) {
        return std::nullopt;
    }

    in.read(reinterpret_cast<char*>(&chunk.blocks), sizeof(ChunkBlocks));
    in.read(reinterpret_cast<char*>(&chunk.tintColors), sizeof(ChunkTintColors));
    if (!in) {
        return std::nullopt;
    }

    chunk.coord = coord;
    chunk.dirty = false;
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

std::filesystem::path WorldPersistence::getLegacyChunkPath(const ChunkCoord& coord) const {
    char filename[64];
    snprintf(filename, sizeof(filename), "chunk_%d_%d_%d.bin", coord.x, coord.y, coord.z);
    return saveDir_ / filename;
}

std::filesystem::path WorldPersistence::getRegionPath(const ChunkCoord& coord) const {
    char filename[64];
    snprintf(filename, sizeof(filename), "r.%d.%d.%d.trr", coord.x, coord.y, coord.z);
    return saveDir_ / "regions" / filename;
}

std::filesystem::path WorldPersistence::getPlayerPath(const std::string& name) const {
    // Basic sanitization
    std::string safeName = name;
    std::replace_if(safeName.begin(), safeName.end(), [](char c) {
        return !std::isalnum(static_cast<unsigned char>(c));
    }, '_');
    return saveDir_ / ("player_" + safeName + ".bin");
}

} // namespace voxel
