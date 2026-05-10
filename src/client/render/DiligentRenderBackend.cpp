#include "render/DiligentRenderBackend.hpp"

#include <cmath>
#include <cstring>
#include <stdexcept>
#include <vector>

#include "render/Mesh.hpp"

#if TERRALITE_ENABLE_DILIGENT
#    include <unordered_map>

#    include "Graphics/GraphicsEngine/interface/DeviceContext.h"
#    include "Graphics/GraphicsEngine/interface/RenderDevice.h"
#    include "Graphics/GraphicsEngine/interface/SwapChain.h"
#    include "Common/interface/RefCntAutoPtr.hpp"
#endif

#if TERRALITE_ENABLE_DILIGENT && defined(_WIN32)
#    define GLFW_EXPOSE_NATIVE_WIN32
#    include <GLFW/glfw3.h>
#    include <GLFW/glfw3native.h>
#    include "Graphics/GraphicsEngineD3D11/interface/EngineFactoryD3D11.h"
#    include "Graphics/GraphicsEngine/interface/GraphicsTypes.h"
#    include "Platforms/interface/NativeWindow.h"
#endif

namespace voxel {

// ---------------------------------------------------------------------------
// Matrix helpers (row-major, mul(rowVec, M) convention)
// ---------------------------------------------------------------------------
namespace {

void matIdentity(float m[16]) {
    for (int i = 0; i < 16; ++i) m[i] = 0.f;
    m[0] = m[5] = m[10] = m[15] = 1.f;
}

void matMul(const float a[16], const float b[16], float out[16]) {
    float tmp[16];
    for (int r = 0; r < 4; ++r)
        for (int c = 0; c < 4; ++c) {
            float s = 0.f;
            for (int k = 0; k < 4; ++k) s += a[r * 4 + k] * b[k * 4 + c];
            tmp[r * 4 + c] = s;
        }
    for (int i = 0; i < 16; ++i) out[i] = tmp[i];
}

float dot3(float ax, float ay, float az, float bx, float by, float bz) {
    return ax * bx + ay * by + az * bz;
}

void cross3(
    float ax, float ay, float az,
    float bx, float by, float bz,
    float& rx, float& ry, float& rz
) {
    rx = ay * bz - az * by;
    ry = az * bx - ax * bz;
    rz = ax * by - ay * bx;
}

void normalize3(float& x, float& y, float& z) {
    const float len = std::sqrt(x * x + y * y + z * z);
    if (len > 1e-6f) { x /= len; y /= len; z /= len; }
}

}  // namespace

// ---------------------------------------------------------------------------
// HLSL shaders
// ---------------------------------------------------------------------------
#if TERRALITE_ENABLE_DILIGENT
namespace {

// Vertex shader: transforms position by MVP, passes vertex color to PS.
// Uses row_major so C++ float[16] uploads match what the shader expects.
constexpr const char* kChunkVS = R"HLSL(
cbuffer Constants : register(b0)
{
    row_major float4x4 gMVP;
};

struct VSInput
{
    float3 pos  : ATTRIB0;
    float3 norm : ATTRIB1;
    float3 col  : ATTRIB2;
    float  u    : ATTRIB3;
    float  v    : ATTRIB4;
};

struct PSInput
{
    float4 pos : SV_POSITION;
    float3 col : COLOR;
};

void main(in VSInput vsIn, out PSInput psIn)
{
    psIn.pos = mul(float4(vsIn.pos, 1.0), gMVP);
    psIn.col = vsIn.col;
}
)HLSL";

constexpr const char* kChunkPS = R"HLSL(
struct PSInput
{
    float4 pos : SV_POSITION;
    float3 col : COLOR;
};

float4 main(in PSInput psIn) : SV_TARGET
{
    return float4(psIn.col, 1.0);
}
)HLSL";

}  // namespace
#endif

