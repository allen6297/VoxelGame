#pragma once

#include <string>
#include <vector>

#include "data/GameData.hpp"
#include "render/ModelManager.hpp"
#include "render/Renderer.hpp"
#include "world/World.hpp"

namespace voxel {
struct MeshVertex {
    Vec3 position;
    Vec3 normal;
    Color color;
    float u = 0.0f;
    float v = 0.0f;
};

struct MeshSurface {
    std::string albedoTexturePath;
    std::string emissiveTexturePath;
    bool translucent = false;
    float opacity = 1.0f;
    std::vector<MeshVertex> vertices; // cleared after GPU upload
    unsigned int vboId = 0;
    int vertexCount = 0;
};

struct ChunkMesh {
    ChunkCoord coord {};
    std::vector<MeshSurface> surfaces;
    int solidBlocks = 0;
    int visibleFaces = 0;
    int triangleCount = 0;
};

ChunkMesh buildChunkMesh(const Chunk* neighbors[27], const ChunkCoord& coord, const GameData& gameData, const ModelManager& modelManager);
void uploadChunkMesh(ChunkMesh& mesh);
void destroyChunkMesh(ChunkMesh& mesh);
}  // namespace voxel
