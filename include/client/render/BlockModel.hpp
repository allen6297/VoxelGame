#pragma once

#include <array>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace voxel {

struct ModelFace {
    std::array<float, 4> uv { 0.0f, 0.0f, 16.0f, 16.0f };
    std::string rawRef;   // original reference e.g. "#all", kept for parent re-resolution
    std::string texture;  // resolved asset path, set after full texture map is merged
};

struct ModelElementRotation {
    std::array<float, 3> origin { 8.0f, 8.0f, 8.0f };
    int axis = 1;        // 0=X, 1=Y, 2=Z
    float angle = 0.0f;
    bool rescale = false;
};

struct ModelElement {
    std::array<float, 3> from { 0.0f, 0.0f, 0.0f };
    std::array<float, 3> to   { 16.0f, 16.0f, 16.0f };
    std::optional<ModelElementRotation> rotation;
    // Keys: "north", "south", "east", "west", "up", "down"
    std::unordered_map<std::string, ModelFace> faces;
};

struct BlockModel {
    std::string id;  // relative path used as cache key
    std::vector<ModelElement> elements;
    // Raw (unresolved) texture variable map — kept so child models can re-resolve
    // inherited elements against the merged texture map.
    // e.g. "all" -> "#side", "side" -> "textures/blocks/stone.ppm"
    std::unordered_map<std::string, std::string> rawTextures;
    // Fully resolved texture paths — populated after parent chain is merged.
    // e.g. "all" -> "textures/blocks/stone.ppm"
    std::unordered_map<std::string, std::string> textures;
};

}  // namespace voxel
