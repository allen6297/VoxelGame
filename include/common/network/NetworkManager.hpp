#pragma once

#include <cstdint>
#include <vector>
#include <string>
#include <unordered_map>
#include <optional>

#include "Math.hpp"
#include "world/Chunk.hpp"
#include "Entity.hpp"

struct _ENetHost;
struct _ENetPeer;
typedef struct _ENetHost ENetHost;
typedef struct _ENetPeer ENetPeer;

namespace voxel {

struct RemotePlayerState {
    std::uint32_t id = 0;
    std::string name = "Player";
    Vec3 position {0.0f, 0.0f, 0.0f};
    float yaw = 0.0f;
    float pitch = 0.0f;
};

struct NetworkBlockChange {
    Int3 block {0, 0, 0};
    std::uint16_t stateId = 0;
    std::uint32_t playerId = 0;
};

struct NetworkChunkSnapshot {
    Chunk chunk;
};

struct NetworkDeltaChunkSnapshot {
    ChunkCoord coord;
    struct Change {
        std::uint16_t index;
        std::uint16_t stateId;
    };
    std::vector<Change> changes;
};

struct NetworkChunkRequest {
    std::uint32_t playerId = 0;
    ChunkCoord center {};
    std::uint8_t radius = 0;
};

struct NetworkInventoryUpdate {
    std::uint8_t slotIndex = 0;
    std::string itemId;
    int count = 0;
};

struct NetworkSelectSlot {
    std::uint32_t playerId = 0;
    std::uint8_t slotIndex = 0;
};

struct NetworkEntitySpawn {
    std::uint32_t id = 0;
    EntityType type = EntityType::Item;
    Vec3 position {0.0f, 0.0f, 0.0f};
    std::string metadata; // e.g. item ID for ItemEntity
};

struct NetworkEntityDestroy {
    std::uint32_t id = 0;
};

struct NetworkEntityPosition {
    std::uint32_t id = 0;
    Vec3 position {0.0f, 0.0f, 0.0f};
    float yaw = 0.0f;
    float pitch = 0.0f;
};

struct NetworkChatMessage {
    std::uint32_t playerId = 0;
    std::string message;
};

struct NetworkCraftRequest {
    std::uint32_t playerId = 0;
    std::string recipeId;
};

class NetworkManager {
public:
    enum class Mode {
        None,
        Server,
        Client
    };

    NetworkManager();
    ~NetworkManager();

    NetworkManager(const NetworkManager&) = delete;
    NetworkManager& operator=(const NetworkManager&) = delete;

    bool startServer(std::uint16_t port);
    bool connectToServer(const std::string& hostName, std::uint16_t port);
    void shutdown();

    void poll();
    void publishLocalPlayer(const std::string& name, const Vec3& position, float yaw, float pitch);
    void publishBlockChange(const Int3& block, std::uint16_t stateId);
    void publishChunkSnapshot(const Chunk& chunk);
    void publishCompressedChunkSnapshot(const Chunk& chunk);
    void publishChunkInterest(const ChunkCoord& center, std::uint8_t radius);
    void publishSelectSlot(int slotIndex);
    void publishEntitySpawn(const Entity& entity);
    void publishEntityPosition(const Entity& entity);
    void publishEntityDestroy(std::uint32_t entityId);
    void publishChatMessage(const std::string& message);
    void publishWorldTime(double time);
    void publishCraftRequest(const std::string& recipeId);
    bool sendChunkSnapshotToPlayer(std::uint32_t playerId, const Chunk& chunk);
    bool sendCompressedChunkSnapshotToPlayer(std::uint32_t playerId, const Chunk& chunk);
    bool sendInventoryUpdate(std::uint32_t playerId, int slotIndex, const std::string& itemId, int count);
    bool sendEntitySpawnToPlayer(std::uint32_t playerId, const Entity& entity);
    bool sendEntityPositionToPlayer(std::uint32_t playerId, const Entity& entity);
    bool sendEntityDestroyToPlayer(std::uint32_t playerId, std::uint32_t entityId);
    std::vector<NetworkBlockChange> takePendingBlockChanges();
    std::vector<NetworkBlockChange> takePendingBlockEditRequests();
    std::vector<NetworkChunkSnapshot> takePendingChunkSnapshots();
    std::vector<NetworkDeltaChunkSnapshot> takePendingDeltaChunkSnapshots();
    std::vector<NetworkChunkRequest> takePendingChunkRequests();
    std::vector<NetworkSelectSlot> takePendingSelectSlotRequests();
    std::vector<NetworkEntitySpawn> takePendingEntitySpawns();
    std::vector<NetworkEntityPosition> takePendingEntityPositions();
    std::vector<std::uint32_t> takePendingEntityDestroys();
    std::vector<std::uint32_t> takePendingPlayerJoins();
    std::vector<RemotePlayerState> takePendingPlayerStates();
    std::vector<NetworkChatMessage> takePendingChatMessages();
    std::vector<NetworkCraftRequest> takePendingCraftRequests();
    std::optional<double> takePendingWorldTime();
    void setExternalBlockAuthority(bool enabled) { externalBlockAuthority_ = enabled; }

