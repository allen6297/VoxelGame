#include "pack/AssetPackManager.hpp"

#include <iostream>

namespace voxel {

AssetPackManager::AssetPackManager(const PackManager& packManager)
    : packManager_(packManager)
{}

std::pair<std::string, std::string> AssetPackManager::parseId(const std::string& assetId) {
    const auto colon = assetId.find(':');
    if (colon == std::string::npos) return {"", assetId};
    return {assetId.substr(0, colon), assetId.substr(colon + 1)};
}

std::optional<std::filesystem::path> AssetPackManager::resolve(const std::string& assetId) const {
    const auto [ns, relativePath] = parseId(assetId);

    if (!ns.empty()) {
        // Namespaced: look only in the specified pack
        const Pack* pack = packManager_.findPack(ns);
        if (!pack) {
            std::cerr << "AssetPackManager: unknown pack namespace '" << ns << "'\n";
            return std::nullopt;
        }

        if (pack->isZip()) {
            std::cerr << "AssetPackManager: zip asset resolution not yet supported for '"
                      << assetId << "'\n";
            return std::nullopt;
        }

        const auto fullPath = pack->path() / "assets" / relativePath;
        if (std::filesystem::exists(fullPath)) return fullPath;
        return std::nullopt;
    }

    // Unqualified: walk packs in priority order
    for (const auto& pack : packManager_.packs()) {
        if (pack.isZip()) continue;  // zip support deferred

        const auto fullPath = pack.path() / "assets" / relativePath;
        if (std::filesystem::exists(fullPath)) return fullPath;
    }

    return std::nullopt;
}

std::string AssetPackManager::resolvePath(const std::string& assetId,
                                          const std::string& fallback) const {
    const auto path = resolve(assetId);
    return path ? path->string() : fallback;
}

}  // namespace voxel
