#pragma once

#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>

namespace voxel {

enum class EnginePhase : std::size_t {
    Update = 0,
    Render,
    Network,
    Simulation,
    Scripts,
    ChunkMaintenance,
    TerrainIntegration,
    MeshCompletion,
    MeshBuild,
    MeshUpload,
    MeshQueue,
    Count
};

struct EngineDiagnosticsMetricStats {
    double average = 0.0;
    double minimum = 0.0;
    double maximum = 0.0;
};

struct EngineDiagnosticsRollingStats {
    static constexpr std::size_t WindowSize = 120;

    std::size_t sampleCount = 0;
    EngineDiagnosticsMetricStats frameDeltaMs;
    EngineDiagnosticsMetricStats updateMs;
    EngineDiagnosticsMetricStats renderMs;
    EngineDiagnosticsMetricStats networkMs;
    EngineDiagnosticsMetricStats simulationMs;
    EngineDiagnosticsMetricStats scriptsMs;
    EngineDiagnosticsMetricStats chunkMaintenanceMs;
    EngineDiagnosticsMetricStats terrainIntegrationMs;
    EngineDiagnosticsMetricStats meshCompletionMs;
    EngineDiagnosticsMetricStats meshBuildMs;
    EngineDiagnosticsMetricStats meshUploadMs;
    EngineDiagnosticsMetricStats meshQueueMs;
};

struct EngineDiagnosticsSnapshot {
    std::uint64_t frameIndex = 0;
    double frameDeltaMs = 0.0;
    double updateMs = 0.0;
    double renderMs = 0.0;
    double networkMs = 0.0;
    double simulationMs = 0.0;
    double scriptsMs = 0.0;
    double chunkMaintenanceMs = 0.0;
    double terrainIntegrationMs = 0.0;
    double meshCompletionMs = 0.0;
    double meshBuildMs = 0.0;
    double meshUploadMs = 0.0;
    double meshQueueMs = 0.0;
    int loadedChunks = 0;
    int pendingTerrainJobs = 0;
    int pendingMeshJobs = 0;
    int pendingMeshUploads = 0;
    int queuedMeshBuilds = 0;
    int loadedEntities = 0;
    int connectedPlayers = 0;
    EngineDiagnosticsRollingStats rolling;
};

class EngineDiagnostics {
public:
    void beginFrame(double frameDeltaMs);
    void recordPhase(EnginePhase phase, double elapsedMs);
    void setChunkGauges(int loadedChunks, int pendingTerrainJobs, int pendingMeshJobs, int pendingMeshUploads, int queuedMeshBuilds);
    void setEntityGauge(int loadedEntities);
    void setPlayerGauge(int connectedPlayers);
    EngineDiagnosticsSnapshot snapshot() const;

private:
    void storeCompletedFrame();
    EngineDiagnosticsRollingStats calculateRollingStats() const;

    EngineDiagnosticsSnapshot snapshot_{};
    std::array<EngineDiagnosticsSnapshot, EngineDiagnosticsRollingStats::WindowSize> history_{};
    std::size_t historyStart_ = 0;
    std::size_t historyCount_ = 0;
};

class ScopedEngineTimer {
public:
    ScopedEngineTimer(EngineDiagnostics& diagnostics, EnginePhase phase);
    ScopedEngineTimer(const ScopedEngineTimer&) = delete;
    ScopedEngineTimer& operator=(const ScopedEngineTimer&) = delete;
    ~ScopedEngineTimer();

private:
    EngineDiagnostics& diagnostics_;
    EnginePhase phase_;
    std::chrono::steady_clock::time_point start_;
};

}  // namespace voxel
