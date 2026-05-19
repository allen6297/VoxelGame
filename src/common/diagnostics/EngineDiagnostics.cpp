#include "diagnostics/EngineDiagnostics.hpp"

namespace voxel {
namespace {

double toMilliseconds(const std::chrono::steady_clock::duration duration) {
    return std::chrono::duration<double, std::milli>(duration).count();
}

void accumulateMetric(
    EngineDiagnosticsMetricStats& stats,
    const double value,
    const bool firstSample
) {
    stats.average += value;
    if (firstSample) {
        stats.minimum = value;
        stats.maximum = value;
        return;
    }
    if (value < stats.minimum) {
        stats.minimum = value;
    }
    if (value > stats.maximum) {
        stats.maximum = value;
    }
}

void finishMetric(EngineDiagnosticsMetricStats& stats, const std::size_t sampleCount) {
    if (sampleCount == 0) {
        stats = {};
        return;
    }
    stats.average /= static_cast<double>(sampleCount);
}

}  // namespace

void EngineDiagnostics::beginFrame(const double frameDeltaMs) {
    if (snapshot_.frameIndex > 0) {
        storeCompletedFrame();
    }

    ++snapshot_.frameIndex;
    snapshot_.frameDeltaMs = frameDeltaMs;
    snapshot_.updateMs = 0.0;
    snapshot_.renderMs = 0.0;
    snapshot_.networkMs = 0.0;
    snapshot_.simulationMs = 0.0;
    snapshot_.scriptsMs = 0.0;
    snapshot_.chunkMaintenanceMs = 0.0;
    snapshot_.terrainIntegrationMs = 0.0;
    snapshot_.meshCompletionMs = 0.0;
    snapshot_.meshBuildMs = 0.0;
    snapshot_.meshUploadMs = 0.0;
    snapshot_.meshQueueMs = 0.0;
    snapshot_.rolling = calculateRollingStats();
}

void EngineDiagnostics::recordPhase(const EnginePhase phase, const double elapsedMs) {
    switch (phase) {
    case EnginePhase::Update:
        snapshot_.updateMs += elapsedMs;
        break;
    case EnginePhase::Render:
        snapshot_.renderMs += elapsedMs;
        break;
    case EnginePhase::Network:
        snapshot_.networkMs += elapsedMs;
        break;
    case EnginePhase::Simulation:
        snapshot_.simulationMs += elapsedMs;
        break;
    case EnginePhase::Scripts:
        snapshot_.scriptsMs += elapsedMs;
        break;
    case EnginePhase::ChunkMaintenance:
        snapshot_.chunkMaintenanceMs += elapsedMs;
        break;
    case EnginePhase::TerrainIntegration:
        snapshot_.terrainIntegrationMs += elapsedMs;
        break;
    case EnginePhase::MeshCompletion:
        snapshot_.meshCompletionMs += elapsedMs;
        break;
    case EnginePhase::MeshBuild:
        snapshot_.meshBuildMs += elapsedMs;
        break;
    case EnginePhase::MeshUpload:
        snapshot_.meshUploadMs += elapsedMs;
        break;
    case EnginePhase::MeshQueue:
        snapshot_.meshQueueMs += elapsedMs;
        break;
    case EnginePhase::Count:
        break;
    }
}

void EngineDiagnostics::setChunkGauges(
    const int loadedChunks,
    const int pendingTerrainJobs,
    const int pendingMeshJobs,
    const int pendingMeshUploads,
    const int queuedMeshBuilds
) {
    snapshot_.loadedChunks = loadedChunks;
    snapshot_.pendingTerrainJobs = pendingTerrainJobs;
    snapshot_.pendingMeshJobs = pendingMeshJobs;
    snapshot_.pendingMeshUploads = pendingMeshUploads;
    snapshot_.queuedMeshBuilds = queuedMeshBuilds;
}

void EngineDiagnostics::setEntityGauge(const int loadedEntities) {
    snapshot_.loadedEntities = loadedEntities;
}

void EngineDiagnostics::setPlayerGauge(const int connectedPlayers) {
    snapshot_.connectedPlayers = connectedPlayers;
}

EngineDiagnosticsSnapshot EngineDiagnostics::snapshot() const {
    EngineDiagnosticsSnapshot result = snapshot_;
    result.rolling = calculateRollingStats();
    return result;
}

void EngineDiagnostics::storeCompletedFrame() {
    EngineDiagnosticsSnapshot completedFrame = snapshot_;
    completedFrame.rolling = {};

    const std::size_t index = (historyStart_ + historyCount_) % history_.size();
    history_[index] = completedFrame;
    if (historyCount_ < history_.size()) {
        ++historyCount_;
    } else {
        historyStart_ = (historyStart_ + 1) % history_.size();
    }
}

EngineDiagnosticsRollingStats EngineDiagnostics::calculateRollingStats() const {
    EngineDiagnosticsRollingStats stats;
    stats.sampleCount = historyCount_;
    if (historyCount_ == 0) {
        return stats;
    }

    for (std::size_t i = 0; i < historyCount_; ++i) {
        const EngineDiagnosticsSnapshot& frame = history_[(historyStart_ + i) % history_.size()];
        const bool firstSample = i == 0;
        accumulateMetric(stats.frameDeltaMs, frame.frameDeltaMs, firstSample);
        accumulateMetric(stats.updateMs, frame.updateMs, firstSample);
        accumulateMetric(stats.renderMs, frame.renderMs, firstSample);
        accumulateMetric(stats.networkMs, frame.networkMs, firstSample);
        accumulateMetric(stats.simulationMs, frame.simulationMs, firstSample);
        accumulateMetric(stats.scriptsMs, frame.scriptsMs, firstSample);
        accumulateMetric(stats.chunkMaintenanceMs, frame.chunkMaintenanceMs, firstSample);
        accumulateMetric(stats.terrainIntegrationMs, frame.terrainIntegrationMs, firstSample);
        accumulateMetric(stats.meshCompletionMs, frame.meshCompletionMs, firstSample);
        accumulateMetric(stats.meshBuildMs, frame.meshBuildMs, firstSample);
        accumulateMetric(stats.meshUploadMs, frame.meshUploadMs, firstSample);
        accumulateMetric(stats.meshQueueMs, frame.meshQueueMs, firstSample);
    }

    finishMetric(stats.frameDeltaMs, historyCount_);
    finishMetric(stats.updateMs, historyCount_);
    finishMetric(stats.renderMs, historyCount_);
    finishMetric(stats.networkMs, historyCount_);
    finishMetric(stats.simulationMs, historyCount_);
    finishMetric(stats.scriptsMs, historyCount_);
    finishMetric(stats.chunkMaintenanceMs, historyCount_);
    finishMetric(stats.terrainIntegrationMs, historyCount_);
    finishMetric(stats.meshCompletionMs, historyCount_);
    finishMetric(stats.meshBuildMs, historyCount_);
    finishMetric(stats.meshUploadMs, historyCount_);
    finishMetric(stats.meshQueueMs, historyCount_);
    return stats;
}

ScopedEngineTimer::ScopedEngineTimer(EngineDiagnostics& diagnostics, const EnginePhase phase)
    : diagnostics_(diagnostics),
      phase_(phase),
      start_(std::chrono::steady_clock::now()) {}

ScopedEngineTimer::~ScopedEngineTimer() {
    diagnostics_.recordPhase(phase_, toMilliseconds(std::chrono::steady_clock::now() - start_));
}

}  // namespace voxel