// ---------------------------------------------------------------------------
// Impl
// ---------------------------------------------------------------------------
struct DiligentRenderBackend::Impl {
#if TERRALITE_ENABLE_DILIGENT
    Diligent::RefCntAutoPtr<Diligent::IRenderDevice>           device;
    Diligent::RefCntAutoPtr<Diligent::IDeviceContext>          context;
    Diligent::RefCntAutoPtr<Diligent::ISwapChain>              swapChain;
    Diligent::RefCntAutoPtr<Diligent::IPipelineState>          pso;
    Diligent::RefCntAutoPtr<Diligent::IShaderResourceBinding>  srb;
    Diligent::RefCntAutoPtr<Diligent::IBuffer>                 vsConstants;
    Diligent::RefCntAutoPtr<Diligent::ITexture>                depthBuffer;
    Diligent::ITextureView*                                    depthView = nullptr;
    std::unordered_map<std::uintptr_t, Diligent::RefCntAutoPtr<Diligent::IBuffer>>   buffers;
    std::unordered_map<std::uintptr_t, Diligent::RefCntAutoPtr<Diligent::ITexture>>  textures;
    std::uintptr_t nextResourceId = 1;
    int width  = 0;
    int height = 0;
    float proj[16] = {};
    float view[16] = {};
#endif
};

DiligentRenderBackend::DiligentRenderBackend() : impl_(new Impl()) {
#if TERRALITE_ENABLE_DILIGENT
    matIdentity(impl_->proj);
    matIdentity(impl_->view);
#endif
}

DiligentRenderBackend::~DiligentRenderBackend() {
    delete impl_;
}

const char* DiligentRenderBackend::name() const {
#if TERRALITE_ENABLE_DILIGENT && defined(_WIN32)
    return "Diligent D3D11";
#elif TERRALITE_ENABLE_DILIGENT
    return "Diligent";
#else
    return "Diligent unavailable";
#endif
}

bool DiligentRenderBackend::available() const {
#if TERRALITE_ENABLE_DILIGENT && defined(_WIN32)
    return true;
#else
    return false;
#endif
}

// ---------------------------------------------------------------------------
// Depth buffer helpers
// ---------------------------------------------------------------------------
#if TERRALITE_ENABLE_DILIGENT
namespace {

void createDepthBuffer(
    Diligent::IRenderDevice* device,
    int width,
    int height,
    Diligent::RefCntAutoPtr<Diligent::ITexture>& outTexture,
    Diligent::ITextureView*& outView
) {
    outTexture = nullptr;
    outView    = nullptr;
    if (device == nullptr || width <= 0 || height <= 0) return;

    Diligent::TextureDesc desc;
    desc.Name      = "TERRALITE depth buffer";
    desc.Type      = Diligent::RESOURCE_DIM_TEX_2D;
    desc.Width     = static_cast<Diligent::Uint32>(width);
    desc.Height    = static_cast<Diligent::Uint32>(height);
    desc.MipLevels = 1;
    desc.Format    = Diligent::TEX_FORMAT_D32_FLOAT;
    desc.BindFlags = Diligent::BIND_DEPTH_STENCIL;
    device->CreateTexture(desc, nullptr, outTexture.RawDblPtr());
    if (outTexture) {
        outView = outTexture->GetDefaultView(Diligent::TEXTURE_VIEW_DEPTH_STENCIL);
    }
}

}  // namespace
#endif

