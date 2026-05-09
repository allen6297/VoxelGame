#pragma once
#include <filesystem>

namespace voxel {
    /**
     * Returns the standard OS path for application data.
     * Windows: %APPDATA%\TERRALITE
     * Linux: ~/.local/share/TERRALITE (or $XDG_DATA_HOME)
     * macOS: ~/Library/Application Support/TERRALITE
     */
    std::filesystem::path getStandardSavePath();
}
