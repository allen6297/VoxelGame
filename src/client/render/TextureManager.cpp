#include "render/TextureManager.hpp"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include <fstream>
#include <iostream>
#include <vector>

#include "render/RenderBackend.hpp"

namespace voxel {
namespace {
bool loadPpm(const std::string& path, int& width, int& height, std::vector<unsigned char>& pixels) {
    std::ifstream file(path);
    if (!file) {
        return false;
    }

    std::string magic;
    file >> magic;
    if (magic != "P3") {
        return false;
    }

    file >> width >> height;
    int maxValue = 0;
    file >> maxValue;
    if (width <= 0 || height <= 0 || maxValue <= 0) {
        return false;
    }

    pixels.resize(static_cast<std::size_t>(width * height * 3));
    for (int i = 0; i < width * height * 3; ++i) {
        int value = 0;
        file >> value;
        pixels[static_cast<std::size_t>(i)] = static_cast<unsigned char>((255 * value) / maxValue);
    }

    return true;
}

bool loadImage(const std::string& path, int& width, int& height, int& channelCount, std::vector<unsigned char>& pixels) {
    if (loadPpm(path, width, height, pixels)) {
        channelCount = 3;
        return true;
    }

    int decodedChannels = 0;
    unsigned char* decoded = stbi_load(path.c_str(), &width, &height, &decodedChannels, 4);
    if (decoded == nullptr) {
        return false;
    }

    channelCount = 4;
    pixels.assign(decoded, decoded + static_cast<std::size_t>(width * height * channelCount));
    stbi_image_free(decoded);
    return true;
}
}  // namespace

TextureManager::TextureManager(const IRenderBackend& renderer)
    : renderer_(renderer) {
}

TextureManager::~TextureManager() {
    renderer_.destroyTexture(blackFallback_.handle);
    for (auto& [path, texture] : textures_) {
        renderer_.destroyTexture(texture.handle);
    }
}

TextureResource TextureManager::createTexture(
    const int width,
    const int height,
    const int channelCount,
    const unsigned char* pixels
) const {
    TextureResource resource;
    resource.width = width;
    resource.height = height;
    resource.handle = renderer_.createTexture2D(width, height, channelCount, pixels);
    return resource;
}

bool TextureManager::loadTexture(const std::string& relativePath, const std::string& assetsRoot) {
    if (textures_.contains(relativePath)) {
        return true;
    }

    int width = 0;
    int height = 0;
    int channelCount = 0;
    std::vector<unsigned char> pixels;
    if (!loadImage(assetsRoot + "/" + relativePath, width, height, channelCount, pixels)) {
        std::cerr << "Failed to load texture: " << assetsRoot + "/" + relativePath << '\n';
        return false;
    }

    TextureResource resource = createTexture(width, height, channelCount, pixels.data());

    textures_.emplace(relativePath, resource);
    std::cout << "Loaded texture: " << relativePath << " (" << width << "x" << height << ")\n";
    return true;
}

const TextureResource* TextureManager::find(const std::string& relativePath) const {
    const auto iterator = textures_.find(relativePath);
    if (iterator == textures_.end()) {
        return nullptr;
    }
    return &iterator->second;
}

const TextureResource& TextureManager::blackFallback() const {
    if (blackFallback_.handle.id == 0) {
        static constexpr unsigned char kBlackPixel[4] = {0, 0, 0, 255};
        blackFallback_ = createTexture(1, 1, 4, kBlackPixel);
    }
    return blackFallback_;
}
}  // namespace voxel
