add_library(voxel_core STATIC
    include/common/data/DataComponents.hpp
    src/common/data/GameData.cpp
    src/common/data/DataComponents.cpp
    src/common/diagnostics/EngineDiagnostics.cpp
    src/common/jobs/JobSystem.cpp
    src/common/data/JsonValue.cpp
    src/common/pack/Pack.cpp
    src/common/pack/PackManager.cpp
    src/common/pack/ScriptManager.cpp
    src/common/pack/ScriptBindingsCustom.cpp
    src/common/pack/generated/ParseBindings.cpp
    src/common/ecs/EntitySystem.cpp
    src/common/ecs/Systems.cpp
    src/common/Math.cpp
    src/common/OSUtils.cpp
    src/common/world/WorldPersistence.cpp
    src/common/network/NetworkManager.cpp
    src/common/player/Inventory.cpp
    src/common/world/BiomeDefinition.cpp
    src/common/world/TerrainGenerator.cpp
    src/common/world/WorldSimulation.cpp
    src/common/world/World.cpp
    src/common/PlayerShared.cpp
)
target_include_directories(voxel_core PUBLIC
    include
    include/common
    third_party
    ${enet_SOURCE_DIR}/include
    ${quickjs_SOURCE_DIR}
)
target_compile_features(voxel_core PUBLIC cxx_std_23)
target_link_libraries(voxel_core PUBLIC
    enet
    EnTT::EnTT
    miniz
    qjs
)

add_library(voxel_server_support STATIC
    src/server/HeadlessServer.cpp
    src/server/ServerBootstrap.cpp
)
target_include_directories(voxel_server_support PUBLIC
    include
    include/common
    include/server
    src/server
    third_party
    ${enet_SOURCE_DIR}/include
    ${quickjs_SOURCE_DIR}
)
target_compile_features(voxel_server_support PUBLIC cxx_std_23)
target_link_libraries(voxel_server_support PUBLIC voxel_core)

add_library(voxel_client_support STATIC
    src/client/pack/AssetPackManager.cpp
    src/client/game/Game.cpp
    src/client/game/GameBlocks.cpp
    src/client/game/GameLifecycle.cpp
    src/client/game/GameRender.cpp
    src/client/game/GameUpdate.cpp
    src/client/render/Mesh.cpp
    src/platform/glfw/GlfwClientWindow.cpp
    src/platform/glfw/GlfwInput.cpp
    src/client/render/DiligentRenderBackend.cpp
    src/client/render/ModelManager.cpp
    src/client/render/OpenGLRenderBackend.cpp
    src/client/render/Renderer.cpp
    src/client/render/ShaderProgram.cpp
    src/client/render/TextureManager.cpp
    src/client/ui/GameUI.cpp
    src/client/ui/RmlUi_Platform_GLFW.cpp
    src/client/ui/RmlUi_Renderer_GL2.cpp
    src/common/Player.cpp
    ${IMGUI_SOURCES}
)
target_include_directories(voxel_client_support PUBLIC
    include
    include/common
    include/client
    src/client
    src/common
    include/client/ui
    third_party
    ${enet_SOURCE_DIR}/include
    ${quickjs_SOURCE_DIR}
    ${imgui_SOURCE_DIR}
    ${imgui_SOURCE_DIR}/backends
)
target_compile_features(voxel_client_support PUBLIC cxx_std_23)
target_compile_definitions(voxel_client_support PRIVATE VOXEL_CLIENT)
target_link_libraries(voxel_client_support PUBLIC
    voxel_core
    ${GLFW_TARGET}
    ${PLATFORM_LIBS}
    RmlUi::Core
    Freetype::Freetype
)
if(TERRALITE_ENABLE_DILIGENT)
    set(TERRALITE_DILIGENT_BACKEND_LIBS
        Diligent-GraphicsEngineOpenGL-shared
        Diligent-GraphicsEngineVk-shared
    )
    if(WIN32)
        list(APPEND TERRALITE_DILIGENT_BACKEND_LIBS
            Diligent-GraphicsEngineD3D11-shared
            Diligent-GraphicsEngineD3D12-shared
        )
    endif()

    target_compile_definitions(voxel_client_support PUBLIC TERRALITE_ENABLE_DILIGENT=1)
    target_include_directories(voxel_client_support PUBLIC
        ${diligentcore_SOURCE_DIR}
    )
    target_link_libraries(voxel_client_support PUBLIC
        Diligent-BuildSettings
        ${TERRALITE_DILIGENT_BACKEND_LIBS}
    )
else()
    target_compile_definitions(voxel_client_support PUBLIC TERRALITE_ENABLE_DILIGENT=0)
endif()
if(WIN32)
    target_link_libraries(voxel_core PUBLIC ws2_32 winmm)
