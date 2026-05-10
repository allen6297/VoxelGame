#pragma once

#include <cstddef>
#include <string>

#include "render/RenderResource.hpp"
#include "render/Renderer.hpp"

namespace voxel {
struct ChunkMesh;
class TextureManager;

class IRenderBackend {
public:
    virtual ~IRenderBackend() = default;

    virtual const char* name() const = 0;

    virtual void beginFrame(int width, int height, const Color& clearColor) const = 0;
    virtual void endFrame() const = 0;
    virtual void beginWorldOverlayState() const = 0;
    virtual void endWorldOverlayState() const = 0;
    virtual void beginPreviewState() const = 0;
    virtual void endPreviewState() const = 0;
    virtual RenderViewport currentViewport() const = 0;
    virtual void beginPreviewViewport(
        int x,
        int y,
        int width,
        int height,
        const Color& clearColor
    ) const = 0;
    virtual void restoreViewport(const RenderViewport& viewport) const = 0;
    virtual void setPerspective(float fovYDegrees, float aspect, float nearPlane, float farPlane) const = 0;
    virtual void applyCameraView(const Vec3& eye, const Vec3& lookDirection) const = 0;
    virtual void setBlockPreviewCamera(float orthoExtent, const Vec3& center) const = 0;
    virtual RenderTextureHandle createTexture2D(
        int width,
        int height,
        int channelCount,
        const unsigned char* pixels
    ) const = 0;
    virtual void destroyTexture(RenderTextureHandle& texture) const = 0;
    virtual void uploadChunkMesh(ChunkMesh& mesh) const = 0;
    virtual bool uploadChunkMeshSurface(ChunkMesh& mesh, std::size_t surfaceIndex) const = 0;
    virtual void destroyChunkMesh(ChunkMesh& mesh) const = 0;
    virtual void renderMesh(const ChunkMesh& mesh, const TextureManager& textures) const = 0;
    virtual void renderMeshWireframe(const ChunkMesh& mesh, bool translucentOnly = false) const = 0;
    virtual void drawHoveredFaceOverlay(const RaycastHit& hit) const = 0;
    virtual void drawCrosshair(int width, int height) const = 0;
    virtual void drawText(const std::string& text, float x, float y, float scale) const = 0;
    virtual void drawTextBillboard(const std::string& text, const Vec3& pos, float scale) const = 0;
    virtual void drawPlayerModel(const Vec3& pos, float yaw, float pitch) const = 0;
    virtual void drawItemEntityMarker(const Vec3& pos, float age) const = 0;
    virtual void drawDebugOverlay(int width, int height, const DebugOverlayData& data) const = 0;
    virtual void drawInventoryBar(int width, int height, const Inventory& inventory) const = 0;
};
}  // namespace voxel
