#pragma once

#include <string>
#include <unordered_map>

#include "render/BlockModel.hpp"

namespace voxel {

class ModelManager {
public:
    // Load a model from assetsRoot/relativePath and cache it. Returns nullptr on failure.
    const BlockModel* load(const std::string& relativePath, const std::string& assetsRoot);
    const BlockModel* get(const std::string& relativePath) const;

private:
    const BlockModel* loadInternal(const std::string& relativePath,
                                   const std::string& assetsRoot,
                                   int depth);
    std::unordered_map<std::string, BlockModel> models_;
};

}  // namespace voxel
