#include "render/ModelManager.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>

#include "data/JsonValue.hpp"

namespace voxel {
namespace {

std::string readFile(const std::filesystem::path& path) {
    std::ifstream stream(path);
    if (!stream) return {};
    std::ostringstream buf;
    buf << stream.rdbuf();
    return buf.str();
}

// Follow "#key" chains in a raw texture map to a final resolved path.
std::string resolveTexture(
    const std::unordered_map<std::string, std::string>& raw,
    const std::string& ref
) {
    std::string current = ref;
    for (int i = 0; i < 16; ++i) {
        if (current.empty() || current[0] != '#') return current;
        const auto it = raw.find(current.substr(1));
        if (it == raw.end()) return current;  // unresolvable
        current = it->second;
    }
    return current;
}

// Re-resolve every face's texture using the given merged raw texture map.
void resolveElementFaces(
    std::vector<ModelElement>& elements,
    const std::unordered_map<std::string, std::string>& raw
) {
    for (auto& element : elements) {
        for (auto& [name, face] : element.faces) {
            face.texture = resolveTexture(raw, face.rawRef);
        }
    }
}

// Parse elements from a JSON array. Stores the raw "#key" ref in face.rawRef
// and leaves face.texture empty — caller resolves after texture map is merged.
std::vector<ModelElement> parseElements(const JsonValue& array) {
    std::vector<ModelElement> elements;
    for (const auto& elemVal : array.asArray()) {
        if (!elemVal.isObject()) continue;
        const auto& obj = elemVal.asObject();
        ModelElement element;

        const auto fromIt = obj.find("from");
        if (fromIt != obj.end() && fromIt->second.isArray()) {
            const auto& a = fromIt->second.asArray();
            if (a.size() == 3) element.from = {
                static_cast<float>(a[0].asNumber()),
                static_cast<float>(a[1].asNumber()),
                static_cast<float>(a[2].asNumber())
            };
        }
        const auto toIt = obj.find("to");
        if (toIt != obj.end() && toIt->second.isArray()) {
            const auto& a = toIt->second.asArray();
            if (a.size() == 3) element.to = {
                static_cast<float>(a[0].asNumber()),
                static_cast<float>(a[1].asNumber()),
                static_cast<float>(a[2].asNumber())
            };
        }

        const auto rotIt = obj.find("rotation");
        if (rotIt != obj.end() && rotIt->second.isObject()) {
            const auto& ro = rotIt->second.asObject();
            ModelElementRotation rot;
            const auto originIt = ro.find("origin");
            if (originIt != ro.end() && originIt->second.isArray()) {
                const auto& a = originIt->second.asArray();
                if (a.size() == 3) rot.origin = {
                    static_cast<float>(a[0].asNumber()),
                    static_cast<float>(a[1].asNumber()),
                    static_cast<float>(a[2].asNumber())
                };
            }
            const auto axisIt = ro.find("axis");
            if (axisIt != ro.end() && axisIt->second.isString()) {
                const std::string& ax = axisIt->second.asString();
                if      (ax == "x" || ax == "X") rot.axis = 0;
                else if (ax == "y" || ax == "Y") rot.axis = 1;
                else                              rot.axis = 2;
            }
            const auto angleIt = ro.find("angle");
            if (angleIt != ro.end() && angleIt->second.isNumber()) {
                rot.angle = static_cast<float>(angleIt->second.asNumber());
            }
            const auto rescaleIt = ro.find("rescale");
            if (rescaleIt != ro.end() && rescaleIt->second.isBool()) {
                rot.rescale = rescaleIt->second.asBool();
            }
            element.rotation = rot;
        }

        const auto facesIt = obj.find("faces");
        if (facesIt != obj.end() && facesIt->second.isObject()) {
            for (const auto& [faceName, faceVal] : facesIt->second.asObject()) {
                if (!faceVal.isObject()) continue;
                const auto& fo = faceVal.asObject();
                ModelFace face;
                const auto uvIt = fo.find("uv");
                if (uvIt != fo.end() && uvIt->second.isArray()) {
                    const auto& a = uvIt->second.asArray();
                    if (a.size() == 4) face.uv = {
                        static_cast<float>(a[0].asNumber()),
                        static_cast<float>(a[1].asNumber()),
                        static_cast<float>(a[2].asNumber()),
                        static_cast<float>(a[3].asNumber())
                    };
                }
                const auto texIt = fo.find("texture");
                if (texIt != fo.end() && texIt->second.isString()) {
                    face.rawRef = texIt->second.asString();
                    // texture (resolved) left empty — filled after merge
                }
                element.faces[faceName] = std::move(face);
            }
        }

        elements.push_back(std::move(element));
    }
    return elements;
}

}  // namespace

const BlockModel* ModelManager::load(const std::string& relativePath,
                                     const std::string& assetsRoot) {
    return loadInternal(relativePath, assetsRoot, 0);
}

const BlockModel* ModelManager::loadInternal(const std::string& relativePath,
                                              const std::string& assetsRoot,
                                              const int depth) {
    if (models_.contains(relativePath)) {
        return &models_.at(relativePath);
    }
    if (depth > 8) {
        std::cerr << "ModelManager: parent chain too deep at: " << relativePath << '\n';
        return nullptr;
    }

    const std::filesystem::path fullPath = std::filesystem::path(assetsRoot) / relativePath;
    const std::string text = readFile(fullPath);
    if (text.empty()) {
        std::cerr << "ModelManager: failed to load model: " << fullPath << '\n';
        return nullptr;
    }

    try {
        const JsonValue root = parseJson(text);
        const auto& rootObj = root.asObject();

        BlockModel model;
        model.id = relativePath;

        // 1. Load parent first — inherit its elements and raw texture map
        const auto parentIt = rootObj.find("parent");
        if (parentIt != rootObj.end() && parentIt->second.isString()) {
            const BlockModel* parent = loadInternal(
                parentIt->second.asString(), assetsRoot, depth + 1);
            if (parent) {
                model.elements    = parent->elements;    // inherit geometry
                model.rawTextures = parent->rawTextures; // inherit texture vars as base
            }
        }

        // 2. Collect this file's texture variable declarations and merge on top
        const auto texIt = rootObj.find("textures");
        if (texIt != rootObj.end() && texIt->second.isObject()) {
            for (const auto& [key, val] : texIt->second.asObject()) {
                if (val.isString()) {
                    model.rawTextures[key] = val.asString();  // child overrides parent
                }
            }
        }

        // 3. If this file has its own elements, they replace the parent's
        const auto elemIt = rootObj.find("elements");
        if (elemIt != rootObj.end() && elemIt->second.isArray()) {
            model.elements = parseElements(elemIt->second);
        }

        // 4. Resolve all face texture refs through the final merged raw map
        resolveElementFaces(model.elements, model.rawTextures);

        // 5. Build the resolved public texture map (for Game::populateFaceTextures)
        for (const auto& [key, raw] : model.rawTextures) {
            const std::string resolved = resolveTexture(model.rawTextures, raw);
            if (!resolved.empty() && resolved[0] != '#') {
                model.textures[key] = resolved;
            }
        }

        const auto [it, _] = models_.emplace(relativePath, std::move(model));
        return &it->second;

    } catch (const std::exception& e) {
        std::cerr << "ModelManager: parse error in " << relativePath
                  << ": " << e.what() << '\n';
        return nullptr;
    }
}

const BlockModel* ModelManager::get(const std::string& relativePath) const {
    const auto it = models_.find(relativePath);
    return it != models_.end() ? &it->second : nullptr;
}

}  // namespace voxel
