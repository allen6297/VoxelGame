#include "server/HeadlessServer.hpp"
#include "common/OSUtils.hpp"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <utility>
#include <random>

#include "common/world/Chunk.hpp"
#include "common/world/World.hpp"
#include "common/Entity.hpp"

namespace voxel {
namespace {

constexpr int kServerChunkRadius = 2;
constexpr auto kChunkMaintenanceInterval = std::chrono::milliseconds(250);
constexpr float kServerPlayerRadius = 0.32f;
constexpr float kServerPlayerHeight = 1.8f;

ChunkCoord chunkForPosition(const Vec3& position) {
    const int x = static_cast<int>(std::floor(position.x));
    const int y = std::clamp(static_cast<int>(std::floor(position.y)), 0, kWorldY - 1);
    const int z = static_cast<int>(std::floor(position.z));
    return worldToChunkCoord(x, y, z);
}

bool boolProperty(const BlockDefinition& def, const std::string& key, const bool fallback = false) {
    const auto it = def.properties.find(key);
    if (it == def.properties.end() || !std::holds_alternative<bool>(it->second)) {
        return fallback;
    }
    return std::get<bool>(it->second);
}

bool isCropBlock(const BlockDefinition& def) {
    return boolProperty(def, "crop");
}

bool isCropSoil(const GameData& gameData, const std::uint16_t stateId) {
    const BlockDefinition* def = findBlockDefinitionForBlockType(gameData, stateId);
    return def != nullptr && (def->id == "base:dirt" || def->id == "base:grass" ||
                              def->id == "dirt" || def->id == "grass");
}

bool playerCollidesAt_Headless(
    const World& world,
    const GameData& gameData,
    const Vec3& position
) {
    const float minX = position.x - kServerPlayerRadius;
    const float maxX = position.x + kServerPlayerRadius;
    const float minY = position.y;
    const float maxY = position.y + kServerPlayerHeight;
    const float minZ = position.z - kServerPlayerRadius;
    const float maxZ = position.z + kServerPlayerRadius;

    const int x0 = static_cast<int>(std::floor(minX));
    const int x1 = static_cast<int>(std::floor(maxX));
    const int y0 = static_cast<int>(std::floor(minY));
    const int y1 = static_cast<int>(std::floor(maxY));
    const int z0 = static_cast<int>(std::floor(minZ));
    const int z1 = static_cast<int>(std::floor(maxZ));

    const int exp = gameData.collisionSearchExpansion;
    for (int x = x0 - exp; x <= x1 + exp; ++x) {
        for (int y = y0 - exp; y <= y1 + exp; ++y) {
            for (int z = z0 - exp; z <= z1 + exp; ++z) {
                const std::uint16_t stateId = getBlock(world, x, y, z);
                if (stateId == 0 || !gameData.solidByRuntimeId[stateId]) {
                    continue;
                }

                const BlockDefinition* def = findBlockDefinitionForBlockType(gameData, stateId);
                const std::vector<CollisionBox>* stateBoxes = collisionBoxesForState(gameData, stateId);
                if (def == nullptr || ((stateBoxes == nullptr || stateBoxes->empty()) && def->collisionBoxes.empty())) {
                    return true;
                }

                const float bx = static_cast<float>(x);
                const float by = static_cast<float>(y);
                const float bz = static_cast<float>(z);
                const std::vector<CollisionBox>& boxes =
                    (stateBoxes != nullptr && !stateBoxes->empty()) ? *stateBoxes : def->collisionBoxes;
                for (const auto& box : boxes) {
                    if (minX < bx + box.maxX && maxX > bx + box.minX &&
                        minY < by + box.maxY && maxY > by + box.minY &&
                        minZ < bz + box.maxZ && maxZ > bz + box.minZ) {
                        return true;
                    }
                }
            }
        }
    }

    return false;
}

}  // namespace

HeadlessServer::HeadlessServer(GameData gameData)
    : gameData_(std::move(gameData)),
      persistence_(nullptr),
      lastChunkMaintenance_(std::chrono::steady_clock::now() - kChunkMaintenanceInterval),
      lastTick_(std::chrono::steady_clock::now()) {
    std::filesystem::path standardPath = getStandardSavePath();
    std::filesystem::path legacyPath = "world_save";

    if (std::filesystem::exists(legacyPath) && !std::filesystem::exists(standardPath)) {
        std::cout << "Migrating " << legacyPath << " to " << standardPath << "...\n";
        try {
            std::filesystem::create_directories(standardPath.parent_path());
            std::filesystem::rename(legacyPath, standardPath);
        } catch (const std::exception& e) {
            std::cerr << "Migration failed: " << e.what() << ". Falling back to legacy path.\n";
            standardPath = legacyPath;
        }
    }

    persistence_ = std::make_unique<WorldPersistence>(standardPath);
    chunkTasks_ = std::make_unique<ChunkTaskQueue>(simulation_.terrainGenerator(), gameData_);

    scriptManager_ = std::make_unique<ScriptManager>();
    scriptManager_->setWorldSimulation(&simulation_);
    scriptManager_->setGameData(&gameData_);
    scriptManager_->setHostKind(ScriptHost::Server);
}

HeadlessServer::~HeadlessServer() {
    std::cout << "Saving all players and chunks...\n";
    for (auto& [id, player] : players_) {
        persistence_->savePlayer(player);
    }
    for (auto& [coord, chunk] : simulation_.world().chunks) {
        persistence_->saveChunk(chunk);
    }
}

bool HeadlessServer::start(const std::uint16_t port, const std::filesystem::path& projectRoot) {
    if (!network_.startServer(port)) {
        return false;
    }
    network_.setExternalBlockAuthority(true);

    // Initialize runtime scripting
    packManager_.discover(projectRoot / "packs");
    scriptManager_->loadGameData(packManager_, projectRoot / "engine" / "scripts");
    scriptManager_->loadRuntimeScripts(packManager_);

    ensureChunksAround({0, 0, 0});
    std::cout << "Headless world loaded with " << simulation_.world().chunks.size() << " chunk(s).\n";
    return true;
}

void HeadlessServer::tick() {
    const auto now = std::chrono::steady_clock::now();
    const float deltaTime = std::chrono::duration<float>(now - lastTick_).count();
    lastTick_ = now;

    network_.poll();
    chunkTasks_->poll();

    // Handle player joins
    for (std::uint32_t playerId : network_.takePendingPlayerJoins()) {
        players_[playerId] = Player();
        players_[playerId].name = "Player_" + std::to_string(playerId);
        std::cout << "Player " << playerId << " joined.\n";
        network_.broadcastChatMessage(0, "Player_" + std::to_string(playerId) + " joined the game.");
    }

    // Handle player disconnects
    static std::unordered_set<std::uint32_t> lastPlayers;
    std::unordered_set<std::uint32_t> currentPlayers;
    for (const auto& [id, player] : players_) {
        currentPlayers.insert(id);
    }

    // This is a bit inefficient to do every tick, but players_ is small.
    // Better way: Check who was in lastPlayers but NOT in network.remotePlayers() anymore.
    // Wait, NetworkManager already removed them from its internal map.
    // I need to know who was removed.
    
    // Actually, I'll just check if they are still in network.remotePlayers() OR if it's local (which it isn't here).
    auto it = players_.begin();
    while (it != players_.end()) {
        if (network_.remotePlayers().find(it->first) == network_.remotePlayers().end()) {
            std::cout << "Player " << it->second.name << " (" << it->first << ") disconnected. Saving data.\n";
            network_.broadcastChatMessage(0, it->second.name + " left the game.");
            persistence_->savePlayer(it->second);
            lastSentChunkStateByPlayer_.erase(it->first);
            knownEntitiesByPlayer_.erase(it->first);
            it = players_.erase(it);
        } else {
            ++it;
        }
    }

    // Handle player state updates
    for (const auto& state : network_.takePendingPlayerStates()) {
        auto it = players_.find(state.id);
        if (it != players_.end()) {
            // If it's a new player with a name we haven't seen, try to load them
            if (it->second.name == ("Player_" + std::to_string(state.id)) && state.name != it->second.name) {
                network_.broadcastChatMessage(0, it->second.name + " is now known as " + state.name);
                auto loaded = persistence_->loadPlayer(state.name);
                if (loaded) {
                    std::cout << "Loaded persisted data for player: " << state.name << "\n";
                    it->second = *loaded;
                    // Sync loaded inventory back to client
                    for (int i = 0; i < kInventorySlots; ++i) {
                        const auto& slot = it->second.inventory.slots[i];
                        network_.sendInventoryUpdate(state.id, i, slot.itemId, slot.count);
                    }
                }
            }
            it->second.name = state.name;
            it->second.position = state.position;
            it->second.yaw = state.yaw;
            it->second.pitch = state.pitch;
        }
    }

    processChunkRequests();
    processSelectSlotRequests();
    processCraftRequests();
    processBlockEditRequests();

    if (scriptManager_) {
        scriptManager_->tick(deltaTime);
    }

    // Broadcast world time periodically (e.g. every 2 seconds)
    static auto lastTimeSync = std::chrono::steady_clock::now();
    if (std::chrono::duration<float>(now - lastTimeSync).count() >= 2.0f) {
        network_.publishWorldTime(simulation_.time());
        lastTimeSync = now;
    }

    // Log chat messages (NetworkManager handles broadcasting)
    for (auto& chat : network_.takePendingChatMessages()) {
        std::string senderName = "Unknown";
        auto it = players_.find(chat.playerId);
        if (it != players_.end()) {
            senderName = it->second.name;
        }

        if (!chat.message.empty() && chat.message[0] == '/' && scriptManager_) {
            for (const auto& reply : scriptManager_->executeCommand(chat.playerId, chat.message)) {
                network_.broadcastChatMessage(0, reply);
                std::cout << "[COMMAND] " << reply << std::endl;
            }
            continue;
        }
        std::cout << "[CHAT] " << senderName << " (" << chat.playerId << "): " << chat.message << std::endl;
    }

    simulation_.tick(gameData_, deltaTime, [this](const Int3& block, const std::uint16_t stateId) {
        network_.publishBlockChange(block, stateId);
    });

    // Item pickup logic
    std::vector<Entity*> nearbyEntities;
    for (auto& [playerId, player] : players_) {
        nearbyEntities.clear();
        simulation_.queryEntities(player.position, 2.0f, nearbyEntities);

        for (auto* entity : nearbyEntities) {
            if (entity->type == EntityType::Item) {
                ItemEntity* item = static_cast<ItemEntity*>(entity);
                if (addItem(player.inventory, gameData_, item->itemId, item->count)) {
                    std::cout << "Player " << player.name << " picked up " << item->count << "x " << item->itemId << "\n";
                    // Sync inventory to player
                    for (int i = 0; i < kInventorySlots; ++i) {
                        const auto& slot = player.inventory.slots[i];
                        network_.sendInventoryUpdate(playerId, i, slot.itemId, slot.count);
                    }
                    // Destroy entity
                    network_.publishEntityDestroy(item->id);
                    simulation_.removeEntity(item->id);
                }
            }
        }
    }

    // Broadcast entity positions and handle targeted spawns/destroys
    constexpr float kEntitySyncRadius = 64.0f;
    std::vector<Entity*> entitiesInRange;
    for (auto& [playerId, player] : players_) {
        entitiesInRange.clear();
        simulation_.queryEntities(player.position, kEntitySyncRadius, entitiesInRange);

        auto& known = knownEntitiesByPlayer_[playerId];
        std::unordered_set<std::uint32_t> currentInRangeIds;

        for (auto* entity : entitiesInRange) {
            currentInRangeIds.insert(entity->id);
            if (known.find(entity->id) == known.end()) {
                // Spawn for player
                network_.sendEntitySpawnToPlayer(playerId, *entity);
                known.insert(entity->id);
            } else {
                // Update position for player
                network_.sendEntityPositionToPlayer(playerId, *entity);
            }
        }

        // Remove entities that are no longer in range
        auto knownIt = known.begin();
        while (knownIt != known.end()) {
            if (currentInRangeIds.find(*knownIt) == currentInRangeIds.end()) {
                network_.sendEntityDestroyToPlayer(playerId, *knownIt);
                knownIt = known.erase(knownIt);
            } else {
                ++knownIt;
            }
        }
    }

    if (now - lastChunkMaintenance_ >= kChunkMaintenanceInterval) {
        ensureChunksAroundPlayers();
        expireUnusedChunks();
        lastChunkMaintenance_ = now;
    }
}

void HeadlessServer::processChunkRequests() {
    for (const NetworkChunkRequest& request : network_.takePendingChunkRequests()) {
        const std::uint8_t radius = static_cast<std::uint8_t>(std::min<int>(request.radius, kViewDistance));
        ensureChunksAroundPlayer(request.playerId, request.center, radius);
    }
}

void HeadlessServer::processBlockEditRequests() {
    for (const NetworkBlockChange& request : network_.takePendingBlockEditRequests()) {
        if (!isKnownState(request.stateId)) {
            continue;
        }
        if (!isWithinReach(request)) {
            continue;
        }

        const ChunkCoord targetChunk = worldToChunkCoord(request.block.x, request.block.y, request.block.z);
        if (!ensureChunkLoaded(targetChunk)) {
            continue;
        }
        if (!isValidBlockEditTarget(request)) {
            continue;
        }

        auto playerIt = players_.find(request.playerId);
        if (playerIt == players_.end()) {
            continue;
        }
        Player& player = playerIt->second;

        // Inventory validation for placement
        const std::uint16_t current = getBlock(simulation_.world(), request.block.x, request.block.y, request.block.z);
        if (current == 0 && request.stateId != 0) {
            // Placement: Check if player has the item that places this block
            const auto itemId = selectedItemId(player.inventory);
            if (!itemId.has_value()) {
                continue;
            }

            const auto placingId = blockTypeForItemId(gameData_, itemId.value());
            if (!placingId.has_value() || placingId.value() != request.stateId) {
                continue;
            }

            // Consume item
            if (!removeSelectedItem(player.inventory, 1)) {
                continue;
            }

            // Sync updated slot to player
            const int slotIndex = player.inventory.selectedIndex;
            const auto& slot = player.inventory.slots[slotIndex];
            network_.sendInventoryUpdate(request.playerId, slotIndex, slot.itemId, slot.count);
        }

        const BlockChangeResult result = simulation_.applyBlockChange(request.block, request.stateId);
        if (!result.applied || !result.changed) {
            continue;
        }

        // Spawning items on block break
        if (request.stateId == 0 && current != 0) {
            const BlockDefinition* def = findBlockDefinitionForBlockType(gameData_, current);
            if (def != nullptr) {
                for (const auto& drop : def->drops) {
                    auto item = std::make_unique<ItemEntity>(0, drop.item, drop.count);
                    item->position = {
                        request.block.x + 0.5f,
                        request.block.y + 0.5f,
                        request.block.z + 0.5f
                    };
                    // Give some random velocity
                    std::mt19937 gen(std::random_device{}());
                    std::uniform_real_distribution<float> dist(-1.0f, 1.0f);
                    item->velocity = { dist(gen), 2.0f, dist(gen) };

                    const auto entity = simulation_.spawnEntity(std::move(item));
                    network_.publishEntitySpawn(*simulation_.findEntity(entity));
                }
            }
        }

        network_.publishBlockChange(request.block, request.stateId);
    }
}

void HeadlessServer::processSelectSlotRequests() {
    for (const NetworkSelectSlot& request : network_.takePendingSelectSlotRequests()) {
        auto playerIt = players_.find(request.playerId);
        if (playerIt != players_.end()) {
            selectSlot(playerIt->second.inventory, request.slotIndex);
        }
    }
}

void HeadlessServer::processCraftRequests() {
    for (const NetworkCraftRequest& request : network_.takePendingCraftRequests()) {
        auto it = players_.find(request.playerId);
        if (it != players_.end()) {
            auto& player = it->second;
            const auto recipeIt = gameData_.recipes.find(request.recipeId);
            if (recipeIt == gameData_.recipes.end()) continue;

            const auto& recipe = recipeIt->second;

            // Map ingredients to counts
            std::unordered_map<std::string, int> required;
            for (const auto& itemId : recipe.ingredients) {
                required[itemId]++;
            }

            // Check ingredients
            bool hasAll = true;
            for (const auto& [itemId, count] : required) {
                if (!hasItem(player.inventory, itemId, count)) {
                    hasAll = false;
                    break;
                }
            }

            if (hasAll) {
                // Remove ingredients
                for (const auto& [itemId, count] : required) {
                    removeItem(player.inventory, itemId, count);
                }

                // Add output
                addItem(player.inventory, gameData_, recipe.output, recipe.count);

                // Sync inventory
                for (int i = 0; i < kInventorySlots; ++i) {
                    const auto& slot = player.inventory.slots[i];
                    network_.sendInventoryUpdate(request.playerId, i, slot.itemId, slot.count);
                }

                std::cout << "Player " << player.name << " crafted " << recipe.count << "x " << recipe.output << "\n";
            }
        }
    }
}

void HeadlessServer::ensureChunksAroundPlayers() {
    for (const std::uint32_t playerId : network_.takePendingPlayerJoins()) {
        Player& player = players_[playerId];
        // Give some starting items for testing if empty
        if (player.inventory.slots[0].count == 0) {
            addItem(player.inventory, gameData_, "base:grass_block", 64);
            addItem(player.inventory, gameData_, "base:dirt", 64);
            addItem(player.inventory, gameData_, "base:stone", 64);
            addItem(player.inventory, gameData_, "base:oak_log", 64);
            addItem(player.inventory, gameData_, "base:oak_leaves", 64);
            addItem(player.inventory, gameData_, "base:sand", 64);
            addItem(player.inventory, gameData_, "base:water", 64);
        }

        // Sync initial inventory to player
        for (int i = 0; i < kInventorySlots; ++i) {
            const auto& slot = player.inventory.slots[i];
            network_.sendInventoryUpdate(playerId, i, slot.itemId, slot.count);
        }

        sendLoadedChunksToPlayer(playerId, {0, 0, 0});
    }

    if (network_.remotePlayers().empty()) {
        ensureChunksAround({0, 0, 0});
        return;
    }

    for (const auto& [playerId, player] : network_.remotePlayers()) {
        ensureChunksAroundPlayer(playerId, chunkForPosition(player.position), kServerChunkRadius);
    }
}

void HeadlessServer::ensureChunksAround(const ChunkCoord& center) {
    for (int dz = -kServerChunkRadius; dz <= kServerChunkRadius; ++dz) {
        for (int dx = -kServerChunkRadius; dx <= kServerChunkRadius; ++dx) {
            for (int cy = 0; cy < kChunkCountY; ++cy) {
                ensureChunkLoaded({center.x + dx, cy, center.z + dz});
            }
        }
    }
}

void HeadlessServer::ensureChunksAroundPlayer(
    const std::uint32_t playerId,
    const ChunkCoord& center,
    const std::uint8_t radius
) {
    auto& sentChunks = sentChunksByPlayer_[playerId];

    const int r = static_cast<int>(radius);
    const float rSq = static_cast<float>(r * r);

    for (int dz = -r; dz <= r; ++dz) {
        for (int dx = -r; dx <= r; ++dx) {
            for (int cy = 0; cy < kChunkCountY; ++cy) {
                const int dy = cy - center.y;
                if (dx * dx + dy * dy + dz * dz > rSq) {
                    continue;
                }

                const ChunkCoord coord {center.x + dx, cy, center.z + dz};
                if (!ensureChunkLoaded(coord)) {
                    continue;
                }
                
                const Chunk& currentChunk = getChunk(simulation_.world(), coord);
                
                if (sentChunks.insert(coord).second) {
                    // First time sending this chunk to this player
                    network_.sendCompressedChunkSnapshotToPlayer(playerId, currentChunk);
                    lastSentChunkStateByPlayer_[playerId][coord] = currentChunk;
                } else {
                    // Already sent, check for deltas
                    auto& lastStates = lastSentChunkStateByPlayer_[playerId];
                    auto it = lastStates.find(coord);
                    if (it != lastStates.end()) {
                        network_.sendDeltaChunkSnapshot(network_.peerForPlayer(playerId), currentChunk, it->second);
                        it->second = currentChunk;
                    } else {
                        // Fallback if cache missed
                        network_.sendCompressedChunkSnapshotToPlayer(playerId, currentChunk);
                        lastStates[coord] = currentChunk;
                    }
                }
            }
        }
    }
}

void HeadlessServer::expireUnusedChunks() {
    const auto now = std::chrono::steady_clock::now();
    const double nowSeconds = std::chrono::duration<double>(now.time_since_epoch()).count();

    const int unloadDist = kServerChunkRadius + 2;
    const float unloadDistSq = static_cast<float>(unloadDist * unloadDist);

    std::vector<ChunkCoord> toRemove;
    for (auto& [coord, chunk] : simulation_.world().chunks) {
        // Find if any player is near this chunk
        bool nearAnyPlayer = false;
        for (const auto& pair : network_.remotePlayers()) {
            const auto& player = pair.second;
            const ChunkCoord playerChunk = chunkForPosition(player.position);
            const int dx = coord.x - playerChunk.x;
            const int dy = coord.y - playerChunk.y;
            const int dz = coord.z - playerChunk.z;

            if (dx * dx + dy * dy + dz * dz <= unloadDistSq) {
                nearAnyPlayer = true;
                break;
            }
        }

        if (!nearAnyPlayer) {
            // No player nearby, check if it's been unused for a while
            // We can't easily track access time in World without modifying it, 
            // so we'll just check if it's far from ALL players for a certain duration.
            // Actually, we added lastChunkAccess_ to WorldSimulation, let's use it.
            // If it's not in the map, we initialize it now.
            // (Note: we need to update access time when players are near)
        }
    }
    // Simplified for now: if no players are near and it's not spawn, remove.
    // Spawn is at {0,0,0}.

    for (auto it = simulation_.world().chunks.begin(); it != simulation_.world().chunks.end(); ) {
        const ChunkCoord& coord = it->first;
        if (std::abs(coord.x) <= kServerChunkRadius && std::abs(coord.z) <= kServerChunkRadius) {
            ++it;
            continue; // Keep spawn area
        }

        bool nearAnyPlayer = false;
        for (const auto& pair : network_.remotePlayers()) {
            const auto& player = pair.second;
            const ChunkCoord playerChunk = chunkForPosition(player.position);
            const int dx = coord.x - playerChunk.x;
            const int dy = coord.y - playerChunk.y;
            const int dz = coord.z - playerChunk.z;

            if (dx * dx + dy * dy + dz * dz <= unloadDistSq) {
                nearAnyPlayer = true;
                break;
            }
        }

        if (!nearAnyPlayer) {
            // Save before unloading
            persistence_->saveChunk(it->second);
            it = simulation_.world().chunks.erase(it);
        } else {
            ++it;
        }
    }
}

void HeadlessServer::sendLoadedChunksToPlayer(const std::uint32_t playerId, const ChunkCoord& center) {
    ensureChunksAroundPlayer(playerId, center, kServerChunkRadius);
}

bool HeadlessServer::ensureChunkLoaded(const ChunkCoord& coord) {
    if (simulation_.world().chunks.contains(coord)) {
        return true;
    }

    if (pendingChunks_.contains(coord)) {
        return false;
    }

    pendingChunks_.insert(coord);
    chunkTasks_->enqueue(coord, [this, coord](Chunk chunk) {
        // If we have a saved version, use it instead of the newly generated one
        if (auto saved = persistence_->loadChunk(coord)) {
            chunk = std::move(*saved);
        }

        pendingChunks_.erase(coord);
        simulation_.world().chunks[coord] = std::move(chunk);
        network_.publishCompressedChunkSnapshot(simulation_.world().chunks[coord]);
    });

    return false;
}

bool HeadlessServer::isKnownState(const std::uint16_t stateId) const {
    return stateId == 0 || findBlockDefinitionForBlockType(gameData_, stateId) != nullptr;
}

bool HeadlessServer::isWithinReach(const NetworkBlockChange& request) const {
    const auto playerIt = network_.remotePlayers().find(request.playerId);
    if (playerIt == network_.remotePlayers().end()) {
        return false;
    }

    const Vec3 eye {
        playerIt->second.position.x,
        playerIt->second.position.y + 1.62f,
        playerIt->second.position.z
    };
    const Vec3 blockCenter {
        static_cast<float>(request.block.x) + 0.5f,
        static_cast<float>(request.block.y) + 0.5f,
        static_cast<float>(request.block.z) + 0.5f
    };

    return length(blockCenter - eye) <= kReach + 0.75f;
}

bool HeadlessServer::isValidBlockEditTarget(const NetworkBlockChange& request) {
    if (!isYInBounds(request.block.y)) {
        return false;
    }

    const std::uint16_t current = getBlock(simulation_.world(), request.block.x, request.block.y, request.block.z);
    if (request.stateId == 0) {
        const BlockDefinition* currentDef = findBlockDefinitionForBlockType(gameData_, current);
        return current != 0 && (currentDef == nullptr || currentDef->material != "liquid");
    }

    const BlockDefinition* placedDef = findBlockDefinitionForBlockType(gameData_, request.stateId);
    if (placedDef == nullptr || placedDef->material == "liquid") {
        return false;
    }
    if (current != 0 || isObstructedByModel(simulation_.world(), gameData_, request.block)) {
        return false;
    }

    if (isCropBlock(*placedDef)) {
        const std::uint16_t soil = getBlock(simulation_.world(), request.block.x, request.block.y - 1, request.block.z);
        if (!isCropSoil(gameData_, soil) ||
            getBlock(simulation_.world(), request.block.x, request.block.y + 1, request.block.z) != 0) {
            return false;
        }
    }

    if (gameData_.solidByRuntimeId[request.stateId]) {
        const auto playerIt = network_.remotePlayers().find(request.playerId);
        if (playerIt == network_.remotePlayers().end()) {
            return false;
        }

        setBlock(simulation_.world(), request.block.x, request.block.y, request.block.z, request.stateId);
        const bool wouldCollide = playerCollidesAt_Headless(simulation_.world(), gameData_, playerIt->second.position);
        setBlock(simulation_.world(), request.block.x, request.block.y, request.block.z, current);
        if (wouldCollide) {
            return false;
        }
    }

    return true;
}

}  // namespace voxel
