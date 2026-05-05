#pragma once

#include <string>
#include <unordered_map>

namespace voxel {
struct TextureResource {
    unsigned int glId = 0;
    int width = 0;
    int height = 0;
};

class TextureManager {
public:
    TextureManager() = default;
    ~TextureManager();

    TextureManager(const TextureManager&) = delete;
    TextureManager& operator=(const TextureManager&) = delete;

    bool loadTexture(const std::string& relativePath, const std::string& assetsRoot);
    const TextureResource* find(const std::string& relativePath) const;
    const TextureResource& blackFallback() const;

private:
    TextureResource createTexture(int width, int height, int channelCount, const unsigned char* pixels) const;

    std::unordered_map<std::string, TextureResource> textures_;
    mutable TextureResource blackFallback_ {};
};
}  // namespace voxel
