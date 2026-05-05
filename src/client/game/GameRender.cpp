#include "game/Game.hpp"
#include "common/Entity.hpp"

namespace voxel {
namespace {

void renderItemEntity(const ItemEntity& item, const GameData& gameData, const TextureManager& textureManager) {
    (void)gameData;
    (void)textureManager;
    glPushMatrix();
    glTranslatef(item.position.x, item.position.y, item.position.z);
    
    // Floating effect
    float hover = std::sin(item.age * 2.0f) * 0.1f;
    glTranslatef(0, hover, 0);
    glRotatef(item.age * 45.0f, 0, 1, 0);
    glScalef(0.25f, 0.25f, 0.25f);

    glColor3f(1, 1, 1);
    glDisable(GL_TEXTURE_2D);
    
    // Draw a simple cube for the item
    glBegin(GL_QUADS);
    // front
    glVertex3f(-0.5, -0.5,  0.5); glVertex3f( 0.5, -0.5,  0.5);
    glVertex3f( 0.5,  0.5,  0.5); glVertex3f(-0.5,  0.5,  0.5);
    // back
    glVertex3f(-0.5, -0.5, -0.5); glVertex3f(-0.5,  0.5, -0.5);
    glVertex3f( 0.5,  0.5, -0.5); glVertex3f( 0.5, -0.5, -0.5);
    // top
    glVertex3f(-0.5,  0.5, -0.5); glVertex3f(-0.5,  0.5,  0.5);
    glVertex3f( 0.5,  0.5,  0.5); glVertex3f( 0.5,  0.5, -0.5);
    // bottom
    glVertex3f(-0.5, -0.5, -0.5); glVertex3f( 0.5, -0.5, -0.5);
    glVertex3f( 0.5, -0.5,  0.5); glVertex3f(-0.5, -0.5,  0.5);
    // right
    glVertex3f( 0.5, -0.5, -0.5); glVertex3f( 0.5,  0.5, -0.5);
    glVertex3f( 0.5,  0.5,  0.5); glVertex3f( 0.5, -0.5,  0.5);
    // left
    glVertex3f(-0.5, -0.5, -0.5); glVertex3f(-0.5, -0.5,  0.5);
    glVertex3f(-0.5,  0.5,  0.5); glVertex3f(-0.5,  0.5, -0.5);
    glEnd();

    glEnable(GL_TEXTURE_2D);
    glPopMatrix();
}

void renderRemotePlayerMarker(const RemotePlayerState& player) {
    // Nameplate
    drawTextBillboard(player.name, {player.position.x, player.position.y + 2.0f, player.position.z}, 1.5f);

    glPushAttrib(GL_ENABLE_BIT | GL_CURRENT_BIT);
    glDisable(GL_TEXTURE_2D);
    
    drawPlayerModel(player.position, player.yaw, player.pitch);

    glPopAttrib();
}

void renderBlockPreviewMesh(
    const GameData& gameData,
    ModelManager& modelManager,
    TextureManager& textureManager,
    std::unordered_map<std::string, ChunkMesh>& iconMeshes,
    int fbWidth,
    int fbHeight,
    const std::string& blockId,
    const int vx,
    const int vyScreen,
    const int vw,
    const int vh
) {
    const BlockDefinition* block = findBlockDefinition(gameData, blockId);
    if (!block) return;

    if (!iconMeshes.contains(block->id)) {
        Chunk chunk;
        chunk.blocks[1][1][1] = runtimeIdForBlock(gameData, block->id);
        
        const Chunk* neighbors[27];
        for (int i = 0; i < 27; ++i) neighbors[i] = nullptr;
        neighbors[13] = &chunk;

        ChunkMesh mesh = buildChunkMesh(neighbors, {0, 0, 0}, gameData, modelManager);
        uploadChunkMesh(mesh);
        iconMeshes.emplace(block->id, std::move(mesh));
    }
    const ChunkMesh& iconMesh = iconMeshes.at(block->id);
    if (iconMesh.surfaces.empty()) return;

    const int vyGl = fbHeight - vyScreen - vh;
    glScissor(vx, vyGl, vw, vh);
    glViewport(vx, vyGl, vw, vh);

    glClearColor(0.84f, 0.82f, 0.80f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    float cx = 1.5f, cy = 1.5f, cz = 1.5f;
    float orthoExt = 0.95f;

    if (const BlockModel* model = modelManager.get(block->modelPath);
        model && !model->elements.empty()) {
        float bmin[3] = {16, 16, 16};
        float bmax[3] = {0, 0, 0};
        for (const auto& el : model->elements) {
            for (int a = 0; a < 3; ++a) {
                bmin[a] = std::min(bmin[a], el.from[a]);
                bmax[a] = std::max(bmax[a], el.to[a]);
            }
        }

        cx = 1.0f + (bmin[0] + bmax[0]) / 32.0f;
        cy = 1.0f + (bmin[1] + bmax[1]) / 32.0f;
        cz = 1.0f + (bmin[2] + bmax[2]) / 32.0f;

        const float hx = (bmax[0] - bmin[0]) / 32.0f;
        const float hy = (bmax[1] - bmin[1]) / 32.0f;
        const float hz = (bmax[2] - bmin[2]) / 32.0f;

        static constexpr float kC45 = 0.70711f;
        static constexpr float kC28 = 0.88295f;
        static constexpr float kS28 = 0.46947f;

        float maxE = 0.0f;
        for (int sx : {-1, 1}) for (int sy : {-1, 1}) for (int sz : {-1, 1}) {
            const float lx = sx * hx, ly = sy * hy, lz = sz * hz;
            const float scrX = (lx - lz) * kC45;
            const float scrY = ly * kC28 - (lx + lz) * kC45 * kS28;
            maxE = std::max(maxE, std::max(std::abs(scrX), std::abs(scrY)));
        }
        orthoExt = std::max(maxE * 1.18f, 0.05f);
    }

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(-orthoExt, orthoExt, -orthoExt, orthoExt, 0.1, 20.0);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glTranslatef(0.0f, -0.05f, -5.0f);
    glRotatef(28.0f, 1.0f, 0.0f, 0.0f);
    glRotatef(-45.0f, 0.0f, 1.0f, 0.0f);
    glTranslatef(-cx, -cy, -cz);

    renderMesh(iconMesh, textureManager);
}

}  // namespace

void Game::render(const int framebufferWidth, const int framebufferHeight) const {
    const float aspect = framebufferHeight == 0 ? 1.0f :
        static_cast<float>(framebufferWidth) / static_cast<float>(framebufferHeight);

    glViewport(0, 0, framebufferWidth, framebufferHeight);
    
    // Clear color based on world time
    const float time = static_cast<float>(simulation_.time());
    const float cycle = std::sin(time * 0.05f) * 0.5f + 0.5f; // simple 0-1 cycle
    glClearColor(0.58f * cycle, 0.78f * cycle, 0.98f * cycle, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    setPerspective(70.0f, aspect, 0.1f, 200.0f);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    const Vec3 eye = getEyePosition(player_);
    applyCameraView(eye, getLookDirection(player_));

    const int px = static_cast<int>(std::floor(player_.position.x));
    const int py = std::clamp(static_cast<int>(std::floor(player_.position.y)), 0, kWorldY - 1);
    const int pz = static_cast<int>(std::floor(player_.position.z));
    const ChunkCoord playerChunk = worldToChunkCoord(px, py, pz);
    const Vec3 lookDir = getLookDirection(player_);
    const Frustum frustum = calculateFrustum(eye, lookDir, 70.0f, aspect, 0.1f, 200.0f);

    for (const auto& [coord, mesh] : meshes_) {
        if (mesh.triangleCount > 0) {
            const Vec3 min {static_cast<float>(coord.x * kChunkX), static_cast<float>(coord.y * kChunkY), static_cast<float>(coord.z * kChunkZ)};
            const Vec3 max {min.x + kChunkX, min.y + kChunkY, min.z + kChunkZ};
            if (frustum.isBoxVisible(min, max)) {
                renderMesh(mesh, textureManager_);
            }
        }
    }
    if (network_ != nullptr) {
        for (const auto& [id, remotePlayer] : network_->remotePlayers()) {
            (void) id;
            renderRemotePlayerMarker(remotePlayer);
        }
    }

    for (const auto& entity : simulation_.entities()) {
        if (entity->type == EntityType::Item) {
            renderItemEntity(static_cast<const ItemEntity&>(*entity), gameData_, textureManager_);
        }
    }
    if (const auto it = meshes_.find(playerChunk); it != meshes_.end()) {
        renderMeshWireframe(it->second);
    }

    drawCrosshair(framebufferWidth, framebufferHeight);
    if (currentHit_.has_value()) {
        drawHoveredFaceOverlay(*currentHit_);
    }

    const float wx = player_.position.x;
    const float wz = player_.position.z;
    const ClimatePoint climate = simulation_.terrainGenerator().sampleClimateAt(wx, wz);
    const BiomeDefinition* biome = simulation_.terrainGenerator().selectBiomeAt(wx, wz, gameData_.biomes);
    (void) climate;
    (void) biome;
}

DebugOverlayData Game::getDebugData() const {
    const int px = static_cast<int>(std::floor(player_.position.x));
    const int py = std::clamp(static_cast<int>(std::floor(player_.position.y)), 0, kWorldY - 1);
    const int pz = static_cast<int>(std::floor(player_.position.z));
    const ChunkCoord playerChunk = worldToChunkCoord(px, py, pz);

    static const ChunkMesh kEmptyMesh {};
    const auto meshIt = meshes_.find(playerChunk);
    const ChunkMesh& activeMesh = (meshIt != meshes_.end()) ? meshIt->second : kEmptyMesh;

    const float wx = player_.position.x;
    const float wz = player_.position.z;
    const ClimatePoint climate = simulation_.terrainGenerator().sampleClimateAt(wx, wz);
    const BiomeDefinition* biome = simulation_.terrainGenerator().selectBiomeAt(wx, wz, gameData_.biomes);

    return {
        player_.position,
        currentHit_.has_value() ? std::optional<Int3>(currentHit_->block) : std::nullopt,
        fps_,
        frameTimeMs_,
        playerChunk.x,
        playerChunk.y,
        playerChunk.z,
        static_cast<int>(meshes_.size()),
        activeMesh.solidBlocks,
        activeMesh.visibleFaces,
        activeMesh.triangleCount,
        biome ? biome->name : "",
        climate.temperature,
        climate.humidity,
        climate.rainfall,
        climate.elevation,
        climate.drainage,
        climate.waterTable
    };
}

void Game::renderHotbarIcons(int fbWidth, int fbHeight) {
    constexpr float kSlotOuterDp = 70.0f;
    constexpr float kGapDp       = 8.0f;
    constexpr float kBorderDp    = 3.0f;
    constexpr float kBottomOffDp = 24.0f;

    float scaleX = 1.0f;
    float scaleY = 1.0f;
    if (GLFWwindow* window = glfwGetCurrentContext()) {
        int windowWidth = 0;
        int windowHeight = 0;
        glfwGetWindowSize(window, &windowWidth, &windowHeight);
        if (windowWidth > 0) scaleX = static_cast<float>(fbWidth) / static_cast<float>(windowWidth);
        if (windowHeight > 0) scaleY = static_cast<float>(fbHeight) / static_cast<float>(windowHeight);
    }

    const float kSlotOuter = kSlotOuterDp * scaleX;
    const float kGap = kGapDp * scaleX;
    const float kBorder = kBorderDp * scaleX;
    const float kBottomOff = kBottomOffDp * scaleY;
    const float kNudgeX = scaleX * 4.0f;
    const float kNudgeY = scaleY * 4.0f;
    const float kStep = kSlotOuter + kGap;
    const float kContainerW = kSlotOuter * static_cast<float>(kInventorySlots)
        + kGap * static_cast<float>(kInventorySlots - 1);

    const float containerLeft = static_cast<float>(fbWidth) * 0.5f - kContainerW * 0.5f;
    const float slotTop = static_cast<float>(fbHeight) - kBottomOff - kSlotOuter;
    const float inner = kSlotOuter - kBorder * 2.0f;

    glPushAttrib(GL_ALL_ATTRIB_BITS);
    GLint savedVP[4];
    glGetIntegerv(GL_VIEWPORT, savedVP);

    glEnable(GL_SCISSOR_TEST);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);

    for (int i = 0; i < kInventorySlots; ++i) {
        const int vx = static_cast<int>(containerLeft + static_cast<float>(i) * kStep + kBorder - kNudgeX);
        const int vyScreen = static_cast<int>(slotTop + kBorder - kNudgeY);
        const int vw = static_cast<int>(inner);
        const int vh = static_cast<int>(inner);
        const int vyGl = fbHeight - vyScreen - vh + 2;

        glScissor(vx, vyGl, vw, vh + 1);
        glViewport(vx, vyGl, vw, vh + 1);

        glClearColor(0.84f, 0.82f, 0.80f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        const auto& slot = player_.inventory.slots[static_cast<std::size_t>(i)];
        if (slot.count == 0 || slot.itemId.empty()) continue;

        const BlockDefinition* block = nullptr;
        if (const auto* item = findItemDefinition(gameData_, slot.itemId)) {
            if (item->placeableBlock.has_value()) {
                block = findBlockDefinition(gameData_, *item->placeableBlock);
            }
        }
        if (!block) block = findBlockDefinition(gameData_, slot.itemId);
        if (!block) continue;
        renderBlockPreviewMesh(gameData_, modelManager_, textureManager_, iconMeshes_,
                               fbWidth, fbHeight, block->id, vx, vyScreen, vw, vh + 1);
    }

    glDisable(GL_SCISSOR_TEST);
    glViewport(savedVP[0], savedVP[1], savedVP[2], savedVP[3]);
    glPopAttrib();
}

void Game::renderBlockPreview(const int framebufferWidth, const int framebufferHeight,
                              const std::string& blockId, const int x, const int y,
                              const int width, const int height) {
    glPushAttrib(GL_ALL_ATTRIB_BITS);
    GLint savedVP[4];
    glGetIntegerv(GL_VIEWPORT, savedVP);

    glEnable(GL_SCISSOR_TEST);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);

    renderBlockPreviewMesh(gameData_, modelManager_, textureManager_, iconMeshes_,
                           framebufferWidth, framebufferHeight, blockId, x, y, width, height);

    glDisable(GL_SCISSOR_TEST);
    glViewport(savedVP[0], savedVP[1], savedVP[2], savedVP[3]);
    glPopAttrib();
}

}  // namespace voxel
