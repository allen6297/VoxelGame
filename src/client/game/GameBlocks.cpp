#include "game/Game.hpp"

#include <algorithm>

#include "GameInternal.hpp"

namespace voxel {

void Game::handleBlockActions() {
    if (!currentHit_.has_value()) {
        return;
    }

    const BlockDefinition* hitDef = findBlockDefinitionForBlockType(gameData_, currentHit_->type);
    if (input_.breakPressed && (hitDef == nullptr || hitDef->material != "liquid")) {
        if (network_ != nullptr && network_->mode() == NetworkManager::Mode::Client) {
            network_->publishBlockChange(currentHit_->block, 0);
            return;
        }

        const BlockDefinition* block = findBlockDefinitionForBlockType(gameData_, currentHit_->type);
        const BlockChangeResult result = simulation_.applyBlockChange(currentHit_->block, 0);
        if (!result.applied || !result.changed) {
            return;
        }
        blockTickGeneration_.erase(game_internal::blockTickKey(currentHit_->block));
        if (block != nullptr) {
            if (network_ == nullptr || network_->mode() != NetworkManager::Mode::Client) {
                // In singleplayer or host, spawn items
                for (const auto& drop : game_internal::cropDropsForState(gameData_, *block, currentHit_->type)) {
                    // We can either add directly or spawn entities.
                    // Let's spawn entities even in singleplayer for consistency, but for now
                    // let's keep direct add in singleplayer to avoid breaking things if pick-up isn't ready.
                    addItem(player_.inventory, gameData_, drop.item, drop.count);
                }
                
                // Also handle normal block drops if not a crop
                if (game_internal::cropDropsForState(gameData_, *block, currentHit_->type).empty()) {
                    for (const auto& drop : block->drops) {
                        addItem(player_.inventory, gameData_, drop.item, drop.count);
                    }
                }
            }
        }
        rebuildMeshesAroundBlock(currentHit_->block);
        if (network_ != nullptr) {
            network_->publishBlockChange(currentHit_->block, 0);
        }
    }

    if (!input_.placePressed) {
        return;
    }

    const Int3 target = currentHit_->previousEmpty;
    if (!isYInBounds(target.y)) return;
    if (isOccupied(simulation_.world(), target.x, target.y, target.z)) return;
    if (isObstructedByModel(simulation_.world(), gameData_, target)) return;

    const auto selectedItem = selectedItemId(player_.inventory);
    if (!selectedItem.has_value()) {
        return;
    }

    if (*selectedItem == game_internal::kWheatSeedsItemId) {
        const std::uint16_t soil = getBlock(simulation_.world(), target.x, target.y - 1, target.z);
        if (!game_internal::isCropSoil(gameData_, soil) ||
            getBlock(simulation_.world(), target.x, target.y + 1, target.z) != 0) {
            return;
        }

        const auto cropType = runtimeIdForBlockState(gameData_, game_internal::kWheatBlockId, {{"age", 0}});
        if (!cropType.has_value()) return;
        if (network_ != nullptr && network_->mode() == NetworkManager::Mode::Client) {
            network_->publishBlockChange(target, *cropType);
            return;
        }
        if (!removeSelectedItem(player_.inventory, 1)) return;

        const BlockChangeResult result = simulation_.applyBlockChange(target, *cropType);
        if (!result.applied || !result.changed) {
            return;
        }
        scheduleBlockTick(target, *cropType, 0.0f);
        rebuildMeshesAroundBlock(target);
        if (network_ != nullptr) {
            network_->publishBlockChange(target, *cropType);
        }
        return;
    }

    const auto blockType = blockTypeForItemId(gameData_, *selectedItem);
    if (!blockType.has_value()) {
        return;
    }

    setBlock(simulation_.world(), target.x, target.y, target.z, *blockType);
    const bool wouldCollide = playerCollidesAt(simulation_.world(), gameData_, player_.position);
    setBlock(simulation_.world(), target.x, target.y, target.z, 0);
    if (wouldCollide) return;
    if (network_ != nullptr && network_->mode() == NetworkManager::Mode::Client) {
        network_->publishBlockChange(target, *blockType);
        return;
    }
    if (!removeSelectedItem(player_.inventory, 1)) return;

    const BlockChangeResult result = simulation_.applyBlockChange(target, *blockType);
    if (!result.applied || !result.changed) {
        return;
    }
    scheduleBlockTick(target, *blockType, 0.0f);
    rebuildMeshesAroundBlock(target);
    if (network_ != nullptr) {
        network_->publishBlockChange(target, *blockType);
    }
}

void Game::handleInventorySelection(const ClientInputFrame& input) {
    for (int i = 0; i < kInventorySlots; ++i) {
        const bool down = input.cursorCaptured && input.slotDown[static_cast<std::size_t>(i)];
        if (down && !input_.slotHeld[static_cast<std::size_t>(i)]) {
            selectSlot(player_.inventory, i);
            if (network_ != nullptr && network_->mode() == NetworkManager::Mode::Client) {
                network_->publishSelectSlot(i);
            }
        }
        input_.slotHeld[static_cast<std::size_t>(i)] = down;
    }
}

void Game::rebuildChunkMesh(const ChunkCoord& coord) {
    ScopedEngineTimer meshBuildTimer(diagnostics_, EnginePhase::MeshBuild);

    if (!inChunkBounds(coord) || !chunkLoaded(simulation_.world(), coord)) {
        return;
    }

    queuedMeshBuilds_.erase(
        std::remove(queuedMeshBuilds_.begin(), queuedMeshBuilds_.end(), coord),
        queuedMeshBuilds_.end()
    );
    discardPendingMeshUpload(coord);

    const auto it = meshes_.find(coord);
    if (it != meshes_.end()) {
        renderBackend_.destroyChunkMesh(it->second);
    }

    std::array<const Chunk*, 27> snapshot;
    snapshot.fill(nullptr);
    for (int cx = 0; cx < 3; ++cx) {
        for (int cy = 0; cy < 3; ++cy) {
            for (int cz = 0; cz < 3; ++cz) {
                ChunkCoord target = {coord.x + cx - 1, coord.y + cy - 1, coord.z + cz - 1};
                const auto chunkIt = simulation_.world().chunks.find(target);
                if (chunkIt != simulation_.world().chunks.end()) {
                    snapshot[cx * 9 + cy * 3 + cz] = &chunkIt->second;
                }
            }
        }
    }

    const Chunk* neighbors[27];
    for (int i = 0; i < 27; ++i) neighbors[i] = snapshot[i];

    ChunkMesh newMesh = buildChunkMesh(neighbors, coord, gameData_, modelManager_);
    renderBackend_.uploadChunkMesh(newMesh);
    meshes_[coord] = std::move(newMesh);
}

void Game::rebuildMeshesAroundBlock(const Int3& block) {
    const ChunkCoord center = worldToChunkCoord(block.x, block.y, block.z);
    rebuildChunkMesh(center);

    const Int3 local = worldToLocalBlock(block.x, block.y, block.z);
    if (local.x == 0) rebuildChunkMesh({center.x - 1, center.y, center.z});
    if (local.x == kChunkX - 1) rebuildChunkMesh({center.x + 1, center.y, center.z});
    if (local.y == 0) rebuildChunkMesh({center.x, center.y - 1, center.z});
    if (local.y == kChunkY - 1) rebuildChunkMesh({center.x, center.y + 1, center.z});
    if (local.z == 0) rebuildChunkMesh({center.x, center.y, center.z - 1});
    if (local.z == kChunkZ - 1) rebuildChunkMesh({center.x, center.y, center.z + 1});
}

}  // namespace voxel
