#pragma once
#include <filesystem>

namespace voxel {
    /**
     * Returns the standard OS path for application data.
     * Windows: %APPDATA%\VoxelGame
     * Linux: ~/.local/share/VoxelGame (or $XDG_DATA_HOME)
     * macOS: ~/Library/Application Support/VoxelGame
     */
    std::filesystem::path getStandardSavePath();
}
