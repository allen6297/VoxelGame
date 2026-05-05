#include "OSUtils.hpp"
#include <cstdlib>

#ifdef _WIN32
#include <shlobj.h>
#include <windows.h>
#else
#include <unistd.h>
#include <sys/types.h>
#include <pwd.h>
#endif

namespace voxel {

std::filesystem::path getStandardSavePath() {
#ifdef _WIN32
    PWSTR path = nullptr;
    if (SHGetKnownFolderPath(FOLDERID_RoamingAppData, 0, nullptr, &path) == S_OK) {
        std::filesystem::path base(path);
        CoTaskMemFree(path);
        return base / "VoxelGame";
    }
    // Fallback if SHGetKnownFolderPath fails
    const char* appdata = std::getenv("APPDATA");
    if (appdata) {
        return std::filesystem::path(appdata) / "VoxelGame";
    }
#elif defined(__APPLE__)
    const char* home = std::getenv("HOME");
    if (home) {
        return std::filesystem::path(home) / "Library" / "Application Support" / "VoxelGame";
    }
#else // Linux/Unix
    const char* xdg_data = std::getenv("XDG_DATA_HOME");
    if (xdg_data) {
        return std::filesystem::path(xdg_data) / "VoxelGame";
    }
    const char* home = std::getenv("HOME");
    if (home) {
        return std::filesystem::path(home) / ".local" / "share" / "VoxelGame";
    }
#endif
    // Extreme fallback
    return std::filesystem::current_path() / "world_save";
}

} // namespace voxel