endif()

add_executable(Terralite
    src/app/client/ClientNetworkSession.cpp
    src/app/client/ClientOptions.cpp
    src/app/client/ClientRuntimeBridge.cpp
    src/app/client/ClientUiController.cpp
    src/app/client/ClientMain.cpp
)
target_link_libraries(Terralite PRIVATE
    voxel_client_support
    voxel_server_support
)
if(TERRALITE_ENABLE_DILIGENT)
    copy_required_dlls(Terralite)
endif()

add_executable(TerraliteServer
    src/app/server/ServerMain.cpp
)
target_link_libraries(TerraliteServer PRIVATE
    voxel_server_support
)

add_library(launcher_core STATIC
    src/launcher/LauncherCore.cpp
    src/common/data/JsonValue.cpp
)
target_include_directories(launcher_core PUBLIC
    include
    include/common
)
target_compile_features(launcher_core PUBLIC cxx_std_23)

add_executable(TerraliteLauncher
    src/app/launcher/LauncherMain.cpp
    ${IMGUI_SOURCES}
)
target_include_directories(TerraliteLauncher PRIVATE
    include
    ${imgui_SOURCE_DIR}
    ${imgui_SOURCE_DIR}/backends
)
target_compile_features(TerraliteLauncher PRIVATE cxx_std_23)
target_compile_definitions(TerraliteLauncher PRIVATE
    TERRALITE_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
)
target_link_libraries(TerraliteLauncher PRIVATE
    launcher_core
    ${GLFW_TARGET}
    ${PLATFORM_LIBS}
)
if(APPLE)
    target_compile_definitions(TerraliteLauncher PRIVATE GL_SILENCE_DEPRECATION)
endif()

add_executable(TerraliteDataTests
    tests/GameDataTests.cpp
)
target_link_libraries(TerraliteDataTests PRIVATE voxel_core)
target_compile_definitions(TerraliteDataTests PRIVATE
    TERRALITE_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
)

add_executable(TerraliteDataComponentTests
    tests/DataComponentTests.cpp
)
target_link_libraries(TerraliteDataComponentTests PRIVATE voxel_core)

add_executable(TerraliteLauncherCoreTests
    tests/LauncherCoreTests.cpp
)
target_link_libraries(TerraliteLauncherCoreTests PRIVATE launcher_core)

add_executable(TerraliteNetworkConnectionTests
    tests/NetworkConnectionTests.cpp
)
target_link_libraries(TerraliteNetworkConnectionTests PRIVATE voxel_server_support)
target_compile_definitions(TerraliteNetworkConnectionTests PRIVATE
    TERRALITE_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
)

add_executable(TerraliteJobSystemTests
    tests/JobSystemTests.cpp
)
target_link_libraries(TerraliteJobSystemTests PRIVATE voxel_core)

# ── Code generation ───────────────────────────────────────────────────────────
find_program(NODEJS node)
if(NODEJS)
    file(GLOB CODEGEN_SCHEMA_FILES CONFIGURE_DEPENDS
        ${CMAKE_SOURCE_DIR}/tools/codegen/schema/*.js
    )
    add_custom_target(generate_bindings
        COMMAND ${NODEJS} ${CMAKE_SOURCE_DIR}/tools/codegen/generate.js
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        SOURCES
            ${CMAKE_SOURCE_DIR}/tools/codegen/generate.js
            ${CODEGEN_SCHEMA_FILES}
        COMMENT "Regenerating parse bindings and voxel.d.ts from schema (cmake --build . --target generate_bindings)"
    )
else()
    add_custom_target(generate_bindings
        COMMAND ${CMAKE_COMMAND} -E echo "Node.js not found; using pre-generated bindings"
    )
    message(STATUS "Node.js not found — using pre-generated bindings (run node tools/codegen/generate.js to regenerate)")
endif()
if(NOT TARGET update_mappings)
    add_custom_target(update_mappings DEPENDS generate_bindings)
endif()

if(APPLE)
    target_compile_definitions(voxel_client_support PRIVATE GL_SILENCE_DEPRECATION)
endif()

enable_testing()
add_test(NAME TerraliteDataTests COMMAND $<TARGET_FILE:TerraliteDataTests>)
add_test(NAME TerraliteDataComponentTests COMMAND $<TARGET_FILE:TerraliteDataComponentTests>)
add_test(NAME TerraliteLauncherCoreTests COMMAND $<TARGET_FILE:TerraliteLauncherCoreTests>)
add_test(NAME TerraliteNetworkConnectionTests COMMAND $<TARGET_FILE:TerraliteNetworkConnectionTests>)
add_test(NAME TerraliteJobSystemTests COMMAND $<TARGET_FILE:TerraliteJobSystemTests>)
