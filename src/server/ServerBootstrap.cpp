#include "server/ServerBootstrap.hpp"

#include <stdexcept>

#include "common/pack/PackManager.hpp"
#include "common/pack/ScriptManager.hpp"

namespace voxel {

std::filesystem::path findProjectRoot() {
    std::filesystem::path current = std::filesystem::current_path();

    while (!current.empty()) {
        if (std::filesystem::exists(current / "packs")) {
            return current;
        }

        const auto parent = current.parent_path();
        if (parent == current) {
            break;
        }
        current = parent;
    }

    throw std::runtime_error("Could not find project root containing packs/");
}

ServerContext loadServerContext(const std::filesystem::path& projectRoot) {
    PackManager packManager;
    packManager.discover(projectRoot / "packs");

    if (packManager.findPack("base") == nullptr) {
        throw std::runtime_error("Required 'base' pack not found in packs/");
    }

    ScriptManager scriptManager;
    scriptManager.setHostKind(ScriptHost::Server);
    return {
        scriptManager.loadGameData(packManager, projectRoot / "engine" / "scripts"),
        projectRoot
    };
}

}  // namespace voxel
