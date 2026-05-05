cmake_minimum_required(VERSION 3.20)
project(VoxelGame LANGUAGES CXX)
include(FetchContent)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Allow older third-party CMakeLists (e.g. miniz) that declare cmake_minimum < 3.5
set(CMAKE_POLICY_VERSION_MINIMUM 3.5)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(APPLE)
    list(APPEND CMAKE_PREFIX_PATH
        "/opt/homebrew"
        "/opt/homebrew/opt/glfw"
        "/opt/homebrew/opt/freetype"
        "/usr/local"
        "/usr/local/opt/glfw"
        "/usr/local/opt/freetype"
    )
endif()

# ── QuickJS-NG (JavaScript engine for ScriptManager) ─────────────────────────
FetchContent_Declare(
    quickjs
    GIT_REPOSITORY https://github.com/quickjs-ng/quickjs.git
    GIT_TAG        v0.9.0
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(quickjs)

# ── miniz (zip reading for pack system) ──────────────────────────────────────
FetchContent_Declare(
    miniz
    GIT_REPOSITORY https://github.com/richgel999/miniz.git
    GIT_TAG        3.0.2
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(miniz)

FetchContent_Declare(
    enet
    GIT_REPOSITORY https://github.com/lsalzman/enet.git
    GIT_TAG        v1.3.18
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(enet)

# ── FreeType (required by RmlUI font engine) ──────────────────────────────────
find_package(Freetype QUIET)
if(NOT Freetype_FOUND)
    set(FT_DISABLE_ZLIB    ON CACHE BOOL "" FORCE)
    set(FT_DISABLE_BZIP2   ON CACHE BOOL "" FORCE)
    set(FT_DISABLE_PNG     ON CACHE BOOL "" FORCE)
    set(FT_DISABLE_HARFBUZZ ON CACHE BOOL "" FORCE)
    set(FT_DISABLE_BROTLI  ON CACHE BOOL "" FORCE)
    FetchContent_Declare(
        freetype
        GIT_REPOSITORY https://github.com/freetype/freetype.git
        GIT_TAG        VER-2-13-3
        GIT_SHALLOW    TRUE
    )
    FetchContent_MakeAvailable(freetype)
    if(TARGET freetype AND NOT TARGET Freetype::Freetype)
        add_library(Freetype::Freetype ALIAS freetype)
    endif()
endif()

# ── RmlUI ─────────────────────────────────────────────────────────────────────
set(BUILD_SHARED_LIBS          OFF CACHE BOOL "" FORCE)
set(RMLUI_SAMPLES              OFF CACHE BOOL "" FORCE)
set(RMLUI_TESTS                OFF CACHE BOOL "" FORCE)
set(RMLUI_THIRDPARTY_CONTAINERS OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
    RmlUi
    GIT_REPOSITORY https://github.com/mikke89/RmlUi.git
    GIT_TAG        6.0
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(RmlUi)

# ── Dear ImGui (debug overlay) ────────────────────────────────────────────────
FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG        v1.91.0
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(imgui)

set(IMGUI_SOURCES
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl2.cpp
    src/common/pack/ScriptBindingCustom.cpp
)

find_package(glfw3 CONFIG QUIET)

if(TARGET glfw)
    set(GLFW_TARGET glfw)
elseif(TARGET glfw::glfw)
    set(GLFW_TARGET glfw::glfw)
else()
    find_path(GLFW_INCLUDE_DIR GLFW/glfw3.h
        PATHS
            /opt/homebrew/include
            /opt/homebrew/opt/glfw/include
            /usr/local/include
            /usr/local/opt/glfw/include
            "C:/Program Files/GLFW/include"
            "C:/glfw/include"
    )
    find_library(GLFW_LIBRARY glfw
        PATHS
            /opt/homebrew/lib
            /opt/homebrew/opt/glfw/lib
            /usr/local/lib
            /usr/local/opt/glfw/lib
            "C:/Program Files/GLFW/lib"
            "C:/glfw/lib"
    )

    if(GLFW_INCLUDE_DIR AND GLFW_LIBRARY)
        add_library(glfw_manual UNKNOWN IMPORTED)
        set_target_properties(glfw_manual PROPERTIES
            IMPORTED_LOCATION "${GLFW_LIBRARY}"
            INTERFACE_INCLUDE_DIRECTORIES "${GLFW_INCLUDE_DIR}"
        )
        set(GLFW_TARGET glfw_manual)
    else()
        set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
        set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
        set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
        FetchContent_Declare(
            glfw
            GIT_REPOSITORY https://github.com/glfw/glfw.git
            GIT_TAG        3.4
            GIT_SHALLOW    TRUE
        )
        FetchContent_MakeAvailable(glfw)
        set(GLFW_TARGET glfw)
    endif()
endif()

# ── Platform-specific OpenGL and windowing libraries ──────────────────────────
if(APPLE)
    find_library(OPENGL_FRAMEWORK OpenGL REQUIRED)
    find_library(COCOA_FRAMEWORK   Cocoa REQUIRED)
    find_library(IOKIT_FRAMEWORK   IOKit REQUIRED)
    find_library(COREVIDEO_FRAMEWORK CoreVideo REQUIRED)
    set(PLATFORM_LIBS
        ${OPENGL_FRAMEWORK}
        ${COCOA_FRAMEWORK}
        ${IOKIT_FRAMEWORK}
        ${COREVIDEO_FRAMEWORK}
    )
elseif(WIN32)
    set(PLATFORM_LIBS opengl32)
else()  # Linux / BSD
    find_package(OpenGL REQUIRED)
    set(PLATFORM_LIBS OpenGL::GL dl)
endif()

add_executable(VoxelGame
    src/common/data/GameData.cpp
    src/common/data/JsonValue.cpp
    src/common/pack/Pack.cpp
    src/common/pack/PackManager.cpp
    src/client/pack/AssetPackManager.cpp
    src/common/pack/ScriptManager.cpp
    src/common/pack/ScriptBindingsCustom.cpp
    src/common/pack/generated/ParseBindings.cpp
    src/main.cpp
    src/client/game/Game.cpp
    src/client/game/GameBlocks.cpp
    src/client/game/GameLifecycle.cpp
    src/client/game/GameRender.cpp
    src/client/game/GameUpdate.cpp
    src/common/Math.cpp
    src/common/OSUtils.cpp
    src/common/world/WorldPersistence.cpp
    src/common/network/NetworkManager.cpp
    src/server/HeadlessServer.cpp
    src/server/ServerBootstrap.cpp
    src/common/player/Inventory.cpp
    src/client/render/Mesh.cpp
    src/client/render/ModelManager.cpp
    src/common/Player.cpp
    src/client/render/Renderer.cpp
    src/client/render/ShaderProgram.cpp
    src/client/render/TextureManager.cpp
    src/client/ui/GameUI.cpp
    src/common/world/BiomeDefinition.cpp
    src/common/world/TerrainGenerator.cpp
    src/common/world/WorldSimulation.cpp
    src/common/world/World.cpp
    ${rmlui_SOURCE_DIR}/Backends/RmlUi_Platform_GLFW.cpp
    ${rmlui_SOURCE_DIR}/Backends/RmlUi_Renderer_GL2.cpp
    ${IMGUI_SOURCES}
)
target_include_directories(VoxelGame PRIVATE
    include
    include/common
    include/client
    include/server
    src/client
    src/server
    libraries
    ${enet_SOURCE_DIR}/include
    ${quickjs_SOURCE_DIR}
    ${rmlui_SOURCE_DIR}/Include
    ${rmlui_SOURCE_DIR}/Backends
    ${imgui_SOURCE_DIR}
    ${imgui_SOURCE_DIR}/backends
)
target_compile_features(VoxelGame PRIVATE cxx_std_20)
target_compile_definitions(VoxelGame PRIVATE VOXEL_CLIENT)
target_link_libraries(VoxelGame PRIVATE
    enet
    miniz
    qjs
    ${GLFW_TARGET}
    ${PLATFORM_LIBS}
    RmlUi::Core
    Freetype::Freetype
)
if(WIN32)
    target_link_libraries(VoxelGame PRIVATE ws2_32 winmm)
endif()

add_executable(VoxelServer
    src/server/ServerMain.cpp
    src/server/HeadlessServer.cpp
    src/server/ServerBootstrap.cpp
    src/common/data/GameData.cpp
    src/common/data/JsonValue.cpp
    src/common/pack/Pack.cpp
    src/common/pack/PackManager.cpp
    src/common/pack/ScriptManager.cpp
    src/common/pack/ScriptBindingCustom.cpp
    src/common/pack/ScriptBindingsCustom.cpp
    src/common/pack/generated/ParseBindings.cpp
    src/common/Math.cpp
    src/common/OSUtils.cpp
    src/common/Player.cpp
    src/common/world/WorldPersistence.cpp
    src/common/player/Inventory.cpp
    src/common/network/NetworkManager.cpp
    src/common/world/BiomeDefinition.cpp
    src/common/world/TerrainGenerator.cpp
    src/common/world/World.cpp
    src/common/world/WorldSimulation.cpp
)
target_include_directories(VoxelServer PRIVATE
    include
    include/common
    include/server
    src/server
    libraries
    ${enet_SOURCE_DIR}/include
    ${quickjs_SOURCE_DIR}
)
target_compile_features(VoxelServer PRIVATE cxx_std_20)
target_link_libraries(VoxelServer PRIVATE enet miniz qjs)
if(WIN32)
    target_link_libraries(VoxelServer PRIVATE ws2_32 winmm)
endif()

# ── Code generation ───────────────────────────────────────────────────────────
find_program(NODEJS node)
if(NODEJS)
    add_custom_target(generate_bindings
        COMMAND ${NODEJS} ${CMAKE_SOURCE_DIR}/tools/codegen/generate.js
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        SOURCES
            ${CMAKE_SOURCE_DIR}/tools/codegen/generate.js
            ${CMAKE_SOURCE_DIR}/tools/codegen/schema/block.js
            ${CMAKE_SOURCE_DIR}/tools/codegen/schema/item.js
            ${CMAKE_SOURCE_DIR}/tools/codegen/schema/biome.js
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
    target_compile_definitions(VoxelGame PRIVATE GL_SILENCE_DEPRECATION)
endif()


# ── Install & CPack (distributable package) ───────────────────────────────────
install(TARGETS VoxelGame
    RUNTIME DESTINATION .   # Windows/Linux: place exe at package root
    BUNDLE  DESTINATION .   # macOS: place .app at package root
)
install(DIRECTORY ${CMAKE_SOURCE_DIR}/packs/ DESTINATION packs)

set(CPACK_PACKAGE_NAME        "VoxelGame")
set(CPACK_PACKAGE_VERSION     "0.1.0")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "A voxel survival game")
set(CPACK_PACKAGE_INSTALL_DIRECTORY   "VoxelGame")

if(APPLE)
    set(CPACK_GENERATOR "DragNDrop")
elseif(WIN32)
    # Produces both a ZIP and an NSIS installer (NSIS must be installed)
    set(CPACK_GENERATOR "ZIP;NSIS")
    set(CPACK_NSIS_DISPLAY_NAME "VoxelGame")
    set(CPACK_NSIS_ENABLE_UNINSTALL_BEFORE_INSTALL ON)
else()
    set(CPACK_GENERATOR "TGZ")
endif()

include(CPack)
