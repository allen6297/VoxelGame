#include "render/Renderer.hpp"

#include <array>
#include <cctype>
#include <cstddef>
#include <memory>
#include <sstream>

#include "render/Mesh.hpp"
#include "render/OpenGLCompat.hpp"
#include "render/ShaderProgram.hpp"

namespace voxel {
namespace {
using Glyph = std::array<const char*, 7>;

struct FaceQuad {
    Vec3 normal;
    std::array<Vec3, 4> corners;
};

ShaderProgram& chunkShader() {
    static ShaderProgram shader;
    static bool initialized = false;
    if (!initialized) {
        shader.initialize();
        initialized = true;
    }
    return shader;
}

const Glyph& glyphFor(const char c) {
    static const Glyph unknown = {"00000", "00100", "00100", "00100", "00000", "00100", "00000"};
    static const Glyph space = {"000", "000", "000", "000", "000", "000", "000"};
    static const Glyph colon = {"0", "1", "0", "0", "1", "0", "0"};
    static const Glyph comma = {"000", "000", "000", "000", "010", "010", "100"};
    static const Glyph dash = {"000", "000", "000", "111", "000", "000", "000"};
    static const Glyph dot = {"0", "0", "0", "0", "0", "0", "1"};
    static const Glyph zero = {"01110", "10001", "10011", "10101", "11001", "10001", "01110"};
    static const Glyph one = {"00100", "01100", "00100", "00100", "00100", "00100", "01110"};
    static const Glyph two = {"01110", "10001", "00001", "00010", "00100", "01000", "11111"};
    static const Glyph three = {"11110", "00001", "00001", "01110", "00001", "00001", "11110"};
    static const Glyph four = {"00010", "00110", "01010", "10010", "11111", "00010", "00010"};
    static const Glyph five = {"11111", "10000", "10000", "11110", "00001", "00001", "11110"};
    static const Glyph six = {"01110", "10000", "10000", "11110", "10001", "10001", "01110"};
    static const Glyph seven = {"11111", "00001", "00010", "00100", "01000", "01000", "01000"};
    static const Glyph eight = {"01110", "10001", "10001", "01110", "10001", "10001", "01110"};
    static const Glyph nine = {"01110", "10001", "10001", "01111", "00001", "00001", "01110"};
    static const Glyph a = {"01110", "10001", "10001", "11111", "10001", "10001", "10001"};
    static const Glyph b = {"11110", "10001", "10001", "11110", "10001", "10001", "11110"};
    static const Glyph cGlyph = {"01110", "10001", "10000", "10000", "10000", "10001", "01110"};
    static const Glyph d = {"11110", "10001", "10001", "10001", "10001", "10001", "11110"};
    static const Glyph e = {"11111", "10000", "10000", "11110", "10000", "10000", "11111"};
    static const Glyph f = {"11111", "10000", "10000", "11110", "10000", "10000", "10000"};
    static const Glyph g = {"01110", "10001", "10000", "10111", "10001", "10001", "01110"};
    static const Glyph h = {"10001", "10001", "10001", "11111", "10001", "10001", "10001"};
    static const Glyph i = {"01110", "00100", "00100", "00100", "00100", "00100", "01110"};
    static const Glyph k = {"10001", "10010", "10100", "11000", "10100", "10010", "10001"};
    static const Glyph l = {"10000", "10000", "10000", "10000", "10000", "10000", "11111"};
    static const Glyph m = {"10001", "11011", "10101", "10101", "10001", "10001", "10001"};
    static const Glyph n = {"10001", "11001", "10101", "10011", "10001", "10001", "10001"};
    static const Glyph o = {"01110", "10001", "10001", "10001", "10001", "10001", "01110"};
    static const Glyph p = {"11110", "10001", "10001", "11110", "10000", "10000", "10000"};
    static const Glyph r = {"11110", "10001", "10001", "11110", "10100", "10010", "10001"};
    static const Glyph s = {"01111", "10000", "10000", "01110", "00001", "00001", "11110"};
    static const Glyph t = {"11111", "00100", "00100", "00100", "00100", "00100", "00100"};
    static const Glyph u = {"10001", "10001", "10001", "10001", "10001", "10001", "01110"};
    static const Glyph v = {"10001", "10001", "10001", "10001", "10001", "01010", "00100"};
    static const Glyph x = {"10001", "10001", "01010", "00100", "01010", "10001", "10001"};
    static const Glyph y = {"10001", "10001", "01010", "00100", "00100", "00100", "00100"};
    static const Glyph z = {"11111", "00001", "00010", "00100", "01000", "10000", "11111"};

    switch (std::toupper(static_cast<unsigned char>(c))) {
        case ' ': return space;
        case ':': return colon;
        case ',': return comma;
        case '-': return dash;
        case '.': return dot;
        case '0': return zero;
        case '1': return one;
        case '2': return two;
        case '3': return three;
        case '4': return four;
        case '5': return five;
        case '6': return six;
        case '7': return seven;
        case '8': return eight;
        case '9': return nine;
        case 'A': return a;
        case 'B': return b;
        case 'C': return cGlyph;
        case 'D': return d;
        case 'E': return e;
        case 'F': return f;
        case 'G': return g;
        case 'H': return h;
        case 'I': return i;
        case 'K': return k;
        case 'L': return l;
        case 'M': return m;
        case 'N': return n;
        case 'O': return o;
        case 'P': return p;
        case 'R': return r;
        case 'S': return s;
        case 'T': return t;
        case 'U': return u;
        case 'V': return v;
        case 'X': return x;
        case 'Y': return y;
        case 'Z': return z;
        default: return unknown;
    }
}

void drawGlyph(const char c, const float x, const float y, const float scale) {
    const Glyph& glyph = glyphFor(c);
    glBegin(GL_QUADS);
    for (int row = 0; row < static_cast<int>(glyph.size()); ++row) {
        const char* line = glyph[row];
        for (int col = 0; line[col] != '\0'; ++col) {
            if (line[col] != '1') {
                continue;
            }
            const float x0 = x + static_cast<float>(col) * scale;
            const float y0 = y + static_cast<float>(row) * scale;
            const float x1 = x0 + scale;
            const float y1 = y0 + scale;
            glVertex2f(x0, y0);
            glVertex2f(x1, y0);
            glVertex2f(x1, y1);
            glVertex2f(x0, y1);
        }
    }
    glEnd();
}

}  // namespace

void drawText(const std::string& text, const float x, const float y, const float scale) {
    float cursor = x;
    glColor3f(0.10f, 0.10f, 0.12f);
    for (const char c : text) {
        drawGlyph(c, cursor, y, scale);
        cursor += ((c == ':' || c == '.' || c == ',') ? 2.0f : 6.0f) * scale;
    }
}

void drawTextBillboard(const std::string& text, const Vec3& pos, const float scale) {
    GLfloat modelview[16];
    glGetFloatv(GL_MODELVIEW_MATRIX, modelview);

    // Billboarding: clear the rotation part of the modelview matrix
    // to keep the text facing the camera.
    for (int i = 0; i < 3; ++i) {
        for (int j = 0; j < 3; ++j) {
            modelview[i * 4 + j] = (i == j) ? 1.0f : 0.0f;
        }
    }

    glPushMatrix();
    glTranslatef(pos.x, pos.y, pos.z);
    glMultMatrixf(modelview);
    
    // Scale down for world space and flip Y for typical UI coords if needed,
    // but here we just want it readable.
    glScalef(0.015f * scale, -0.015f * scale, 0.015f * scale);
    
    // Center text horizontally (roughly 6 units per char)
    const float xOffset = -static_cast<float>(text.size()) * 6.0f * 0.5f;
    drawText(text, xOffset, 0.0f, 1.0f);
    
    glPopMatrix();
}

void drawPlayerModel(const Vec3& pos, const float yaw, const float pitch) {
    glPushMatrix();
    glTranslatef(pos.x, pos.y, pos.z);
    glRotatef(-yaw, 0.0f, 1.0f, 0.0f);
    // pitch could be used for head rotation later

    // Simple boxy body
    glColor3f(0.2f, 0.4f, 0.8f);
    glBegin(GL_QUADS);
    // Front
    glVertex3f(-0.3f, 0.0f,  0.2f); glVertex3f( 0.3f, 0.0f,  0.2f);
    glVertex3f( 0.3f, 1.4f,  0.2f); glVertex3f(-0.3f, 1.4f,  0.2f);
    // Back
    glVertex3f(-0.3f, 0.0f, -0.2f); glVertex3f(-0.3f, 1.4f, -0.2f);
    glVertex3f( 0.3f, 1.4f, -0.2f); glVertex3f( 0.3f, 0.0f, -0.2f);
    // Sides ... (truncated for brevity but I'll add them all)
    glVertex3f(-0.3f, 0.0f, -0.2f); glVertex3f(-0.3f, 0.0f,  0.2f);
    glVertex3f(-0.3f, 1.4f,  0.2f); glVertex3f(-0.3f, 1.4f, -0.2f);
    glVertex3f( 0.3f, 0.0f, -0.2f); glVertex3f( 0.3f, 1.4f, -0.2f);
    glVertex3f( 0.3f, 1.4f,  0.2f); glVertex3f( 0.3f, 0.0f,  0.2f);
    // Top/Bottom
    glVertex3f(-0.3f, 1.4f, -0.2f); glVertex3f(-0.3f, 1.4f,  0.2f);
    glVertex3f( 0.3f, 1.4f,  0.2f); glVertex3f( 0.3f, 1.4f, -0.2f);
    glEnd();

    // Head
    glTranslatef(0.0f, 1.4f, 0.0f);
    glRotatef(-pitch, 1.0f, 0.0f, 0.0f);
    glColor3f(0.9f, 0.7f, 0.6f);
    glBegin(GL_QUADS);
    glVertex3f(-0.2f, 0.0f, -0.2f); glVertex3f( 0.2f, 0.0f, -0.2f);
    glVertex3f( 0.2f, 0.4f, -0.2f); glVertex3f(-0.2f, 0.4f, -0.2f);
    // (other faces)
    glVertex3f(-0.2f, 0.0f,  0.2f); glVertex3f(-0.2f, 0.4f,  0.2f);
    glVertex3f( 0.2f, 0.4f,  0.2f); glVertex3f( 0.2f, 0.0f,  0.2f);
    glVertex3f(-0.2f, 0.0f, -0.2f); glVertex3f(-0.2f, 0.0f,  0.2f);
    glVertex3f(-0.2f, 0.4f,  0.2f); glVertex3f(-0.2f, 0.4f, -0.2f);
    glVertex3f( 0.2f, 0.0f, -0.2f); glVertex3f( 0.2f, 0.4f, -0.2f);
    glVertex3f( 0.2f, 0.4f,  0.2f); glVertex3f( 0.2f, 0.0f,  0.2f);
    glVertex3f(-0.2f, 0.4f, -0.2f); glVertex3f( 0.2f, 0.4f, -0.2f);
    glVertex3f( 0.2f, 0.4f,  0.2f); glVertex3f(-0.2f, 0.4f,  0.2f);
    glEnd();

    glPopMatrix();
}

std::string formatFloat(const float value) {
    std::ostringstream stream;
    stream.setf(std::ios::fixed);
    stream.precision(2);
    stream << value;
    return stream.str();
}

std::optional<FaceQuad> hoveredFaceQuad(const RaycastHit& hit) {
    const float bx = static_cast<float>(hit.block.x);
    const float by = static_cast<float>(hit.block.y);
    const float bz = static_cast<float>(hit.block.z);
    constexpr float kInset = 0.0025f;

    // World-space bounds of the specific collision box that was hit
    const float minX = bx + hit.hitBox.minX;
    const float maxX = bx + hit.hitBox.maxX;
    const float minY = by + hit.hitBox.minY;
    const float maxY = by + hit.hitBox.maxY;
    const float minZ = bz + hit.hitBox.minZ;
    const float maxZ = bz + hit.hitBox.maxZ;

    const Vec3& n = hit.hitNormal;

    if (n.x < -0.5f) {
        return FaceQuad{n, {{{minX-kInset, minY, minZ}, {minX-kInset, minY, maxZ}, {minX-kInset, maxY, maxZ}, {minX-kInset, maxY, minZ}}}};
    }
    if (n.x > 0.5f) {
        return FaceQuad{n, {{{maxX+kInset, minY, maxZ}, {maxX+kInset, minY, minZ}, {maxX+kInset, maxY, minZ}, {maxX+kInset, maxY, maxZ}}}};
    }
    if (n.y < -0.5f) {
        return FaceQuad{n, {{{minX, minY-kInset, maxZ}, {maxX, minY-kInset, maxZ}, {maxX, minY-kInset, minZ}, {minX, minY-kInset, minZ}}}};
    }
    if (n.y > 0.5f) {
        return FaceQuad{n, {{{minX, maxY+kInset, minZ}, {maxX, maxY+kInset, minZ}, {maxX, maxY+kInset, maxZ}, {minX, maxY+kInset, maxZ}}}};
    }
    if (n.z < -0.5f) {
        return FaceQuad{n, {{{maxX, minY, minZ-kInset}, {minX, minY, minZ-kInset}, {minX, maxY, minZ-kInset}, {maxX, maxY, minZ-kInset}}}};
    }
    if (n.z > 0.5f) {
        return FaceQuad{n, {{{minX, minY, maxZ+kInset}, {maxX, minY, maxZ+kInset}, {maxX, maxY, maxZ+kInset}, {minX, maxY, maxZ+kInset}}}};
    }

    return std::nullopt;
}

void setPerspective(const float fovYDegrees, const float aspect, const float nearPlane, const float farPlane) {
    const float top = nearPlane * std::tan(toRadians(fovYDegrees) * 0.5f);
    const float right = top * aspect;
    glFrustum(-right, right, -top, top, nearPlane, farPlane);
}

void applyCameraView(const Vec3& eye, const Vec3& lookDirection) {
    const Vec3 forward = normalize(lookDirection);
    const Vec3 worldUp {0.0f, 1.0f, 0.0f};
    const Vec3 right = normalize(cross(forward, worldUp));
    const Vec3 up = cross(right, forward);

    const GLfloat viewMatrix[16] = {
        right.x, up.x, -forward.x, 0.0f,
        right.y, up.y, -forward.y, 0.0f,
        right.z, up.z, -forward.z, 0.0f,
        -dot(right, eye), -dot(up, eye), dot(forward, eye), 1.0f
    };

    glMultMatrixf(viewMatrix);
}

void renderMesh(const ChunkMesh& mesh, const TextureManager& textures) {
    ShaderProgram& shader = chunkShader();

    glEnableClientState(GL_VERTEX_ARRAY);
    glEnableClientState(GL_NORMAL_ARRAY);
    glEnableClientState(GL_COLOR_ARRAY);
    glEnableClientState(GL_TEXTURE_COORD_ARRAY);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    constexpr GLsizei kStride = static_cast<GLsizei>(sizeof(MeshVertex));

    for (const bool translucentPass : {false, true}) {
        if (translucentPass) {
            glDepthMask(GL_FALSE);
        }
        for (const auto& surface : mesh.surfaces) {
            if (surface.translucent != translucentPass || surface.vboId == 0 || surface.vertexCount == 0) {
                continue;
            }
            const TextureResource* albedo = textures.find(surface.albedoTexturePath);
            if (albedo == nullptr) {
                continue;
            }
            const TextureResource* emissive = nullptr;
            if (!surface.emissiveTexturePath.empty()) {
                emissive = textures.find(surface.emissiveTexturePath);
            }

            shader.useSurface(albedo, emissive, textures.blackFallback(), surface.opacity);

            glBindBuffer(GL_ARRAY_BUFFER, surface.vboId);
            glVertexPointer  (3, GL_FLOAT, kStride, reinterpret_cast<const void*>(offsetof(MeshVertex, position)));
            glNormalPointer  (   GL_FLOAT, kStride, reinterpret_cast<const void*>(offsetof(MeshVertex, normal)));
            glColorPointer   (3, GL_FLOAT, kStride, reinterpret_cast<const void*>(offsetof(MeshVertex, color)));
            glTexCoordPointer(2, GL_FLOAT, kStride, reinterpret_cast<const void*>(offsetof(MeshVertex, u)));
            glDrawArrays(GL_TRIANGLES, 0, surface.vertexCount);
        }
        if (translucentPass) {
            glDepthMask(GL_TRUE);
        }
    }

    shader.stop();
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glDisable(GL_BLEND);

    glDisableClientState(GL_TEXTURE_COORD_ARRAY);
    glDisableClientState(GL_COLOR_ARRAY);
    glDisableClientState(GL_NORMAL_ARRAY);
    glDisableClientState(GL_VERTEX_ARRAY);
}

void renderMeshWireframe(const ChunkMesh& mesh, const bool translucentOnly) {
    glDisable(GL_CULL_FACE);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_POLYGON_OFFSET_LINE);
    glPolygonOffset(-1.0f, -1.0f);
    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    glLineWidth(1.0f);
    glColor3f(0.08f, 0.08f, 0.08f);

