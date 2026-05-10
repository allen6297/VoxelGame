#pragma once

#include <cstddef>
#include <string>

#include "common/Math.hpp"
#include "render/RenderBackend.hpp"
#include "render/RenderResource.hpp"
#include "render/Renderer.hpp"

struct GLFWwindow;

namespace voxel {
struct ChunkMesh;
class TextureManager;

class DiligentRenderBackend : public IRenderBackend {
public:
    DiligentRenderBackend();
    ~DiligentRenderBackend() override;

    DiligentRenderBackend(const DiligentRenderBackend&) = delete;
    DiligentRenderBackend& operator=(const DiligentRenderBackend&) = delete;

    // ---- Diligent-specific lifecycle (not in IRenderBackend) ----------------
    bool available() const;
    void initialize(GLFWwindow* window, int width, int height);

    // ---- IRenderBackend -----------------------------------------------------
    const char* name() const override;

    void beginFrame(int width, int height, const Color& clearColor) const override;
    void endFrame() const override;
    void beginWorldOverlayState() const override {}
    void endWorldOverlayState() const override {}
    void beginPreviewState() const override {}
    void endPreviewState() const override {}
    RenderViewport currentViewport() const override;
    void beginPreviewViewport(int x, int y, int width, int height, const Color& clearColor) const override {}
    void restoreViewport(const RenderViewport& viewport) const override {}
    void setPerspective(float fovYDegrees, float aspect, float nearPlane, float farPlane) const override;
    void applyCameraView(const Vec3& eye, const Vec3& lookDirection) const override;
    void setBlockPreviewCamera(float orthoExtent, const Vec3& center) const override {}
    RenderTextureHandle createTexture2D(int width, int height, int channelCount, const unsigned char* pixels) const override;
    void destroyTexture(RenderTextureHandle& texture) const override;
    void uploadChunkMesh(ChunkMesh& mesh) const override;
    bool uploadChunkMeshSurface(ChunkMesh& mesh, std::size_t surfaceIndex) const override;
    void destroyChunkMesh(ChunkMesh& mesh) const override;
    void renderMesh(const ChunkMesh& mesh, const TextureManager& textures) const override;
    void renderMeshWireframe(const ChunkMesh& mesh, bool translucentOnly = false) const override {}
    void drawHoveredFaceOverlay(const RaycastHit& hit) const override {}
    void drawCrosshair(int width, int height) const override {}
    void drawText(const std::string& text, float x, float y, float scale) const override {}
    void drawTextBillboard(const std::string& text, const Vec3& pos, float scale) const override {}
    void drawPlayerModel(const Vec3& pos, float yaw, float pitch) const override {}
    void drawItemEntityMarker(const Vec3& pos, float age) const override {}
    void drawDebugOverlay(int width, int height, const DebugOverlayData& data) const override {}
    void drawInventoryBar(int width, int height, const Inventory& inventory) const override {}

private:
    RenderBufferHandle createVertexBuffer(std::size_t byteCount, const void* data) const;
    void destroyBuffer(RenderBufferHandle& buffer) const;

    struct Impl;
    Impl* impl_ = nullptr;
};

}  // namespace voxel
