#pragma once

#include <future>
#include <deque>
#include <queue>
#include <random>
#include <unordered_map>
#include <vector>

#include "data/GameData.hpp"
#include "ecs/EntitySystem.hpp"
#include "Input.hpp"
#include "network/NetworkManager.hpp"
#include "Player.hpp"
#include "render/Mesh.hpp"
#include "render/ModelManager.hpp"
#include "render/RenderBackend.hpp"
#include "render/Renderer.hpp"
#include "render/TextureManager.hpp"
#include "world/TerrainGenerator.hpp"
#include "world/World.hpp"
#include "world/WorldSimulation.hpp"

namespace voxel {
class Game {
public:
    Game(IRenderBackend& backend, GameData gameData, std::string assetsRoot, std::string playerName, NetworkManager* network = nullptr);
    ~Game();
    void update(const ClientInputFrame& input, float deltaTime);
    void reloadContent();
    void render(int framebufferWidth, int framebufferHeight) const;
    void renderHotbarIcons(int framebufferWidth, int framebufferHeight);
    void renderBlockPreview(int framebufferWidth, int framebufferHeight,
                            const std::string& blockId, int x, int y, int width, int height);
    DebugOverlayData getDebugData() const;
    const Inventory& getInventory() const { return player_.inventory; }
    const GameData& gameData() const { return gameData_; }
    Player& player() { return player_; }
    const Player& player() const { return player_; }
    ecs::EntitySystem& entitySystem() { return entitySystem_; }
    const ecs::EntitySystem& entitySystem() const { return entitySystem_; }
    WorldSimulation& simulation() { return simulation_; }
    const WorldSimulation& simulation() const { return simulation_; }

private:
    struct PendingMesh {
        ChunkCoord coord;
        std::future<ChunkMesh> future;
    };
    struct PendingMeshUpload {
        ChunkCoord coord;
        ChunkMesh mesh;
        std::size_t nextSurface = 0;
    };
    struct ScheduledBlockTick {
        double dueTime = 0.0;
        Int3 block {0, 0, 0};
        std::uint32_t generation = 0;
    };
    struct ScheduledBlockTickCompare {
        bool operator()(const ScheduledBlockTick& a, const ScheduledBlockTick& b) const {
            return a.dueTime > b.dueTime;
        }
    };

    void simulateLiquids(float deltaTime);
    void processBlockTicks();
    void scheduleBlockTick(const Int3& block, std::uint16_t stateId, float delaySeconds);
    void scheduleChunkBlockTicks(const Chunk& chunk);
    void handleBlockActions();
    void handleInventorySelection(const ClientInputFrame& input);
    void rebuildChunkMesh(const ChunkCoord& coord);
    void rebuildMeshesAroundBlock(const Int3& block);
    void updateLoadedChunks(const ChunkCoord& playerChunk);
    void launchMeshBuild(const ChunkCoord& coord);
    void collectPending(const ChunkCoord& playerChunk);
    void discardPendingMeshUpload(const ChunkCoord& coord);
    void applyNetworkBlockChanges();
    void applyNetworkEntityChanges();
    void reloadGameData();
    void populateFaceTextures();

    std::string assetsRoot_;
    bool f5WasPressed_ = false;
    GameData gameData_;
    IRenderBackend& renderBackend_;
    TextureManager textureManager_;
    ModelManager modelManager_;
    NetworkManager* network_ = nullptr;
    ecs::EntitySystem entitySystem_;
    WorldSimulation simulation_;
    std::unordered_map<ChunkCoord, ChunkMesh, ChunkCoordHash> meshes_;
    std::unordered_map<ChunkCoord, std::future<Chunk>, ChunkCoordHash> pendingTerrain_;
    std::unordered_map<std::string, ChunkMesh> iconMeshes_;  // cached single-block meshes for hotbar icons
    std::vector<PendingMesh> pendingMeshes_;
    std::deque<PendingMeshUpload> pendingMeshUploads_;
    std::vector<ChunkCoord> queuedMeshBuilds_;
    Player player_;
    InputState input_;
    std::optional<RaycastHit> currentHit_;
    std::priority_queue<ScheduledBlockTick, std::vector<ScheduledBlockTick>, ScheduledBlockTickCompare> blockTicks_;
    std::unordered_map<std::string, std::uint32_t> blockTickGeneration_;
    float liquidStepAccumulator_ = 0.0f;
    double gameTimeSeconds_ = 0.0;
    bool hasRequestedNetworkChunks_ = false;
    ChunkCoord lastRequestedNetworkChunk_ {};
    float fpsAccumulator_ = 0.0f;
    int fpsFrameCount_ = 0;
    int fps_ = 0;
    float frameTimeMs_ = 0.0f;
    std::mt19937 rng_ {std::random_device{}()};
};
}  // namespace voxel