// ---------------------------------------------------------------------------
// initialize
// ---------------------------------------------------------------------------
void DiligentRenderBackend::initialize(GLFWwindow* window, const int width, const int height) {
#if TERRALITE_ENABLE_DILIGENT && defined(_WIN32)
    if (window == nullptr) {
        throw std::runtime_error("Cannot initialize Diligent without a GLFW window.");
    }

#    if ENGINE_DLL
    auto getEngineFactoryD3D11 = Diligent::LoadGraphicsEngineD3D11();
    if (getEngineFactoryD3D11 == nullptr) {
        throw std::runtime_error("Failed to load the Diligent D3D11 engine factory.");
    }
    auto* factory = getEngineFactoryD3D11();
#    else
    auto* factory = Diligent::GetEngineFactoryD3D11();
#    endif
    if (factory == nullptr) {
        throw std::runtime_error("Diligent D3D11 engine factory is unavailable.");
    }

    Diligent::EngineD3D11CreateInfo engineCreateInfo;
    engineCreateInfo.GraphicsAPIVersion = Diligent::Version{11, 0};

    Diligent::IDeviceContext* contexts[] = {nullptr};
    factory->CreateDeviceAndContextsD3D11(
        engineCreateInfo,
        impl_->device.RawDblPtr(),
        contexts
    );
    impl_->context = contexts[0];

    Diligent::SwapChainDesc swapChainDesc;
    swapChainDesc.Width             = static_cast<Diligent::Uint32>(width);
    swapChainDesc.Height            = static_cast<Diligent::Uint32>(height);
    swapChainDesc.ColorBufferFormat = Diligent::TEX_FORMAT_RGBA8_UNORM;
    swapChainDesc.DepthBufferFormat = Diligent::TEX_FORMAT_UNKNOWN;  // managed manually

    Diligent::FullScreenModeDesc fullscreenDesc;
    const Diligent::NativeWindow nativeWindow{glfwGetWin32Window(window)};
    factory->CreateSwapChainD3D11(
        impl_->device,
        impl_->context,
        swapChainDesc,
        fullscreenDesc,
        nativeWindow,
        impl_->swapChain.RawDblPtr()
    );

    impl_->width  = width;
    impl_->height = height;

    // ---- Depth buffer -------------------------------------------------------
    createDepthBuffer(impl_->device, width, height, impl_->depthBuffer, impl_->depthView);

    // ---- Shaders ------------------------------------------------------------
    Diligent::RefCntAutoPtr<Diligent::IShader> vs;
    Diligent::RefCntAutoPtr<Diligent::IShader> ps;
    {
        Diligent::ShaderCreateInfo sci;
        sci.SourceLanguage = Diligent::SHADER_SOURCE_LANGUAGE_HLSL;

        sci.Desc.Name       = "TERRALITE chunk VS";
        sci.Desc.ShaderType = Diligent::SHADER_TYPE_VERTEX;
        sci.Source          = kChunkVS;
        impl_->device->CreateShader(sci, vs.RawDblPtr());

        sci.Desc.Name       = "TERRALITE chunk PS";
        sci.Desc.ShaderType = Diligent::SHADER_TYPE_PIXEL;
        sci.Source          = kChunkPS;
        impl_->device->CreateShader(sci, ps.RawDblPtr());
    }
    if (vs == nullptr || ps == nullptr) {
        throw std::runtime_error("Diligent: failed to compile chunk shaders.");
    }

    // ---- Pipeline state -----------------------------------------------------
    Diligent::LayoutElement layoutElems[] = {
        {0, 0, 3, Diligent::VT_FLOAT32, Diligent::False},  // position
        {1, 0, 3, Diligent::VT_FLOAT32, Diligent::False},  // normal
        {2, 0, 3, Diligent::VT_FLOAT32, Diligent::False},  // color
        {3, 0, 1, Diligent::VT_FLOAT32, Diligent::False},  // u
        {4, 0, 1, Diligent::VT_FLOAT32, Diligent::False},  // v
    };

    Diligent::GraphicsPipelineStateCreateInfo psoCI;
    psoCI.PSODesc.Name                         = "TERRALITE chunk PSO";
    psoCI.PSODesc.PipelineType                 = Diligent::PIPELINE_TYPE_GRAPHICS;
    psoCI.pVS                                  = vs;
    psoCI.pPS                                  = ps;
    psoCI.GraphicsPipeline.NumRenderTargets    = 1;
    psoCI.GraphicsPipeline.RTVFormats[0]       = Diligent::TEX_FORMAT_RGBA8_UNORM;
    psoCI.GraphicsPipeline.DSVFormat           = Diligent::TEX_FORMAT_D32_FLOAT;
    psoCI.GraphicsPipeline.PrimitiveTopology   = Diligent::PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    psoCI.GraphicsPipeline.RasterizerDesc.CullMode         = Diligent::CULL_MODE_NONE;
    psoCI.GraphicsPipeline.DepthStencilDesc.DepthEnable    = Diligent::True;
    psoCI.GraphicsPipeline.DepthStencilDesc.DepthWriteEnable = Diligent::True;
    psoCI.GraphicsPipeline.InputLayout.NumElements         = 5;
    psoCI.GraphicsPipeline.InputLayout.LayoutElements      = layoutElems;
    impl_->device->CreateGraphicsPipelineState(psoCI, impl_->pso.RawDblPtr());

    if (impl_->pso == nullptr) {
        throw std::runtime_error("Diligent: failed to create chunk pipeline state.");
    }

    // ---- Constant buffer (MVP matrix = 64 bytes) ----------------------------
    {
        Diligent::BufferDesc cbDesc;
        cbDesc.Name           = "TERRALITE chunk VS constants";
        cbDesc.Size           = 64;
        cbDesc.Usage          = Diligent::USAGE_DYNAMIC;
        cbDesc.BindFlags      = Diligent::BIND_UNIFORM_BUFFER;
        cbDesc.CPUAccessFlags = Diligent::CPU_ACCESS_WRITE;
        impl_->device->CreateBuffer(cbDesc, nullptr, impl_->vsConstants.RawDblPtr());
    }
    if (impl_->vsConstants == nullptr) {
        throw std::runtime_error("Diligent: failed to create chunk constant buffer.");
    }

    // Bind constant buffer to the static variable on the PSO, then create SRB.
    impl_->pso->GetStaticVariableByName(Diligent::SHADER_TYPE_VERTEX, "Constants")
        ->Set(impl_->vsConstants);
    impl_->pso->CreateShaderResourceBinding(impl_->srb.RawDblPtr(), true);

#else
    (void)window;
    (void)width;
    (void)height;
    throw std::runtime_error("Diligent proof-of-life backend is only wired for Windows D3D11 right now.");
#endif
}