    bool isConnected() const {
        return mode_ == Mode::Client && serverPeer_ != nullptr;
    }

    Mode mode() const { return mode_; }
    std::uint32_t localPlayerId() const { return localPlayerId_; }
    const std::unordered_map<std::uint32_t, RemotePlayerState>& remotePlayers() const {
        return remotePlayers_;
    }

    // Client-only: last authoritative inventory state received from server
    const std::vector<NetworkInventoryUpdate>& pendingInventoryUpdates() const {
        return pendingInventoryUpdates_;
    }
    void clearPendingInventoryUpdates() { pendingInventoryUpdates_.clear(); }

    ENetPeer* peerForPlayer(std::uint32_t playerId) const;
    void sendDeltaChunkSnapshot(ENetPeer* peer, const Chunk& chunk, const Chunk& lastChunk);

    void broadcastChatMessage(std::uint32_t playerId, const std::string& message, ENetPeer* exceptPeer = nullptr);

private:
    void handlePacket(ENetPeer* peer, const std::uint8_t* data, std::size_t size);
    void sendAssignId(ENetPeer* peer, std::uint32_t id);
    void broadcastPlayerState(const RemotePlayerState& state, ENetPeer* exceptPeer);
    void sendPlayerState(ENetPeer* peer, const RemotePlayerState& state);
    void broadcastBlockChange(const NetworkBlockChange& change, ENetPeer* exceptPeer);
    void sendBlockChange(ENetPeer* peer, const NetworkBlockChange& change);
    void broadcastChunkSnapshot(const Chunk& chunk);
    void broadcastCompressedChunkSnapshot(const Chunk& chunk);
    void sendChunkSnapshot(ENetPeer* peer, const Chunk& chunk);
    void sendCompressedChunkSnapshot(ENetPeer* peer, const Chunk& chunk);
    void sendChunkRequest(ENetPeer* peer, const ChunkCoord& center, std::uint8_t radius);
    void sendInventoryUpdate(ENetPeer* peer, int slotIndex, const std::string& itemId, int count);
    void sendSelectSlot(ENetPeer* peer, int slotIndex);
    void sendEntitySpawn(ENetPeer* peer, const NetworkEntitySpawn& spawn);
    void broadcastEntitySpawn(const NetworkEntitySpawn& spawn, ENetPeer* exceptPeer);
    void sendEntityPosition(ENetPeer* peer, const NetworkEntityPosition& pos);
    void broadcastEntityPosition(const NetworkEntityPosition& pos, ENetPeer* exceptPeer);
    void sendEntityDestroy(ENetPeer* peer, std::uint32_t id);
    void broadcastEntityDestroy(std::uint32_t id, ENetPeer* exceptPeer);
    void sendChatMessage(ENetPeer* peer, std::uint32_t playerId, const std::string& message);
    void sendWorldTime(ENetPeer* peer, double time);
    void broadcastWorldTime(double time, ENetPeer* exceptPeer);
    void sendCraftRequest(ENetPeer* peer, const std::string& recipeId);

private:
    ENetHost* host_ = nullptr;
    ENetPeer* serverPeer_ = nullptr;
    Mode mode_ = Mode::None;
    std::uint32_t localPlayerId_ = 0;
    std::uint32_t nextPlayerId_ = 2;
    std::unordered_map<ENetPeer*, std::uint32_t> peerIds_;
    std::unordered_map<std::uint32_t, RemotePlayerState> playerStates_;
    std::unordered_map<std::uint32_t, RemotePlayerState> remotePlayers_;
    std::vector<RemotePlayerState> pendingPlayerStates_;
    std::vector<NetworkBlockChange> pendingBlockChanges_;
    std::vector<NetworkBlockChange> pendingBlockEditRequests_;
    std::vector<NetworkChunkSnapshot> pendingChunkSnapshots_;
    std::vector<NetworkDeltaChunkSnapshot> pendingDeltaChunkSnapshots_;
    std::vector<NetworkChunkRequest> pendingChunkRequests_;
    std::vector<NetworkInventoryUpdate> pendingInventoryUpdates_;
    std::vector<NetworkSelectSlot> pendingSelectSlotRequests_;
    std::vector<NetworkEntitySpawn> pendingEntitySpawns_;
    std::vector<NetworkEntityPosition> pendingEntityPositions_;
    std::vector<std::uint32_t> pendingEntityDestroys_;
    std::vector<std::uint32_t> pendingPlayerJoins_;
    std::vector<NetworkChatMessage> pendingChatMessages_;
    std::vector<NetworkCraftRequest> pendingCraftRequests_;
    std::optional<double> pendingWorldTime_;
    bool externalBlockAuthority_ = false;
};

}  // namespace voxel
