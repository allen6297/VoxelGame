#pragma once

#include "pack/PackManager.hpp"

#include <filesystem>
#include <optional>
#include <string>

namespace voxel {

// Resolves asset IDs to filesystem paths across all loaded packs.
//
// ID format:  "namespace:relative/path"   e.g. "base:textures/blocks/grass.ppm"
//             "relative/path"             searches all packs in priority order
//
// Assets live at  packs/{id}/assets/{relative/path}
class AssetPackManager {
public:
    explicit AssetPackManager(const PackManager& packManager);

    // Returns the filesystem path to the asset, nullopt if not found in any pack.
    std::optional<std::filesystem::path> resolve(const std::string& assetId) const;

    // Convenience: resolve to string, returns fallback if not found.
    std::string resolvePath(const std::string& assetId,
                            const std::string& fallback = "") const;

private:
    const PackManager& packManager_;

    // Splits "namespace:path" → {namespace, path}.
    // If no colon is present, namespace is empty.
    static std::pair<std::string, std::string> parseId(const std::string& assetId);
};

}  // namespace voxel
