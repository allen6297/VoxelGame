#include "game/Game.hpp"

#include <algorithm>

#include "GameInternal.hpp"

namespace voxel {
namespace {

void populateModelDerivedData(
    const BlockModel& model,
    const std::string& assetsRoot,
    TextureManager& textureManager,
    BlockDefinition& block,
    std::unordered_map<std::string, std::string>& faceTextures,
    std::vector<CollisionBox>& collisionBoxes,
    int& collisionSearchExpansion
) {
    static constexpr std::array<const char*, 6> kFaces {
        "up", "down", "north", "south", "east", "west"
    };

    faceTextures.clear();
    collisionBoxes.clear();

    const auto setFace = [&](const std::string& face, const std::string& path) {
        faceTextures[face] = path;
        textureManager.loadTexture(path, assetsRoot);
        if (!block.textures.albedo.has_value()) {
            block.textures.albedo = path;
        }
    };

    for (const auto& element : model.elements) {
        collisionBoxes.push_back({
            element.from[0] / 16.0f, element.from[1] / 16.0f, element.from[2] / 16.0f,
            element.to[0]   / 16.0f, element.to[1]   / 16.0f, element.to[2]   / 16.0f
        });
    }

    for (const auto& box : collisionBoxes) {
        const float maxExtent = std::max({box.maxX, box.maxY, box.maxZ});
        const int needed = std::max(1, static_cast<int>(std::ceil(maxExtent - 1.0f)));
        if (needed > collisionSearchExpansion) {
            collisionSearchExpansion = needed;
        }
    }

    for (const auto& [key, path] : model.textures) {
        if (key == "all") {
            for (const char* f : kFaces) setFace(f, path);
        } else if (key == "top" || key == "up" || key == "end") {
            setFace("up", path);
            setFace("down", path);
        } else if (key == "bottom" || key == "down") {
            setFace("down", path);
        } else if (key == "side") {
            setFace("north", path);
            setFace("south", path);
            setFace("east", path);
            setFace("west", path);
        } else {
            for (const char* f : kFaces) {
                if (key == f) {
                    setFace(key, path);
                    break;
                }
            }
        }
    }
}

}  // namespace

void Game::populateFaceTextures() {
    for (auto& [blockId, block] : gameData_.blocks) {
        if (block.modelPath.empty()) continue;
        const BlockModel* model = modelManager_.get(block.modelPath);
        if (!model || model->textures.empty()) continue;
        populateModelDerivedData(
            *model, assetsRoot_, textureManager_, block,
            block.faceTextures, block.collisionBoxes, gameData_.collisionSearchExpansion
        );
    }

    for (const auto& [stateId, modelPath] : gameData_.stateModelPathById) {
        const std::string* blockId = nullptr;
        if (const auto it = gameData_.blockIdByStateId.find(stateId); it != gameData_.blockIdByStateId.end()) {
            blockId = &it->second;
        }
        if (blockId == nullptr) {
            continue;
        }
        auto blockIt = gameData_.blocks.find(*blockId);
        if (blockIt == gameData_.blocks.end()) {
            continue;
        }
        const BlockModel* model = modelManager_.get(modelPath);
        if (model == nullptr || model->textures.empty()) {
            continue;
        }

        populateModelDerivedData(
            *model, assetsRoot_, textureManager_, blockIt->second,
            gameData_.stateFaceTexturesById[stateId],
            gameData_.stateCollisionBoxesById[stateId],
            gameData_.collisionSearchExpansion
        );
    }
}

Game::Game(GameData gameData, std::string assetsRoot, std::string playerName, NetworkManager* network)
    : assetsRoot_(std::move(assetsRoot)), gameData_(std::move(gameData)), network_(network) {
    player_.name = std::move(playerName);
    if (network_ != nullptr && network_->mode() == NetworkManager::Mode::Server) {
        network_->setExternalBlockAuthority(true);
    }

    for (const auto& [blockId, block] : gameData_.blocks) {
        if (!block.modelPath.empty()) {
            modelManager_.load(block.modelPath, assetsRoot_);
        }
    }
    for (const auto& [stateId, modelPath] : gameData_.stateModelPathById) {
        modelManager_.load(modelPath, assetsRoot_);
    }
    for (const auto& [blockId, block] : gameData_.blocks) {
        for (const auto& texturePath : {
                 texturePathForType(block, "albedo"),
                 texturePathForType(block, "normal"),
                 texturePathForType(block, "roughness"),
                 texturePathForType(block, "emissive")
             }) {
            if (texturePath.has_value()) {
                textureManager_.loadTexture(*texturePath, assetsRoot_);
            }
        }
    }

    populateFaceTextures();
    addItem(player_.inventory, gameData_, "stone", 32);
    addItem(player_.inventory, gameData_, game_internal::kWheatSeedsItemId, 16);
}

Game::~Game() {
    pendingTerrain_.clear();
    pendingMeshes_.clear();
    queuedMeshBuilds_.clear();
    for (auto& [coord, mesh] : meshes_) destroyChunkMesh(mesh);
    for (auto& [id, mesh] : iconMeshes_) destroyChunkMesh(mesh);
}

void Game::reloadContent() {
    reloadGameData();
}

void Game::reloadGameData() {
    pendingTerrain_.clear();
    pendingMeshes_.clear();
    queuedMeshBuilds_.clear();
    blockTicks_ = {};
    blockTickGeneration_.clear();
    for (auto& [id, mesh] : iconMeshes_) destroyChunkMesh(mesh);
    iconMeshes_.clear();

    modelManager_ = ModelManager{};
    for (const auto& [blockId, block] : gameData_.blocks) {
        if (!block.modelPath.empty()) {
            modelManager_.load(block.modelPath, assetsRoot_);
        }
    }
    for (const auto& [stateId, modelPath] : gameData_.stateModelPathById) {
        modelManager_.load(modelPath, assetsRoot_);
    }
    populateFaceTextures();

    std::vector<ChunkCoord> coordsToRebuild;
    coordsToRebuild.reserve(meshes_.size());
    for (auto& [coord, mesh] : meshes_) {
        coordsToRebuild.push_back(coord);
        destroyChunkMesh(mesh);
    }
    meshes_.clear();
    for (const ChunkCoord& coord : coordsToRebuild) {
        launchMeshBuild(coord);
    }
}

}  // namespace voxel