    glEnableClientState(GL_VERTEX_ARRAY);
    for (const auto& surface : mesh.surfaces) {
        if ((translucentOnly && !surface.translucent) || surface.vboId == 0 || surface.vertexCount == 0) {
            continue;
        }
        glBindBuffer(GL_ARRAY_BUFFER, surface.vboId);
        glVertexPointer(3, GL_FLOAT, sizeof(MeshVertex),
                        reinterpret_cast<const void*>(offsetof(MeshVertex, position)));
        glDrawArrays(GL_TRIANGLES, 0, surface.vertexCount);
    }
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glDisableClientState(GL_VERTEX_ARRAY);

    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    glDisable(GL_POLYGON_OFFSET_LINE);
    glEnable(GL_CULL_FACE);
}

void drawHoveredFaceOverlay(const RaycastHit& hit) {
    const auto quad = hoveredFaceQuad(hit);
    if (!quad.has_value()) {
        return;
    }

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_TEXTURE_2D);
    glDisable(GL_CULL_FACE);
    glEnable(GL_POLYGON_OFFSET_FILL);
    glPolygonOffset(-1.0f, -1.0f);
    glColor4f(1.0f, 1.0f, 1.0f, 0.75f);

    glBegin(GL_QUADS);
    glNormal3f(quad->normal.x, quad->normal.y, quad->normal.z);
    glVertex3f(quad->corners[0].x, quad->corners[0].y, quad->corners[0].z);
    glVertex3f(quad->corners[1].x, quad->corners[1].y, quad->corners[1].z);
    glVertex3f(quad->corners[2].x, quad->corners[2].y, quad->corners[2].z);
    glVertex3f(quad->corners[3].x, quad->corners[3].y, quad->corners[3].z);
    glEnd();

    glDisable(GL_POLYGON_OFFSET_FILL);
    glEnable(GL_CULL_FACE);
    glDisable(GL_BLEND);
}

