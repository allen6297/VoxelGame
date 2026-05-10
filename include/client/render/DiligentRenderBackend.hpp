#pragma once

#include <cstddef>

#include "common/Math.hpp"
#include "render/RenderResource.hpp"
#include "render/Renderer.hpp"

struct GLFWwindow;

namespace voxel {
struct ChunkMesh;

class DiligentRenderBackend {
public:
    DiligentRenderBackend();
    ~DiligentRenderBackend();

    DiligentRenderBackend(const DiligentRenderBackend&) = delete;
    DiligentRenderBackend& operator=(const DiligentRenderBackend&) = delete;

    const char* name() const;
    bool available() const;
    void initialize(GLFWwindow* window, int width, int height);
    void resize(int width, int height);
    void clearFrame(const Color& clearColor);
    void present();
    void setPerspective(float fovYDegrees, float aspect, float nearPlane, float farPlane);
    void applyCameraView(const Vec3& eye, const Vec3& lookDirection);
    void renderMesh(const ChunkMesh& mesh);
    RenderBufferHandle createVertexBuffer(std::size_t byteCount, const void* data);
    void destroyBuffer(RenderBufferHandle& buffer);
    RenderTextureHandle createTexture2D(int width, int height, int channelCount, const unsigned char* pixels);
    void destroyTexture(RenderTextureHandle& texture);
    void uploadChunkMesh(ChunkMesh& mesh);
    bool uploadChunkMeshSurface(ChunkMesh& mesh, std::size_t surfaceIndex);
    void destroyChunkMesh(ChunkMesh& mesh);

private:
    struct Impl;
    Impl* impl_ = nullptr;
};

}  // namespace voxel
