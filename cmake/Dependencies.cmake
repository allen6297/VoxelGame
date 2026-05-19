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
    GIT_TAG        v0.14.0
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

# ── EnTT (gameplay ECS for dynamic entities) ─────────────────────────────────
FetchContent_Declare(
    EnTT
    GIT_REPOSITORY https://github.com/skypjack/entt.git
    GIT_TAG        v3.16.0
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(EnTT)

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
set_target_properties(rmlui_core PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)

# ── Dear ImGui (debug overlay) ────────────────────────────────────────────────
FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG        v1.91.0
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(imgui)

if(TERRALITE_ENABLE_DILIGENT)
    set(DILIGENT_INSTALL_CORE OFF CACHE BOOL "" FORCE)
    set(DILIGENT_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(
        DiligentCore
        GIT_REPOSITORY https://github.com/DiligentGraphics/DiligentCore.git
        GIT_TAG        v2.5.6
        SOURCE_DIR     ${CMAKE_BINARY_DIR}/_deps/DiligentCore
        GIT_SHALLOW    TRUE
    )
    FetchContent_MakeAvailable(DiligentCore)
endif()

set(IMGUI_SOURCES
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl2.cpp
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
