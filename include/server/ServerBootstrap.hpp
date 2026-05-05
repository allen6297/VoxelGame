#pragma once

#include <filesystem>

#include "common/data/GameData.hpp"

namespace voxel {

struct ServerContext {
    GameData gameData;
    std::filesystem::path projectRoot;
};

std::filesystem::path findProjectRoot();
ServerContext loadServerContext(const std::filesystem::path& projectRoot);

}  // namespace voxel
