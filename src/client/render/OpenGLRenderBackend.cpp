#include "render/OpenGLRenderBackend.hpp"

#include "render/Mesh.hpp"
#include "render/OpenGLCompat.hpp"

namespace voxel {
const char* OpenGLRenderBackend::name() const {
    return "OpenGL";
}

void OpenGLRenderBackend::beginFrame(const int width, const int height, const Color& clearColor) const {
    glViewport(0, 0, width, height);
    glClearColor(clearColor.r, clearColor.g, clearColor.b, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

void OpenGLRenderBackend::endFrame() const {
}

void OpenGLRenderBackend::beginWorldOverlayState() const {
    glPushAttrib(GL_ENABLE_BIT | GL_CURRENT_BIT);
    glDisable(GL_TEXTURE_2D);
}

void OpenGLRenderBackend::endWorldOverlayState() const {
    glPopAttrib();
}

void OpenGLRenderBackend::beginPreviewState() const {
    glPushAttrib(GL_ALL_ATTRIB_BITS);
}

void OpenGLRenderBackend::endPreviewState() const {
    glPopAttrib();
}

RenderViewport OpenGLRenderBackend::currentViewport() const {
    GLint viewport[4] {};
    glGetIntegerv(GL_VIEWPORT, viewport);
    return {viewport[0], viewport[1], viewport[2], viewport[3]};
}

void OpenGLRenderBackend::beginPreviewViewport(
    const int x,
    const int y,
    const int width,
    const int height,
    const Color& clearColor
) const {
    glEnable(GL_SCISSOR_TEST);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glScissor(x, y, width, height);
    glViewport(x, y, width, height);
    glClearColor(clearColor.r, clearColor.g, clearColor.b, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

void OpenGLRenderBackend::restoreViewport(const RenderViewport& viewport) const {
    glDisable(GL_SCISSOR_TEST);
    glViewport(viewport.x, viewport.y, viewport.width, viewport.height);
}

void OpenGLRenderBackend::setPerspective(
    const float fovYDegrees,
    const float aspect,
    const float nearPlane,
    const float farPlane
) const {
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    voxel::setPerspective(fovYDegrees, aspect, nearPlane, farPlane);
}

void OpenGLRenderBackend::applyCameraView(const Vec3& eye, const Vec3& lookDirection) const {
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    voxel::applyCameraView(eye, lookDirection);
}

void OpenGLRenderBackend::setBlockPreviewCamera(const float orthoExtent, const Vec3& center) const {
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(-orthoExtent, orthoExtent, -orthoExtent, orthoExtent, 0.1, 20.0);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glTranslatef(0.0f, -0.05f, -5.0f);
    glRotatef(28.0f, 1.0f, 0.0f, 0.0f);
    glRotatef(-45.0f, 0.0f, 1.0f, 0.0f);
    glTranslatef(-center.x, -center.y, -center.z);
}

RenderTextureHandle OpenGLRenderBackend::createTexture2D(
    const int width,
    const int height,
    const int channelCount,
    const unsigned char* pixels
) const {
    RenderTextureHandle texture;
    glGenTextures(1, &texture.id);
    glBindTexture(GL_TEXTURE_2D, texture.id);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(
        GL_TEXTURE_2D,
        0,
        channelCount == 4 ? GL_RGBA : GL_RGB,
        width,
        height,
        0,
        channelCount == 4 ? GL_RGBA : GL_RGB,
        GL_UNSIGNED_BYTE,
        pixels
    );
    glBindTexture(GL_TEXTURE_2D, 0);
    return texture;
}

void OpenGLRenderBackend::destroyTexture(RenderTextureHandle& texture) const {
    if (texture.id != 0) {
        glDeleteTextures(1, &texture.id);
        texture.id = 0;
    }
}

void OpenGLRenderBackend::uploadChunkMesh(ChunkMesh& mesh) const {
    for (std::size_t surfaceIndex = 0; surfaceIndex < mesh.surfaces.size(); ++surfaceIndex) {
        uploadChunkMeshSurface(mesh, surfaceIndex);
    }
}

bool OpenGLRenderBackend::uploadChunkMeshSurface(ChunkMesh& mesh, const std::size_t surfaceIndex) const {
    if (surfaceIndex >= mesh.surfaces.size()) {
        return false;
    }

    MeshSurface& surface = mesh.surfaces[surfaceIndex];
    if (surface.vertices.empty()) {
        return false;
    }

    surface.vertexCount = static_cast<int>(surface.vertices.size());
    glGenBuffers(1, &surface.vertexBuffer.id);
    glBindBuffer(GL_ARRAY_BUFFER, surface.vertexBuffer.id);
    glBufferData(GL_ARRAY_BUFFER,
                 static_cast<GLsizeiptr>(surface.vertices.size() * sizeof(MeshVertex)),
                 surface.vertices.data(), GL_STATIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    surface.vertices.clear();
    surface.vertices.shrink_to_fit();
    return true;
}

void OpenGLRenderBackend::destroyChunkMesh(ChunkMesh& mesh) const {
    for (auto& surface : mesh.surfaces) {
        if (surface.vertexBuffer.id != 0) {
            glDeleteBuffers(1, &surface.vertexBuffer.id);
            surface.vertexBuffer.id = 0;
            surface.vertexCount = 0;
        }
    }
}

void OpenGLRenderBackend::renderMesh(const ChunkMesh& mesh, const TextureManager& textures) const {
    voxel::renderMesh(mesh, textures);
}

void OpenGLRenderBackend::renderMeshWireframe(const ChunkMesh& mesh, const bool translucentOnly) const {
    voxel::renderMeshWireframe(mesh, translucentOnly);
}

void OpenGLRenderBackend::drawHoveredFaceOverlay(const RaycastHit& hit) const {
    voxel::drawHoveredFaceOverlay(hit);
}

void OpenGLRenderBackend::drawCrosshair(const int width, const int height) const {
    voxel::drawCrosshair(width, height);
}

void OpenGLRenderBackend::drawText(const std::string& text, const float x, const float y, const float scale) const {
    voxel::drawText(text, x, y, scale);
}

void OpenGLRenderBackend::drawTextBillboard(const std::string& text, const Vec3& pos, const float scale) const {
    voxel::drawTextBillboard(text, pos, scale);
}

void OpenGLRenderBackend::drawPlayerModel(const Vec3& pos, const float yaw, const float pitch) const {
    voxel::drawPlayerModel(pos, yaw, pitch);
}

void OpenGLRenderBackend::drawItemEntityMarker(const Vec3& pos, const float age) const {
    glPushMatrix();
    glTranslatef(pos.x, pos.y, pos.z);

    const float hover = std::sin(age * 2.0f) * 0.1f;
    glTranslatef(0.0f, hover, 0.0f);
    glRotatef(age * 45.0f, 0.0f, 1.0f, 0.0f);
    glScalef(0.25f, 0.25f, 0.25f);

    glColor3f(1.0f, 1.0f, 1.0f);
    glDisable(GL_TEXTURE_2D);

    glBegin(GL_QUADS);
    glVertex3f(-0.5f, -0.5f,  0.5f); glVertex3f( 0.5f, -0.5f,  0.5f);
    glVertex3f( 0.5f,  0.5f,  0.5f); glVertex3f(-0.5f,  0.5f,  0.5f);

    glVertex3f(-0.5f, -0.5f, -0.5f); glVertex3f(-0.5f,  0.5f, -0.5f);
    glVertex3f( 0.5f,  0.5f, -0.5f); glVertex3f( 0.5f, -0.5f, -0.5f);

    glVertex3f(-0.5f,  0.5f, -0.5f); glVertex3f(-0.5f,  0.5f,  0.5f);
    glVertex3f( 0.5f,  0.5f,  0.5f); glVertex3f( 0.5f,  0.5f, -0.5f);

    glVertex3f(-0.5f, -0.5f, -0.5f); glVertex3f( 0.5f, -0.5f, -0.5f);
    glVertex3f( 0.5f, -0.5f,  0.5f); glVertex3f(-0.5f, -0.5f,  0.5f);

    glVertex3f( 0.5f, -0.5f, -0.5f); glVertex3f( 0.5f,  0.5f, -0.5f);
    glVertex3f( 0.5f,  0.5f,  0.5f); glVertex3f( 0.5f, -0.5f,  0.5f);

    glVertex3f(-0.5f, -0.5f, -0.5f); glVertex3f(-0.5f, -0.5f,  0.5f);
    glVertex3f(-0.5f,  0.5f,  0.5f); glVertex3f(-0.5f,  0.5f, -0.5f);
    glEnd();

    glEnable(GL_TEXTURE_2D);
    glPopMatrix();
}

void OpenGLRenderBackend::drawDebugOverlay(const int width, const int height, const DebugOverlayData& data) const {
    voxel::drawDebugOverlay(width, height, data);
}

void OpenGLRenderBackend::drawInventoryBar(const int width, const int height, const Inventory& inventory) const {
    voxel::drawInventoryBar(width, height, inventory);
}
}  // namespace voxel