// ---------------------------------------------------------------------------
// resize
// ---------------------------------------------------------------------------
void DiligentRenderBackend::resize(const int width, const int height) {
#if TERRALITE_ENABLE_DILIGENT
    if (impl_->swapChain == nullptr || width <= 0 || height <= 0) return;
    if (impl_->width == width && impl_->height == height) return;

    impl_->swapChain->Resize(static_cast<Diligent::Uint32>(width), static_cast<Diligent::Uint32>(height));
    impl_->width  = width;
    impl_->height = height;

    createDepthBuffer(impl_->device, width, height, impl_->depthBuffer, impl_->depthView);
#else
    (void)width;
    (void)height;
#endif
}

// ---------------------------------------------------------------------------
// clearFrame
// ---------------------------------------------------------------------------
void DiligentRenderBackend::clearFrame(const Color& clearColor) {
#if TERRALITE_ENABLE_DILIGENT
    if (impl_->context == nullptr || impl_->swapChain == nullptr) return;

    Diligent::ITextureView* rtv = impl_->swapChain->GetCurrentBackBufferRTV();
    impl_->context->SetRenderTargets(
        1,
        &rtv,
        impl_->depthView,
        Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION
    );
    const float color[] = {clearColor.r, clearColor.g, clearColor.b, 1.0f};
    impl_->context->ClearRenderTarget(rtv, color, Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    if (impl_->depthView) {
        impl_->context->ClearDepthStencil(
            impl_->depthView,
            Diligent::CLEAR_DEPTH_FLAG,
            1.f,
            0,
            Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION
        );
    }
#else
    (void)clearColor;
#endif
}

// ---------------------------------------------------------------------------
// present
// ---------------------------------------------------------------------------
void DiligentRenderBackend::present() {
#if TERRALITE_ENABLE_DILIGENT
    if (impl_->swapChain != nullptr) {
        impl_->swapChain->Present();
    }
#endif
}

// ---------------------------------------------------------------------------
// setPerspective  (row-major, left-handed D3D, Z in [0,1])
// ---------------------------------------------------------------------------
void DiligentRenderBackend::setPerspective(
    const float fovYDegrees,
    const float aspect,
    const float nearPlane,
    const float farPlane
) {
#if TERRALITE_ENABLE_DILIGENT
    const float f = 1.0f / std::tan(fovYDegrees * kPi / 360.0f);
    float* P = impl_->proj;
    for (int i = 0; i < 16; ++i) P[i] = 0.f;
    P[0]  = f / aspect;
    P[5]  = f;
    P[10] = farPlane / (farPlane - nearPlane);
    P[11] = 1.f;
    P[14] = -nearPlane * farPlane / (farPlane - nearPlane);
#else
    (void)fovYDegrees; (void)aspect; (void)nearPlane; (void)farPlane;
#endif
}

// ---------------------------------------------------------------------------
// applyCameraView  (row-major LookAtLH)
// ---------------------------------------------------------------------------
void DiligentRenderBackend::applyCameraView(const Vec3& eye, const Vec3& lookDirection) {
#if TERRALITE_ENABLE_DILIGENT
    float zx = lookDirection.x, zy = lookDirection.y, zz = lookDirection.z;
    normalize3(zx, zy, zz);

    float xx, xy, xz;
    cross3(0.f, 1.f, 0.f, zx, zy, zz, xx, xy, xz);  // right = worldUp x forward
    normalize3(xx, xy, xz);

    float yx, yy, yz;
    cross3(zx, zy, zz, xx, xy, xz, yx, yy, yz);  // up = forward x right

    float* V = impl_->view;
    // Row 0..2: columns of the rotation part (transpose of axes matrix)
    V[0]  = xx;  V[1]  = yx;  V[2]  = zx;  V[3]  = 0.f;
    V[4]  = xy;  V[5]  = yy;  V[6]  = zy;  V[7]  = 0.f;
    V[8]  = xz;  V[9]  = yz;  V[10] = zz;  V[11] = 0.f;
    // Row 3: negated dot products with eye
    V[12] = -dot3(xx, xy, xz, eye.x, eye.y, eye.z);
    V[13] = -dot3(yx, yy, yz, eye.x, eye.y, eye.z);
    V[14] = -dot3(zx, zy, zz, eye.x, eye.y, eye.z);
    V[15] = 1.f;
#else
    (void)eye; (void)lookDirection;
#endif
}

// ---------------------------------------------------------------------------
// renderMesh
// ---------------------------------------------------------------------------
void DiligentRenderBackend::renderMesh(const ChunkMesh& mesh) {
#if TERRALITE_ENABLE_DILIGENT
    if (impl_->pso == nullptr || impl_->srb == nullptr || impl_->vsConstants == nullptr) return;
    if (impl_->context == nullptr) return;

    // Build MVP = view * proj
    float mvp[16];
    matMul(impl_->view, impl_->proj, mvp);

    // Upload MVP to constant buffer
    {
        void* mapped = nullptr;
        impl_->context->MapBuffer(impl_->vsConstants, Diligent::MAP_WRITE, Diligent::MAP_FLAG_DISCARD, mapped);
        if (mapped) {
            std::memcpy(mapped, mvp, 64);
            impl_->context->UnmapBuffer(impl_->vsConstants, Diligent::MAP_WRITE);
        }
    }

    impl_->context->SetPipelineState(impl_->pso);
    impl_->context->CommitShaderResources(impl_->srb, Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

    for (const MeshSurface& surface : mesh.surfaces) {
        if (surface.vertexCount <= 0) continue;
        const std::uintptr_t bufferId = surface.vertexBuffer.diligentId();
        if (bufferId == 0) continue;

        auto it = impl_->buffers.find(bufferId);
        if (it == impl_->buffers.end()) continue;

        Diligent::IBuffer* vb     = it->second;
        const Diligent::Uint64 offset = 0;
        impl_->context->SetVertexBuffers(
            0, 1, &vb, &offset,
            Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION,
            Diligent::SET_VERTEX_BUFFERS_FLAG_RESET
        );

        Diligent::DrawAttribs drawAttribs;
        drawAttribs.NumVertices = static_cast<Diligent::Uint32>(surface.vertexCount);
        drawAttribs.Flags       = Diligent::DRAW_FLAG_VERIFY_ALL;
        impl_->context->Draw(drawAttribs);
    }
#else
    (void)mesh;
#endif
}

// ---------------------------------------------------------------------------
// Buffer / texture resource management
// ---------------------------------------------------------------------------
RenderBufferHandle DiligentRenderBackend::createVertexBuffer(const std::size_t byteCount, const void* data) {
#if TERRALITE_ENABLE_DILIGENT
    if (impl_->device == nullptr || data == nullptr || byteCount == 0) return {};

    Diligent::BufferDesc desc;
    desc.Name      = "TERRALITE Diligent vertex buffer";
    desc.Size      = static_cast<Diligent::Uint64>(byteCount);
    desc.BindFlags = Diligent::BIND_VERTEX_BUFFER;
    desc.Usage     = Diligent::USAGE_IMMUTABLE;

    Diligent::BufferData initData;
    initData.pData    = data;
    initData.DataSize = desc.Size;

    Diligent::RefCntAutoPtr<Diligent::IBuffer> buffer;
    impl_->device->CreateBuffer(desc, &initData, buffer.RawDblPtr());
    if (buffer == nullptr) return {};

    const std::uintptr_t id = impl_->nextResourceId++;
    impl_->buffers.emplace(id, std::move(buffer));
    return RenderBufferHandle::diligent(id);
#else
    (void)byteCount; (void)data;
    return {};
#endif
}

void DiligentRenderBackend::destroyBuffer(RenderBufferHandle& buffer) {
#if TERRALITE_ENABLE_DILIGENT
    const std::uintptr_t id = buffer.diligentId();
    if (id != 0) {
        impl_->buffers.erase(id);
        buffer.reset();
    }
#else
    (void)buffer;
#endif
}

RenderTextureHandle DiligentRenderBackend::createTexture2D(
    const int width,
    const int height,
    const int channelCount,
    const unsigned char* pixels
) {
#if TERRALITE_ENABLE_DILIGENT
    if (impl_->device == nullptr || pixels == nullptr || width <= 0 || height <= 0) return {};
    if (channelCount != 3 && channelCount != 4) return {};

    std::vector<unsigned char> rgbaPixels;
    const unsigned char* uploadPixels = pixels;
    if (channelCount == 3) {
        rgbaPixels.reserve(static_cast<std::size_t>(width * height * 4));
        for (int index = 0; index < width * height; ++index) {
            rgbaPixels.push_back(pixels[index * 3 + 0]);
            rgbaPixels.push_back(pixels[index * 3 + 1]);
            rgbaPixels.push_back(pixels[index * 3 + 2]);
            rgbaPixels.push_back(255);
        }
        uploadPixels = rgbaPixels.data();
    }

    Diligent::TextureDesc desc;
    desc.Name      = "TERRALITE Diligent texture";
    desc.Type      = Diligent::RESOURCE_DIM_TEX_2D;
    desc.Width     = static_cast<Diligent::Uint32>(width);
    desc.Height    = static_cast<Diligent::Uint32>(height);
    desc.MipLevels = 1;
    desc.Format    = Diligent::TEX_FORMAT_RGBA8_UNORM;
    desc.BindFlags = Diligent::BIND_SHADER_RESOURCE;
    desc.Usage     = Diligent::USAGE_IMMUTABLE;

    Diligent::TextureSubResData subresource;
    subresource.pData  = uploadPixels;
    subresource.Stride = static_cast<Diligent::Uint64>(width * 4);

    Diligent::TextureData initData;
    initData.NumSubresources = 1;
    initData.pSubResources   = &subresource;

    Diligent::RefCntAutoPtr<Diligent::ITexture> texture;
    impl_->device->CreateTexture(desc, &initData, texture.RawDblPtr());
    if (texture == nullptr) return {};

    const std::uintptr_t id = impl_->nextResourceId++;
    impl_->textures.emplace(id, std::move(texture));
    return RenderTextureHandle::diligent(id);
#else
    (void)width; (void)height; (void)channelCount; (void)pixels;
    return {};
#endif
}

void DiligentRenderBackend::destroyTexture(RenderTextureHandle& texture) {
#if TERRALITE_ENABLE_DILIGENT
    const std::uintptr_t id = texture.diligentId();
    if (id != 0) {
        impl_->textures.erase(id);
        texture.reset();
    }
#else
    (void)texture;
#endif
}

void DiligentRenderBackend::uploadChunkMesh(ChunkMesh& mesh) {
    for (std::size_t surfaceIndex = 0; surfaceIndex < mesh.surfaces.size(); ++surfaceIndex) {
        uploadChunkMeshSurface(mesh, surfaceIndex);
    }
}

bool DiligentRenderBackend::uploadChunkMeshSurface(ChunkMesh& mesh, const std::size_t surfaceIndex) {
    if (surfaceIndex >= mesh.surfaces.size()) return false;

    MeshSurface& surface = mesh.surfaces[surfaceIndex];
    if (surface.vertices.empty()) return false;

    surface.vertexCount  = static_cast<int>(surface.vertices.size());
    surface.vertexBuffer = createVertexBuffer(
        surface.vertices.size() * sizeof(MeshVertex),
        surface.vertices.data()
    );
    if (!surface.vertexBuffer.isValid()) {
        surface.vertexCount = 0;
        return false;
    }
    surface.vertices.clear();
    surface.vertices.shrink_to_fit();
    return true;
}

void DiligentRenderBackend::destroyChunkMesh(ChunkMesh& mesh) {
    for (auto& surface : mesh.surfaces) {
        destroyBuffer(surface.vertexBuffer);
        surface.vertexCount = 0;
    }
}

}  // namespace voxel
