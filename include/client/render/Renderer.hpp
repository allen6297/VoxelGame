#pragma once

#include <cstdint>
#include <optional>
#include <string>

#include "data/GameData.hpp"
#include "diagnostics/EngineDiagnostics.hpp"
#include "Player.hpp"
#include "player/Inventory.hpp"
#include "world/World.hpp"

namespace voxel {
struct ChunkMesh;
class TextureManager;

struct Color {
    float r;
    float g;
    float b;
};

struct DebugOverlayData {
    Vec3 playerPosition;
    std::optional<Int3> targetedBlock;
    int fps = 0;
    float frameTimeMs = 0.0f;
    int chunkX = 0;
    int chunkY = 0;
    int chunkZ = 0;
    int loadedChunks = 0;
    int solidBlocks = 0;
    int visibleFaces = 0;
    int triangleCount = 0;

    // Biome debug
    std::string biomeName;
    float temperature = 0.0f;
    float humidity    = 0.0f;
    float rainfall    = 0.0f;
    float elevation   = 0.0f;
    float drainage    = 0.0f;
    float waterTable  = 0.0f;

    // Engine diagnostics
    std::uint64_t frameIndex = 0;
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
    int pendingTerrainJobs = 0;
    int pendingMeshJobs = 0;
    int pendingMeshUploads = 0;
    int queuedMeshBuilds = 0;
    int loadedEntities = 0;
    int connectedPlayers = 0;
    EngineDiagnosticsRollingStats profiler;
};

void setPerspective(float fovYDegrees, float aspect, float nearPlane, float farPlane);
void applyCameraView(const Vec3& eye, const Vec3& lookDirection);
void renderMesh(const ChunkMesh& mesh, const TextureManager& textures);
void renderMeshWireframe(const ChunkMesh& mesh, bool translucentOnly = false);
void drawHoveredFaceOverlay(const RaycastHit& hit);
void drawCrosshair(int width, int height);
void drawText(const std::string& text, float x, float y, float scale);
void drawTextBillboard(const std::string& text, const Vec3& pos, float scale);
void drawPlayerModel(const Vec3& pos, float yaw, float pitch);
void drawDebugOverlay(int width, int height, const DebugOverlayData& data);
void drawInventoryBar(int width, int height, const Inventory& inventory);
}  // namespace voxel
