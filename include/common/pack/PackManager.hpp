#pragma once

#include "pack/Pack.hpp"

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace voxel {

// Discovers packs in the packs/ directory, manages load order,
// and provides unified file access across all loaded packs.
//
// Load order: index 0 = highest priority. "base" is always lowest.
// pack_order.json lives next to the packs/ directory.
class PackManager {
public:
    // Scan packsDir, read/write pack_order.json, sort by priority.
    void discover(const std::filesystem::path& packsDir);

    // Packs in load order, highest priority first.
    const std::vector<Pack>& packs() const { return packs_; }

    // Find a pack by id. Returns nullptr if not found.
    const Pack* findPack(const std::string& id) const;

    // Read a file from the highest-priority pack that contains it.
    std::optional<std::string> readFile(const std::string& relativePath) const;

    // All files under subdir across all packs, highest priority first, no duplicates.
    std::vector<std::string> listFiles(const std::string& subdir) const;

private:
    std::vector<Pack>     packs_;
    std::filesystem::path packsDir_;

    std::vector<std::string> loadOrder()                                     const;
    std::vector<std::string> buildOrder()                                    const;
    void                     saveOrder(const std::vector<std::string>& order) const;
    void                     applyOrder(const std::vector<std::string>& order);
};

}  // namespace voxel
