#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace voxel {

struct PackManifest {
    std::string              name;
    std::string              version;
    std::string              description;
    std::string              gameVersion;
    std::vector<std::string> dependencies;
};

// A single pack — either a folder or a zip file.
// The pack id is its folder name or zip filename (without extension).
class Pack {
public:
    explicit Pack(std::filesystem::path path);

    const std::string&           id()       const { return id_; }
    const PackManifest&          manifest() const { return manifest_; }
    const std::filesystem::path& path()     const { return path_; }
    bool                         isZip()    const { return isZip_; }

    // Returns file contents, nullopt if not found.
    std::optional<std::string> readFile(const std::string& relativePath) const;
    bool                       hasFile(const std::string& relativePath)  const;

    // Relative paths of all files under subdir (e.g. "scripts/server").
    std::vector<std::string> listFiles(const std::string& subdir) const;

private:
    std::string           id_;
    PackManifest          manifest_;
    std::filesystem::path path_;
    bool                  isZip_ = false;

    void loadManifest();

    std::optional<std::string> readFileFromFolder(const std::string& relativePath) const;
    std::vector<std::string>   listFilesFromFolder(const std::string& subdir)      const;

    std::optional<std::string> readFileFromZip(const std::string& relativePath) const;
    std::vector<std::string>   listFilesFromZip(const std::string& subdir)      const;
};

}  // namespace voxel
