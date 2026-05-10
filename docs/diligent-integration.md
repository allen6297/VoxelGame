# Diligent Engine Integration

TERRALITE keeps game and pack rendering code behind `IRenderBackend`. The current shipping backend is `OpenGLRenderBackend`; Diligent integration is experimental and guarded by CMake.

## CMake Switch

Configure with:

```powershell
cmake -S . -B cmake-build-diligent -DTERRALITE_ENABLE_DILIGENT=ON
```

When enabled, CMake fetches `DiligentCore`.

The source directory intentionally uses that exact name because Diligent headers expect the module layout. `DiligentTools` and `DiligentFX` are intentionally deferred: the first TERRALITE spike only needs Core/device/backend APIs, and Tools currently requires a Python environment with `pip` for `libclang`.

## Current Status

- The dependency hook is present, verified, and off by default.
- Normal OpenGL builds do not fetch or link Diligent.
- `voxel_client_support` receives `TERRALITE_ENABLE_DILIGENT=1` only when the option is enabled.
- Windows links D3D11, D3D12, OpenGL, and Vulkan Diligent backends; non-Windows links OpenGL and Vulkan.
- The Diligent-enabled configure currently needs an explicit Python interpreter when Python is not on `PATH`.
- A Windows Debug DiligentCore-only build was verified with `TERRALITE_ENABLE_DILIGENT=ON`; `Terralite.exe` builds and all three test executables pass under that build tree.
- The renderer-facing boundary is already in place for frame setup, mesh buffers, texture handles, preview viewports, and high-level draw calls.

## Next Spike

The first Diligent backend should be a proof-of-life backend that creates a device/context/swapchain and clears the frame. It should not replace chunk rendering until it can satisfy the existing `IRenderBackend` resource and draw contracts.
