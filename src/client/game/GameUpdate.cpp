#include "game/Game.hpp"

#include <algorithm>
#include <chrono>
#include <iostream>
#include <unordered_set>
#include "common/Entity.hpp"

#include "GameInternal.hpp"

namespace voxel {
namespace {

constexpr int kMaxTerrainJobs = 4;
constexpr int kMaxMeshJobs = 8;
constexpr int kMaxTerrainIntegrationsPerFrame = 2;
constexpr int kMaxMeshUploadsPerFrame = 1;
constexpr int kMaxTerrainQueuesPerFrame = 2;

bool isWithinUnloadDistance(const ChunkCoord& coord, const ChunkCoord& playerChunk) {
    const int unloadDist = kViewDistance + 2;
    return std::abs(coord.x - playerChunk.x) <= unloadDist &&
           std::abs(coord.z - playerChunk.z) <= unloadDist;
}

bool receivesAuthoritativeChunks(const NetworkManager* network) {
    return network != nullptr && network->mode() == NetworkManager::Mode::Client;
}

}  // namespace

void Game::update(GLFWwindow* window, const float deltaTime) {
    if (network_ != nullptr) {
        network_->poll();
        applyNetworkBlockChanges();
        applyNetworkEntityChanges();
    }

    gameTimeSeconds_ += deltaTime;
    frameTimeMs_ = deltaTime * 1000.0f;
    fpsAccumulator_ += deltaTime;
    ++fpsFrameCount_;
    if (fpsAccumulator_ >= 0.5f) {
        fps_ = static_cast<int>(static_cast<float>(fpsFrameCount_) / fpsAccumulator_);
        fpsAccumulator_ = 0.0f;
        fpsFrameCount_ = 0;
    }

    const int px = static_cast<int>(std::floor(player_.position.x));
    const int py = std::clamp(static_cast<int>(std::floor(player_.position.y)), 0, kWorldY - 1);
    const int pz = static_cast<int>(std::floor(player_.position.z));
    const ChunkCoord playerChunk = worldToChunkCoord(px, py, pz);

    if (receivesAuthoritativeChunks(network_) &&
        (!hasRequestedNetworkChunks_ || !(lastRequestedNetworkChunk_ == playerChunk))) {
        network_->publishChunkInterest(playerChunk, kViewDistance);
        lastRequestedNetworkChunk_ = playerChunk;
        hasRequestedNetworkChunks_ = true;
    }

    collectPending(playerChunk);

    updateMouseLook(window, player_);
    updateInput(window, input_);
    handleInventorySelection(window);
    jump(player_, input_);
    updateMovement(window, simulation_.world(), gameData_, player_, deltaTime);
    if (!receivesAuthoritativeChunks(network_)) {
        simulateLiquids(deltaTime);
        processBlockTicks();
    }

    currentHit_ = raycastWorld(simulation_.world(), gameData_, getEyePosition(player_), getLookDirection(player_));
    handleBlockActions();

    if (network_ != nullptr) {
        network_->publishLocalPlayer(player_.name, player_.position, player_.yaw, player_.pitch);
    }

    const bool f5Now = glfwGetKey(window, GLFW_KEY_F5) == GLFW_PRESS;
    if (f5Now && !f5WasPressed_) {
        reloadGameData();
    }
    f5WasPressed_ = f5Now;

    updateLoadedChunks(playerChunk);
}

void Game::applyNetworkBlockChanges() {
    if (network_ == nullptr) {
        return;
    }

    for (NetworkChunkSnapshot& snapshot : network_->takePendingChunkSnapshots()) {
        const ChunkCoord coord = snapshot.chunk.coord;
        pendingTerrain_.erase(coord);
        queuedMeshBuilds_.erase(
            std::remove(queuedMeshBuilds_.begin(), queuedMeshBuilds_.end(), coord),
            queuedMeshBuilds_.end()
        );

        if (const auto it = meshes_.find(coord); it != meshes_.end()) {
            destroyChunkMesh(it->second);
            meshes_.erase(it);
        }

        simulation_.world().chunks[coord] = std::move(snapshot.chunk);
        launchMeshBuild(coord);

        for (const ChunkCoord neighbor : {
                 ChunkCoord{coord.x - 1, coord.y, coord.z},
                 ChunkCoord{coord.x + 1, coord.y, coord.z},
                 ChunkCoord{coord.x, coord.y - 1, coord.z},
                 ChunkCoord{coord.x, coord.y + 1, coord.z},
                 ChunkCoord{coord.x, coord.y, coord.z - 1},
                 ChunkCoord{coord.x, coord.y, coord.z + 1}}) {
            if (!chunkLoaded(simulation_.world(), neighbor)) {
                continue;
            }
            if (const auto it = meshes_.find(neighbor); it != meshes_.end()) {
                destroyChunkMesh(it->second);
                meshes_.erase(it);
            }
            launchMeshBuild(neighbor);
        }
    }

    for (const NetworkDeltaChunkSnapshot& delta : network_->takePendingDeltaChunkSnapshots()) {
        auto it = simulation_.world().chunks.find(delta.coord);
        if (it != simulation_.world().chunks.end()) {
            Chunk& chunk = it->second;
            for (const auto& change : delta.changes) {
                // index = (x << 8) | (y << 4) | z
                int x = (change.index >> 8) & 0xF;
                int y = (change.index >> 4) & 0xF;
                int z = change.index & 0xF;
                chunk.blocks[x][y][z] = change.stateId;
            }
            
            // Rebuild mesh
            if (const auto meshIt = meshes_.find(delta.coord); meshIt != meshes_.end()) {
                destroyChunkMesh(meshIt->second);
                meshes_.erase(meshIt);
            }
            launchMeshBuild(delta.coord);

            // Check neighbors for transparency changes? 
            // Simplified: just rebuild the chunk itself for now.
        }
    }

    if (network_->mode() == NetworkManager::Mode::Server) {
        for (const NetworkBlockChange& request : network_->takePendingBlockEditRequests()) {
            const BlockChangeResult result = simulation_.applyBlockChange(request.block, request.stateId);
            if (!result.applied || !result.changed) {
                continue;
            }
            if (request.stateId == 0) {
                blockTickGeneration_.erase(game_internal::blockTickKey(request.block));
            } else {
                scheduleBlockTick(request.block, request.stateId, 0.0f);
            }
            rebuildMeshesAroundBlock(request.block);
            network_->publishBlockChange(request.block, request.stateId);
        }
    }

    for (const NetworkBlockChange& change : network_->takePendingBlockChanges()) {
        const BlockChangeResult result = simulation_.applyBlockChange(change.block, change.stateId);
        if (!result.applied || !result.changed) {
            continue;
        }
        if (change.stateId == 0) {
            blockTickGeneration_.erase(game_internal::blockTickKey(change.block));
        } else {
            scheduleBlockTick(change.block, change.stateId, 0.0f);
        }
        rebuildMeshesAroundBlock(change.block);
    }

    // Client-side: Apply authoritative inventory updates from server
    for (const NetworkInventoryUpdate& update : network_->pendingInventoryUpdates()) {
        if (update.slotIndex < kInventorySlots) {
            auto& slot = player_.inventory.slots[update.slotIndex];
            slot.itemId = update.itemId;
            slot.count = update.count;
            std::cout << "[Client] Inventory update: slot " << update.slotIndex << " is now " << update.count << "x " << update.itemId << "\n";
        }
    }
    network_->clearPendingInventoryUpdates();
}

void Game::applyNetworkEntityChanges() {
    if (network_ == nullptr) return;

    for (const auto& spawn : network_->takePendingEntitySpawns()) {
        if (spawn.type == EntityType::Item) {
            auto item = std::make_unique<ItemEntity>(spawn.id, spawn.metadata);
            item->position = spawn.position;
            simulation_.spawnEntity(std::move(item));
        }
    }

    for (const auto& pos : network_->takePendingEntityPositions()) {
        if (Entity* e = simulation_.findEntity(pos.id)) {
            e->position = pos.position;
            e->yaw = pos.yaw;
            e->pitch = pos.pitch;
        }
    }

    for (const auto& id : network_->takePendingEntityDestroys()) {
        simulation_.removeEntity(id);
    }
}

void Game::collectPending(const ChunkCoord& playerChunk) {
    int terrainIntegrations = 0;
    for (auto it = pendingTerrain_.begin(); it != pendingTerrain_.end();) {
        if (it->second.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
            const ChunkCoord coord = it->first;
            const bool shouldKeep = isWithinUnloadDistance(coord, playerChunk);
            if (shouldKeep && terrainIntegrations >= kMaxTerrainIntegrationsPerFrame) {
                break;
            }
            Chunk chunk = it->second.get();
            it = pendingTerrain_.erase(it);
            if (shouldKeep) {
                simulation_.world().chunks[coord] = std::move(chunk);
                scheduleChunkBlockTicks(simulation_.world().chunks[coord]);
                launchMeshBuild(coord);
                ++terrainIntegrations;
            }
        } else {
            ++it;
        }
    }

    int meshUploads = 0;
    for (auto it = pendingMeshes_.begin(); it != pendingMeshes_.end();) {
        if (it->future.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
            const ChunkCoord coord = it->coord;
            const bool shouldUpload = chunkLoaded(simulation_.world(), coord) && isWithinUnloadDistance(coord, playerChunk);
            if (shouldUpload && meshUploads >= kMaxMeshUploadsPerFrame) {
                break;
            }
            ChunkMesh mesh = it->future.get();
            it = pendingMeshes_.erase(it);
            if (shouldUpload) {
                const auto existing = meshes_.find(coord);
                if (existing != meshes_.end()) {
                    destroyChunkMesh(mesh);
                    continue;
                }
                uploadChunkMesh(mesh);
                meshes_[coord] = std::move(mesh);
                ++meshUploads;
            }
        } else {
            ++it;
        }
    }

    for (auto it = queuedMeshBuilds_.begin(); it != queuedMeshBuilds_.end() && pendingMeshes_.size() < kMaxMeshJobs;) {
        const ChunkCoord coord = *it;
        it = queuedMeshBuilds_.erase(it);
        if (chunkLoaded(simulation_.world(), coord) && isWithinUnloadDistance(coord, playerChunk)) {
            launchMeshBuild(coord);
        }
    }
}

void Game::processBlockTicks() {
    constexpr int kMaxBlockTicksPerFrame = 64;
    int processed = 0;

    while (!blockTicks_.empty() && processed < kMaxBlockTicksPerFrame) {
        const ScheduledBlockTick tick = blockTicks_.top();
        if (tick.dueTime > gameTimeSeconds_) {
            break;
        }
        blockTicks_.pop();
        ++processed;

        const std::string key = game_internal::blockTickKey(tick.block);
        const auto generationIt = blockTickGeneration_.find(key);
        if (generationIt == blockTickGeneration_.end() || generationIt->second != tick.generation) {
            continue;
        }

        const std::uint16_t stateId = getBlock(simulation_.world(), tick.block.x, tick.block.y, tick.block.z);
        const BlockDefinition* def = findBlockDefinitionForBlockType(gameData_, stateId);
        if (stateId == 0 || def == nullptr || receivesAuthoritativeChunks(network_)) {
            blockTickGeneration_.erase(key);
            continue;
        }

        if (game_internal::isCropBlock(*def)) {
            const std::uint16_t soil = getBlock(simulation_.world(), tick.block.x, tick.block.y - 1, tick.block.z);
            const bool supported = game_internal::isCropSoil(gameData_, soil);
            const bool blockedAbove = getBlock(simulation_.world(), tick.block.x, tick.block.y + 1, tick.block.z) != 0;

            if (!supported || blockedAbove) {
                setBlock(simulation_.world(), tick.block.x, tick.block.y, tick.block.z, 0);
                for (const auto& drop : game_internal::cropDropsForState(gameData_, *def, stateId)) {
                    addItem(player_.inventory, gameData_, drop.item, drop.count);
                }
                rebuildMeshesAroundBlock(tick.block);
                blockTickGeneration_.erase(key);
                continue;
            }

            const int currentAge = game_internal::cropAge(gameData_, stateId);
            if (currentAge < game_internal::cropMaxAge(*def)) {
                std::uniform_real_distribution<float> chance(0.0f, 1.0f);
                if (chance(rng_) <= 0.18f) {
                    const auto nextId = runtimeIdForBlockState(gameData_, def->id, {{"age", currentAge + 1}});
                    if (nextId.has_value()) {
                        setBlock(simulation_.world(), tick.block.x, tick.block.y, tick.block.z, *nextId);
                        rebuildMeshesAroundBlock(tick.block);
                    }
                }
            }
        }

        const std::uint16_t nextStateId = getBlock(simulation_.world(), tick.block.x, tick.block.y, tick.block.z);
        scheduleBlockTick(tick.block, nextStateId, 0.0f);
    }
}

void Game::scheduleBlockTick(const Int3& block, const std::uint16_t stateId, const float delaySeconds) {
    if (stateId == 0) {
        blockTickGeneration_.erase(game_internal::blockTickKey(block));
        return;
    }

    const BlockDefinition* def = findBlockDefinitionForBlockType(gameData_, stateId);
    if (def == nullptr) {
        return;
    }

    const auto intervalSeconds = game_internal::tickIntervalSeconds(*def);
    if (!intervalSeconds.has_value()) {
        blockTickGeneration_.erase(game_internal::blockTickKey(block));
        return;
    }

    const std::string key = game_internal::blockTickKey(block);
    const std::uint32_t generation = ++blockTickGeneration_[key];
    const double dueTime = gameTimeSeconds_ + (delaySeconds > 0.0f ? delaySeconds : *intervalSeconds);
    blockTicks_.push({dueTime, block, generation});
}

void Game::scheduleChunkBlockTicks(const Chunk& chunk) {
    const int baseX = chunk.coord.x * kChunkX;
    const int baseY = chunk.coord.y * kChunkY;
    const int baseZ = chunk.coord.z * kChunkZ;

    for (int lx = 0; lx < kChunkX; ++lx) {
        for (int ly = 0; ly < kChunkY; ++ly) {
            for (int lz = 0; lz < kChunkZ; ++lz) {
                const std::uint16_t stateId = chunk.blocks[lx][ly][lz];
                if (stateId == 0) continue;
                scheduleBlockTick({baseX + lx, baseY + ly, baseZ + lz}, stateId, 0.0f);
            }
        }
    }
}

void Game::launchMeshBuild(const ChunkCoord& coord) {
    if (std::any_of(pendingMeshes_.begin(), pendingMeshes_.end(),
            [&coord](const PendingMesh& pm) { return pm.coord == coord; })) {
        return;
    }
    if (std::find(queuedMeshBuilds_.begin(), queuedMeshBuilds_.end(), coord) != queuedMeshBuilds_.end()) {
        return;
    }
    if (pendingMeshes_.size() >= kMaxMeshJobs) {
        queuedMeshBuilds_.push_back(coord);
        return;
    }

    std::array<const Chunk*, 27> snapshot;
    snapshot.fill(nullptr);
    for (int cx = 0; cx < 3; ++cx) {
        for (int cy = 0; cy < 3; ++cy) {
            for (int cz = 0; cz < 3; ++cz) {
                ChunkCoord target = {coord.x + cx - 1, coord.y + cy - 1, coord.z + cz - 1};
                const auto it = simulation_.world().chunks.find(target);
                if (it != simulation_.world().chunks.end()) {
                    snapshot[cx * 9 + cy * 3 + cz] = &it->second;
                }
            }
        }
    }

    pendingMeshes_.push_back({coord, std::async(std::launch::async,
        [coord, snapshot, &gd = gameData_, &mm = modelManager_]() {
            const Chunk* neighbors[27];
            for (int i = 0; i < 27; ++i) neighbors[i] = snapshot[i];
            return buildChunkMesh(neighbors, coord, gd, mm);
        }
    )});
}

void Game::simulateLiquids(const float deltaTime) {
    constexpr float kLiquidStepSeconds = 0.20f;
    liquidStepAccumulator_ += deltaTime;
    if (liquidStepAccumulator_ < kLiquidStepSeconds) {
        return;
    }
    liquidStepAccumulator_ = 0.0f;

    struct LiquidMove {
        Int3 from;
        Int3 to;
        std::uint16_t blockType;
    };

    std::vector<LiquidMove> moves;
    moves.reserve(64);

    for (const auto& [coord, chunk] : simulation_.world().chunks) {
        const int baseX = coord.x * kChunkX;
        const int baseY = coord.y * kChunkY;
        const int baseZ = coord.z * kChunkZ;
        const int yStart = (baseY == 0) ? 1 : 0;

        for (int lx = 0; lx < kChunkX; ++lx) {
            for (int ly = yStart; ly < kChunkY; ++ly) {
                for (int lz = 0; lz < kChunkZ; ++lz) {
                    const int wx = baseX + lx;
                    const int wy = baseY + ly;
                    const int wz = baseZ + lz;

                    const std::uint16_t blockType = chunk.blocks[lx][ly][lz];
                    if (blockType == 0 || !isLiquidBlockType(gameData_, blockType)) {
                        continue;
                    }
                    if (isOccupied(simulation_.world(), wx, wy - 1, wz)) {
                        continue;
                    }
                    moves.push_back({{wx, wy, wz}, {wx, wy - 1, wz}, blockType});
                }
            }
        }
    }

    std::unordered_set<ChunkCoord, ChunkCoordHash> dirtyChunks;
    const auto markDirty = [&](const Int3& block) {
        const ChunkCoord center = worldToChunkCoord(block.x, block.y, block.z);
        const Int3 local = worldToLocalBlock(block.x, block.y, block.z);
        dirtyChunks.insert(center);
        if (local.x == 0) dirtyChunks.insert({center.x - 1, center.y, center.z});
        if (local.x == kChunkX - 1) dirtyChunks.insert({center.x + 1, center.y, center.z});
        if (local.y == 0) dirtyChunks.insert({center.x, center.y - 1, center.z});
        if (local.y == kChunkY - 1) dirtyChunks.insert({center.x, center.y + 1, center.z});
        if (local.z == 0) dirtyChunks.insert({center.x, center.y, center.z - 1});
        if (local.z == kChunkZ - 1) dirtyChunks.insert({center.x, center.y, center.z + 1});
    };

    for (const auto& move : moves) {
        if (getBlock(simulation_.world(), move.from.x, move.from.y, move.from.z) != move.blockType) {
            continue;
        }
        if (isOccupied(simulation_.world(), move.to.x, move.to.y, move.to.z)) {
            continue;
        }

        setBlock(simulation_.world(), move.from.x, move.from.y, move.from.z, 0);
        setBlock(simulation_.world(), move.to.x, move.to.y, move.to.z, move.blockType);
        markDirty(move.from);
        markDirty(move.to);
    }

    for (const auto& coord : dirtyChunks) {
        rebuildChunkMesh(coord);
    }
}

void Game::updateLoadedChunks(const ChunkCoord& playerChunk) {
    if (!receivesAuthoritativeChunks(network_)) {
        int queued = 0;
        const float maxDistSq = static_cast<float>(kViewDistance * kViewDistance);

        for (int dz = -kViewDistance; dz <= kViewDistance && queued < kMaxTerrainQueuesPerFrame; ++dz) {
            for (int dx = -kViewDistance; dx <= kViewDistance && queued < kMaxTerrainQueuesPerFrame; ++dx) {
                for (int cy = 0; cy < kChunkCountY && queued < kMaxTerrainQueuesPerFrame; ++cy) {
                    const int dy = cy - playerChunk.y;
                    if (dx * dx + dy * dy + dz * dz > maxDistSq) continue;

                    if (pendingTerrain_.size() >= kMaxTerrainJobs) {
                        break;
                    }
                    const ChunkCoord coord {playerChunk.x + dx, cy, playerChunk.z + dz};
                    if (chunkLoaded(simulation_.world(), coord)) continue;
                    if (pendingTerrain_.find(coord) != pendingTerrain_.end()) continue;

                    const TerrainGenerator gen = simulation_.terrainGenerator();
                    pendingTerrain_[coord] = std::async(std::launch::async,
                        [coord, gen, &gd = gameData_]() {
                            return gen.generateChunk(coord, gd);
                        }
                    );
                    ++queued;
                }
            }
        }
    }

    if (network_ != nullptr && network_->isConnected()) {
        const int dist = std::max(std::abs(playerChunk.x - lastRequestedNetworkChunk_.x),
                                 std::abs(playerChunk.z - lastRequestedNetworkChunk_.z));
        if (!hasRequestedNetworkChunks_ || dist > kViewDistance / 2) {
            network_->publishChunkInterest(playerChunk, kViewDistance);
            lastRequestedNetworkChunk_ = playerChunk;
            hasRequestedNetworkChunks_ = true;
        }
    }

    const int unloadDist = kViewDistance + 2;
    const float unloadDistSq = static_cast<float>(unloadDist * unloadDist);

    std::vector<ChunkCoord> toUnload;
    for (const auto& [coord, chunk] : simulation_.world().chunks) {
        const int dx = coord.x - playerChunk.x;
        const int dy = coord.y - playerChunk.y;
        const int dz = coord.z - playerChunk.z;

        if (dx * dx + dy * dy + dz * dz > unloadDistSq) {
            toUnload.push_back(coord);
        }
    }
    for (const auto& coord : toUnload) {
        queuedMeshBuilds_.erase(
            std::remove(queuedMeshBuilds_.begin(), queuedMeshBuilds_.end(), coord),
            queuedMeshBuilds_.end()
        );
        simulation_.world().chunks.erase(coord);
        const auto it = meshes_.find(coord);
        if (it != meshes_.end()) {
            destroyChunkMesh(it->second);
            meshes_.erase(it);
        }
    }
}

}  // namespace voxel
