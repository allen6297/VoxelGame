#include "render/Mesh.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>

#include "Math.hpp"
#include "render/ModelManager.hpp"
#include "world/Chunk.hpp"

namespace voxel {
namespace {

// ── Greedy mesh helpers ──────────────────────────────────────────────────────

struct MaskCell {
    std::uint16_t block = 0;
};

MeshSurface& surfaceForTexture(
    ChunkMesh& mesh,
    const BlockDefinition& block,
    const std::string& albedoPath
) {
    const std::string emissive = block.textures.emissive.value_or("");
    for (auto& surface : mesh.surfaces) {
        if (surface.albedoTexturePath   == albedoPath &&
            surface.emissiveTexturePath == emissive   &&
            surface.translucent         == block.translucent &&
            surface.opacity             == block.opacity) {
            return surface;
        }
    }
    mesh.surfaces.push_back({albedoPath, emissive, block.translucent, block.opacity, {}});
    return mesh.surfaces.back();
}

bool isFaceVisible(
    const GameData& gameData,
    const std::uint16_t currentBlock,
    const std::uint16_t neighborBlock
) {
    if (neighborBlock == 0) {
        return true;
    }

    const BlockDefinition* current  = findBlockDefinitionForBlockType(gameData, currentBlock);
    const BlockDefinition* neighbor = findBlockDefinitionForBlockType(gameData, neighborBlock);
    if (current == nullptr)  return false;
    if (neighbor == nullptr) return true;

    // Model blocks use non-cube geometry — never cull adjacent cube block faces
    if (neighbor->renderType == "model") return true;

    if (current->translucent) {
        return neighbor->id != current->id;
    }
    return !neighbor->solid || neighbor->translucent;
}

void appendTriangle(
    MeshSurface& surface,
    const Vec3& a, const Vec3& b, const Vec3& c,
    const Vec3& normal,
    const Color& color,
    const std::array<std::array<float, 2>, 3>& uv
) {
    surface.vertices.push_back({a, normal, color, uv[0][0], uv[0][1]});
    surface.vertices.push_back({b, normal, color, uv[1][0], uv[1][1]});
    surface.vertices.push_back({c, normal, color, uv[2][0], uv[2][1]});
}

float faceBrightness(const int axis, const int direction) {
    if (axis == 1) return direction > 0 ? 1.00f : 0.45f;
    if (axis == 2) return direction > 0 ? 0.78f : 0.60f;
    return direction > 0 ? 0.72f : 0.52f;
}

void appendQuad(
    ChunkMesh& mesh,
    const std::array<Vec3, 4>& quad,
    const Vec3& normal,
    const BlockDefinition& block,
    const std::string& albedoPath,
    const Color& tint,
    const float brightness,
    const float width,
    const float height
) {
    MeshSurface& surface = surfaceForTexture(mesh, block, albedoPath);
    const Color shaded {tint.r * brightness, tint.g * brightness, tint.b * brightness};
    appendTriangle(surface, quad[0], quad[1], quad[2], normal, shaded,
                   {{{0.0f, 0.0f}, {width, 0.0f}, {width, height}}});
    appendTriangle(surface, quad[0], quad[2], quad[3], normal, shaded,
                   {{{0.0f, 0.0f}, {width, height}, {0.0f, height}}});
    mesh.visibleFaces  += 1;
    mesh.triangleCount += 2;
}

void appendGreedyQuad(
    ChunkMesh& mesh,
    const Chunk& chunk,
    const int axis, const int direction,
    const int slice, const int startU, const int startV,
    const int width, const int height,
    const BlockDefinition& block
) {
    const int u = (axis + 1) % 3;
    const int v = (axis + 2) % 3;
    const ChunkCoord& coord = chunk.coord;

    std::array<float, 3> base {
        static_cast<float>(coord.x * kChunkX),
        static_cast<float>(coord.y * kChunkY),
        static_cast<float>(coord.z * kChunkZ)
    };
    base[axis] += static_cast<float>(direction > 0 ? slice + 1 : slice);
    base[u]    += static_cast<float>(startU);
    base[v]    += static_cast<float>(startV);

    std::array<float, 3> du {0.0f, 0.0f, 0.0f};
    du[u] = static_cast<float>(width);
    std::array<float, 3> dv {0.0f, 0.0f, 0.0f};
    dv[v] = static_cast<float>(height);

    const Vec3 p0 {base[0],               base[1],               base[2]};
    const Vec3 p1 {base[0]+du[0],         base[1]+du[1],         base[2]+du[2]};
    const Vec3 p2 {base[0]+du[0]+dv[0],   base[1]+du[1]+dv[1],   base[2]+du[2]+dv[2]};
    const Vec3 p3 {base[0]+dv[0],         base[1]+dv[1],         base[2]+dv[2]};

    Vec3 normal {0.0f, 0.0f, 0.0f};
    if      (axis == 0) normal.x = static_cast<float>(direction);
    else if (axis == 1) normal.y = static_cast<float>(direction);
    else                normal.z = static_cast<float>(direction);

    // Determine the chunk-local (lx, lz) for tint lookup.
    // u = (axis+1)%3, v = (axis+2)%3; startU is along u, startV along v.
    int lx, lz;
    if      (axis == 0) { lx = slice;  lz = startV; }
    else if (axis == 1) { lx = startV; lz = startU; }
    else                { lx = startU; lz = slice;  }
    lx = std::max(0, std::min(lx, kChunkX - 1));
    lz = std::max(0, std::min(lz, kChunkZ - 1));

    // Block color: use biome tint if block has a tintKey, else use block.color
    Color tint;
    if (block.tintKey) {
        const auto& tc = chunk.tintColors[lx][lz];
        tint = {tc[0], tc[1], tc[2]};
    } else {
        tint = {block.color[0], block.color[1], block.color[2]};
    }

    // Per-face albedo: prefer faceTextures from model, fall back to block.textures.albedo
    const char* faceName = nullptr;
    if      (axis == 0) faceName = (direction > 0) ? "east"  : "west";
    else if (axis == 1) faceName = (direction > 0) ? "up"    : "down";
    else                faceName = (direction > 0) ? "south" : "north";

    std::string albedoPath = block.textures.albedo.value_or("");
    if (const auto it = block.faceTextures.find(faceName); it != block.faceTextures.end()) {
        albedoPath = it->second;
    }

    if (direction > 0) {
        appendQuad(mesh, {p0, p1, p2, p3}, normal, block, albedoPath, tint,
                   faceBrightness(axis, direction),
                   static_cast<float>(width), static_cast<float>(height));
    } else {
        appendQuad(mesh, {p0, p3, p2, p1}, normal, block, albedoPath, tint,
                   faceBrightness(axis, direction),
                   static_cast<float>(height), static_cast<float>(width));
    }
}

// ── Model mesh helpers ───────────────────────────────────────────────────────

// Rotate a point around an axis (0=X,1=Y,2=Z) about an origin.
Vec3 rotateAroundAxis(const Vec3& p, const Vec3& origin, const int axis, const float angleDeg) {
    const float rad = toRadians(angleDeg);
    const float c   = std::cos(rad);
    const float s   = std::sin(rad);
    const float dx  = p.x - origin.x;
    const float dy  = p.y - origin.y;
    const float dz  = p.z - origin.z;
    float rx, ry, rz;
    if (axis == 0) {
        rx = dx;  ry = dy * c - dz * s;  rz = dy * s + dz * c;
    } else if (axis == 1) {
        rx = dx * c + dz * s;  ry = dy;  rz = -dx * s + dz * c;
    } else {
        rx = dx * c - dy * s;  ry = dx * s + dy * c;  rz = dz;
    }
    return {origin.x + rx, origin.y + ry, origin.z + rz};
}

// Rotate a direction vector (no origin offset).
Vec3 rotateDirection(const Vec3& dir, const int axis, const float angleDeg) {
    return rotateAroundAxis(dir, {0.0f, 0.0f, 0.0f}, axis, angleDeg);
}

MeshSurface& surfaceForModelFace(
    ChunkMesh& mesh,
    const std::string& texturePath,
    bool translucent,
    float opacity
) {
    for (auto& surface : mesh.surfaces) {
        if (surface.albedoTexturePath == texturePath &&
            surface.translucent       == translucent  &&
            surface.opacity           == opacity) {
            return surface;
        }
    }
    mesh.surfaces.push_back({texturePath, "", translucent, opacity, {}});
    return mesh.surfaces.back();
}

// Compute normalized (0-1) UV for one vertex of a face.
// v is the vertex in model space (0-16).  uv[] is the face's [u1,v1,u2,v2] in 0-16.
// Each face maps its two in-plane world axes to (u, v) following Minecraft convention:
//   north/south/east/west: u = horizontal axis, v = height (v1=top, v2=bottom)
//   up:  u = x, v = z (+z = v2)
//   down: u = x, v = z (-z = v2, i.e. fz side)
void computeFaceUV(
    const char* face,
    const Vec3& v,
    const float fx, const float fy, const float fz,
    const float tx, const float ty, const float tz,
    const float u1, const float v1, const float u2, const float v2,
    float& outU, float& outV
) {
    const float dx = tx - fx, dy = ty - fy, dz = tz - fz;
    switch (face[0]) {
        case 'n': // north (-z): u=+x, v=+y inverted
            outU = u1 + (v.x - fx) / dx * (u2 - u1);
            outV = v1 + (ty - v.y) / dy * (v2 - v1);
            break;
        case 's': // south (+z): u=-x (mirrored), v=+y inverted
            outU = u1 + (tx - v.x) / dx * (u2 - u1);
            outV = v1 + (ty - v.y) / dy * (v2 - v1);
            break;
        case 'e': // east (+x): u=+z, v=+y inverted
            outU = u1 + (v.z - fz) / dz * (u2 - u1);
            outV = v1 + (ty - v.y) / dy * (v2 - v1);
            break;
        case 'w': // west (-x): u=-z (mirrored), v=+y inverted
            outU = u1 + (tz - v.z) / dz * (u2 - u1);
            outV = v1 + (ty - v.y) / dy * (v2 - v1);
            break;
        case 'u': // up (+y): u=+x, v=+z
            outU = u1 + (v.x - fx) / dx * (u2 - u1);
            outV = v1 + (v.z - fz) / dz * (v2 - v1);
            break;
        default:  // down (-y): u=+x, v=-z
            outU = u1 + (v.x - fx) / dx * (u2 - u1);
            outV = v1 + (tz - v.z) / dz * (v2 - v1);
            break;
    }
}

void emitModelFace(
    ChunkMesh& mesh,
    const std::array<Vec3, 4>& quad,
    const Vec3& normal,
    const std::string& texturePath,
    const std::array<std::array<float, 2>, 4>& vertUVs,
    bool translucent,
    float opacity
) {
    MeshSurface& surface = surfaceForModelFace(mesh, texturePath, translucent, opacity);
    const Color white {1.0f, 1.0f, 1.0f};
    surface.vertices.push_back({quad[0], normal, white, vertUVs[0][0], vertUVs[0][1]});
    surface.vertices.push_back({quad[1], normal, white, vertUVs[1][0], vertUVs[1][1]});
    surface.vertices.push_back({quad[2], normal, white, vertUVs[2][0], vertUVs[2][1]});
    surface.vertices.push_back({quad[0], normal, white, vertUVs[0][0], vertUVs[0][1]});
    surface.vertices.push_back({quad[2], normal, white, vertUVs[2][0], vertUVs[2][1]});
    surface.vertices.push_back({quad[3], normal, white, vertUVs[3][0], vertUVs[3][1]});
    mesh.visibleFaces  += 1;
    mesh.triangleCount += 2;
}

// Facing direction → Y-axis rotation angle (degrees).
// Models are authored facing north; positive angles rotate CCW from above.
float facingAngle(const GameData& gameData, const std::uint16_t stateId) {
    const auto prop = getStateProperty(gameData, stateId, "facing");
    if (!prop.has_value() || !std::holds_alternative<std::string>(*prop)) {
        return 0.0f;
    }
    const std::string& facing = std::get<std::string>(*prop);
    if (facing == "south") return 180.0f;
    if (facing == "west")  return  90.0f;
    if (facing == "east")  return -90.0f;
    return 0.0f;  // north
}

void emitModelBlock(
    ChunkMesh& mesh,
    const BlockModel& model,
    const BlockDefinition& blockDef,
    const GameData& gameData,
    const std::uint16_t stateId,
    const float worldX, const float worldY, const float worldZ
) {
    static constexpr Vec3 kBlockCenter {8.0f, 8.0f, 8.0f};
    const float yRotation = facingAngle(gameData, stateId);

    for (const auto& element : model.elements) {
        const float fx = element.from[0], fy = element.from[1], fz = element.from[2];
        const float tx = element.to[0],   ty = element.to[1],   tz = element.to[2];

        // Face quads in 0-16 model space, matching the greedy mesh winding convention.
        struct FaceDesc {
            const char*          name;
            Vec3                 normal;
            std::array<Vec3, 4>  quad;
        };
        const std::array<FaceDesc, 6> faceDefs = {{
            {"east",  { 1, 0, 0}, {{{tx,fy,fz},{tx,ty,fz},{tx,ty,tz},{tx,fy,tz}}}},
            {"west",  {-1, 0, 0}, {{{fx,fy,fz},{fx,fy,tz},{fx,ty,tz},{fx,ty,fz}}}},
            {"up",    { 0, 1, 0}, {{{fx,ty,fz},{fx,ty,tz},{tx,ty,tz},{tx,ty,fz}}}},
            {"down",  { 0,-1, 0}, {{{fx,fy,fz},{tx,fy,fz},{tx,fy,tz},{fx,fy,tz}}}},
            {"south", { 0, 0, 1}, {{{fx,fy,tz},{tx,fy,tz},{tx,ty,tz},{fx,ty,tz}}}},
            {"north", { 0, 0,-1}, {{{fx,fy,fz},{fx,ty,fz},{tx,ty,fz},{tx,fy,fz}}}}
        }};

        for (const auto& fd : faceDefs) {
            const auto it = element.faces.find(fd.name);
            if (it == element.faces.end() || it->second.texture.empty()) {
                continue;
            }
            const ModelFace& modelFace = it->second;

            // Compute per-vertex UVs in model space (before any rotation/transform)
            // so that u/v correctly follow the face's geometric axes.
            const float u1 = modelFace.uv[0] / 16.0f, v1 = modelFace.uv[1] / 16.0f;
            const float u2 = modelFace.uv[2] / 16.0f, v2 = modelFace.uv[3] / 16.0f;
            std::array<std::array<float, 2>, 4> vertUVs;
            for (int i = 0; i < 4; ++i) {
                computeFaceUV(fd.name, fd.quad[i],
                              fx, fy, fz, tx, ty, tz,
                              u1, v1, u2, v2,
                              vertUVs[i][0], vertUVs[i][1]);
            }

            std::array<Vec3, 4> quad = fd.quad;
            Vec3 normal = fd.normal;

            // 1. Per-element rotation
            if (element.rotation.has_value()) {
                const auto& rot = *element.rotation;
                const Vec3 origin {rot.origin[0], rot.origin[1], rot.origin[2]};
                for (auto& v : quad) {
                    v = rotateAroundAxis(v, origin, rot.axis, rot.angle);
                }
                normal = rotateDirection(normal, rot.axis, rot.angle);
            }

            // 2. Block facing rotation (Y-axis, around block centre)
            if (yRotation != 0.0f) {
                for (auto& v : quad) {
                    v = rotateAroundAxis(v, kBlockCenter, 1, yRotation);
                }
                normal = rotateDirection(normal, 1, yRotation);
            }

            // 3. Scale 0-16 → 0-1, translate to world position
            for (auto& v : quad) {
                v.x = worldX + v.x / 16.0f;
                v.y = worldY + v.y / 16.0f;
                v.z = worldZ + v.z / 16.0f;
            }

            emitModelFace(mesh, quad, normal, modelFace.texture,
                          vertUVs, blockDef.translucent, blockDef.opacity);
        }
    }
}

}  // namespace

// ── Public API ───────────────────────────────────────────────────────────────

ChunkMesh buildChunkMesh(
    const Chunk* neighbors[27],
    const ChunkCoord& coord,
    const GameData& gameData,
    const ModelManager& modelManager
) {
    ChunkMesh mesh;
    mesh.coord = coord;

    const Chunk* chunkPtr = neighbors[13]; // Center chunk (1*9 + 1*3 + 1 = 13)
    if (!chunkPtr) {
        return mesh;
    }
    const Chunk& chunk = *chunkPtr;
    const ChunkBlocks& blocks = chunk.blocks;
    const int worldMinX = coord.x * kChunkX;
    const int worldMinY = coord.y * kChunkY;
    const int worldMinZ = coord.z * kChunkZ;

    auto getBlockLocal = [&](int x, int y, int z) -> std::uint16_t {
        if (x >= 0 && x < kChunkX && y >= 0 && y < kChunkY && z >= 0 && z < kChunkZ) {
            return blocks[x][y][z];
        }
        
        // Relative chunk index
        int cx = (x < 0) ? 0 : (x >= kChunkX ? 2 : 1);
        int cy = (y < 0) ? 0 : (y >= kChunkY ? 2 : 1);
        int cz = (z < 0) ? 0 : (z >= kChunkZ ? 2 : 1);
        const Chunk* neighbor = neighbors[cx * 9 + cy * 3 + cz];
        if (!neighbor) return 0;

        int lx = (x % kChunkX + kChunkX) % kChunkX;
        int ly = (y % kChunkY + kChunkY) % kChunkY;
        int lz = (z % kChunkZ + kChunkZ) % kChunkZ;
        return neighbor->blocks[lx][ly][lz];
    };

    for (int x = 0; x < kChunkX; ++x) {
        for (int y = 0; y < kChunkY; ++y) {
            for (int z = 0; z < kChunkZ; ++z) {
                if (blocks[x][y][z] != 0) {
                    mesh.solidBlocks += 1;
                }
            }
        }
    }

    if (mesh.solidBlocks == 0) {
        return mesh;
    }

    // ── Pass 1: greedy mesh for cube blocks ──────────────────────────────────
    constexpr std::array<int, 3> dims {kChunkX, kChunkY, kChunkZ};

    for (int axis = 0; axis < 3; ++axis) {
        const int u = (axis + 1) % 3;
        const int v = (axis + 2) % 3;
        std::vector<MaskCell> mask(static_cast<std::size_t>(dims[u] * dims[v]));

        for (int direction : {1, -1}) {
            for (int slice = 0; slice < dims[axis]; ++slice) {
                const bool neighborOutside = (direction ==  1 && slice == dims[axis] - 1) ||
                                             (direction == -1 && slice == 0);

                for (int row = 0; row < dims[v]; ++row) {
                    for (int col = 0; col < dims[u]; ++col) {
                        std::array<int, 3> local {0, 0, 0};
                        local[axis] = slice;
                        local[u]    = col;
                        local[v]    = row;

                        const std::uint16_t block = blocks[local[0]][local[1]][local[2]];
                        MaskCell& entry = mask[static_cast<std::size_t>(row * dims[u] + col)];
                        entry.block = 0;

                        if (block == 0) continue;

                        // Skip model blocks — they're handled in the model pass below
                        const BlockDefinition* def = findBlockDefinitionForBlockType(gameData, block);
                        if (def != nullptr && def->renderType == "model") continue;

                        std::uint16_t neighbor;
                        if (neighborOutside) {
                            neighbor = getBlockLocal(local[0] + (axis == 0 ? direction : 0),
                                                     local[1] + (axis == 1 ? direction : 0),
                                                     local[2] + (axis == 2 ? direction : 0));
                        } else {
                            std::array<int, 3> nl = local;
                            nl[axis] += direction;
                            neighbor = blocks[nl[0]][nl[1]][nl[2]];
                        }

                        if (isFaceVisible(gameData, block, neighbor)) {
                            entry.block = block;
                        }
                    }
                }

                for (int row = 0; row < dims[v]; ++row) {
                    for (int col = 0; col < dims[u];) {
                        const MaskCell cell = mask[static_cast<std::size_t>(row * dims[u] + col)];
                        if (cell.block == 0) { ++col; continue; }

                        int width = 1;
                        while (col + width < dims[u] &&
                               mask[static_cast<std::size_t>(row * dims[u] + col + width)].block == cell.block) {
                            ++width;
                        }

                        int height = 1;
                        bool canGrow = true;
                        while (row + height < dims[v] && canGrow) {
                            for (int k = 0; k < width; ++k) {
                                if (mask[static_cast<std::size_t>((row + height) * dims[u] + col + k)].block != cell.block) {
                                    canGrow = false;
                                    break;
                                }
                            }
                            if (canGrow) ++height;
                        }

                        const BlockDefinition* blockDef = findBlockDefinitionForBlockType(gameData, cell.block);
                        if (blockDef != nullptr) {
                            appendGreedyQuad(mesh, chunk, axis, direction, slice,
                                             col, row, width, height, *blockDef);
                        }

                        for (int dy = 0; dy < height; ++dy) {
                            for (int dx = 0; dx < width; ++dx) {
                                mask[static_cast<std::size_t>((row + dy) * dims[u] + col + dx)].block = 0;
                            }
                        }
                        col += width;
                    }
                }
            }
        }
    }

    // ── Pass 2: individual quads for model blocks ────────────────────────────
    for (int x = 0; x < kChunkX; ++x) {
        for (int y = 0; y < kChunkY; ++y) {
            for (int z = 0; z < kChunkZ; ++z) {
                const std::uint16_t stateId = blocks[x][y][z];
                if (stateId == 0) continue;

                const BlockDefinition* def = findBlockDefinitionForBlockType(gameData, stateId);
                const std::string* stateModelPath = modelPathForState(gameData, stateId);
                if (def == nullptr || def->renderType != "model" ||
                    ((stateModelPath == nullptr || stateModelPath->empty()) && def->modelPath.empty())) continue;
                const std::string& modelPath = (stateModelPath != nullptr) ? *stateModelPath : def->modelPath;
                const BlockModel* model = modelManager.get(modelPath);
                if (model == nullptr) continue;

                emitModelBlock(
                    mesh, *model, *def, gameData, stateId,
                    static_cast<float>(worldMinX + x),
                    static_cast<float>(worldMinY + y),
                    static_cast<float>(worldMinZ + z)
                );
            }
        }
    }

    return mesh;
}

}  // namespace voxel
