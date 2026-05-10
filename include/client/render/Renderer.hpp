#pragma once

#include <optional>
#include <string>

#include "data/GameData.hpp"
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