void drawCrosshair(const int width, const int height) {
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glOrtho(0.0, width, height, 0.0, -1.0, 1.0);

    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();

    glDisable(GL_DEPTH_TEST);
    glColor3f(0.05f, 0.05f, 0.05f);
    glLineWidth(2.0f);
    const float cx = static_cast<float>(width) * 0.5f;
    const float cy = static_cast<float>(height) * 0.5f;
    glBegin(GL_LINES);
    glVertex2f(cx - 8.0f, cy);
    glVertex2f(cx + 8.0f, cy);
    glVertex2f(cx, cy - 8.0f);
    glVertex2f(cx, cy + 8.0f);
    glEnd();
    glEnable(GL_DEPTH_TEST);

    glPopMatrix();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
}

void drawDebugOverlay(const int width, const int height, const DebugOverlayData& data) {
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glOrtho(0.0, width, height, 0.0, -1.0, 1.0);

    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();

    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);

    glColor4f(0.96f, 0.95f, 0.90f, 0.85f);
    glBegin(GL_QUADS);
    glVertex2f(12.0f,  12.0f);
    glVertex2f(360.0f, 12.0f);
    glVertex2f(360.0f, 246.0f);
    glVertex2f(12.0f,  246.0f);
    glEnd();

    drawText("DEBUG", 24.0f, 24.0f, 2.0f);
    drawText("FPS " + std::to_string(data.fps) + " " + formatFloat(data.frameTimeMs) + "MS", 150.0f, 24.0f, 2.0f);
    drawText("PLAYER " + formatFloat(data.playerPosition.x) + ", " + formatFloat(data.playerPosition.y) + ", " +
             formatFloat(data.playerPosition.z), 24.0f, 48.0f, 2.0f);

    if (data.targetedBlock.has_value()) {
        drawText("TARGET " + std::to_string(data.targetedBlock->x) + ", " +
                 std::to_string(data.targetedBlock->y) + ", " +
                 std::to_string(data.targetedBlock->z), 24.0f, 66.0f, 2.0f);
    } else {
        drawText("TARGET NONE", 24.0f, 66.0f, 2.0f);
    }

    drawText("CHUNK " + std::to_string(data.chunkX) + ", " + std::to_string(data.chunkY) + ", " +
             std::to_string(data.chunkZ), 24.0f, 84.0f, 2.0f);
    drawText("LOADED " + std::to_string(data.loadedChunks) + "  BLOCKS " + std::to_string(data.solidBlocks),
             24.0f, 102.0f, 2.0f);
    drawText("FACES " + std::to_string(data.visibleFaces) + "  TRIS " + std::to_string(data.triangleCount),
             24.0f, 120.0f, 2.0f);

    // Biome debug section
    drawText("BIOME " + (data.biomeName.empty() ? "NONE" : data.biomeName), 24.0f, 144.0f, 2.0f);
    drawText("TEMP "  + formatFloat(data.temperature) + "  HUM "  + formatFloat(data.humidity),
             24.0f, 162.0f, 2.0f);
    drawText("RAIN "  + formatFloat(data.rainfall)    + "  ELEV " + formatFloat(data.elevation),
             24.0f, 180.0f, 2.0f);
    drawText("DRAIN " + formatFloat(data.drainage)    + "  WTBL " + formatFloat(data.waterTable),
             24.0f, 198.0f, 2.0f);
    drawText("F5 RELOAD DATA", 24.0f, 222.0f, 2.0f);

    glEnable(GL_CULL_FACE);
    glEnable(GL_DEPTH_TEST);

    glPopMatrix();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
}

