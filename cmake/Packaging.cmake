# ── Install & CPack (distributable package) ───────────────────────────────────
install(TARGETS Terralite TerraliteLauncher TerraliteServer
    RUNTIME DESTINATION .   # Windows/Linux: place exe at package root
    BUNDLE  DESTINATION .   # macOS: place .app at package root
)
install(DIRECTORY ${CMAKE_SOURCE_DIR}/packs/ DESTINATION packs)

set(CPACK_PACKAGE_NAME        "Terralite")
set(CPACK_PACKAGE_VERSION     "0.1.0")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "A voxel survival game")
set(CPACK_PACKAGE_INSTALL_DIRECTORY   "Terralite")

if(APPLE)
    set(CPACK_GENERATOR "DragNDrop")
elseif(WIN32)
    # Produces both a ZIP and an NSIS installer (NSIS must be installed)
    set(CPACK_GENERATOR "ZIP;NSIS")
    set(CPACK_NSIS_DISPLAY_NAME "Terralite")
    set(CPACK_NSIS_ENABLE_UNINSTALL_BEFORE_INSTALL ON)
else()
    set(CPACK_GENERATOR "TGZ")
endif()

include(CPack)