void drawInventoryBar(const int width, const int height, const Inventory& inventory) {
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glOrtho(0.0, width, height, 0.0, -1.0, 1.0);

    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();

    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);

    constexpr float slotSize = 44.0f;
    constexpr float gap = 8.0f;
    const float totalWidth = (slotSize * static_cast<float>(kInventorySlots)) + (gap * static_cast<float>(kInventorySlots - 1));
    const float startX = (static_cast<float>(width) - totalWidth) * 0.5f;
    const float startY = static_cast<float>(height) - 70.0f;

    for (int i = 0; i < kInventorySlots; ++i) {
        const float x = startX + static_cast<float>(i) * (slotSize + gap);
        const float y = startY;
        const bool selected = i == inventory.selectedIndex;

        glColor4f(selected ? 0.98f : 0.90f, selected ? 0.88f : 0.88f, selected ? 0.68f : 0.86f, 0.92f);
        glBegin(GL_QUADS);
        glVertex2f(x, y);
        glVertex2f(x + slotSize, y);
        glVertex2f(x + slotSize, y + slotSize);
        glVertex2f(x, y + slotSize);
        glEnd();

        glColor3f(0.18f, 0.18f, 0.20f);
        glLineWidth(selected ? 3.0f : 1.5f);
        glBegin(GL_LINE_LOOP);
        glVertex2f(x, y);
        glVertex2f(x + slotSize, y);
        glVertex2f(x + slotSize, y + slotSize);
        glVertex2f(x, y + slotSize);
        glEnd();

        const auto& slot = inventory.slots[static_cast<std::size_t>(i)];
        drawText(std::to_string(i + 1), x + 4.0f, y + 4.0f, 1.5f);
        if (slot.count > 0) {
            const std::string label = slot.itemId.size() > 5 ? slot.itemId.substr(0, 5) : slot.itemId;
            drawText(label, x + 5.0f, y + 16.0f, 1.5f);
            drawText(std::to_string(slot.count), x + 5.0f, y + 30.0f, 1.5f);
        }
    }

    glEnable(GL_CULL_FACE);
    glEnable(GL_DEPTH_TEST);

    glPopMatrix();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
}
}  // namespace voxel
