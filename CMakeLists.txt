cmake_minimum_required(VERSION 3.20)
project(Terralite LANGUAGES CXX)

include(FetchContent)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Allow older third-party CMakeLists (e.g. miniz) that declare cmake_minimum < 3.5.
set(CMAKE_POLICY_VERSION_MINIMUM 3.5)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(TERRALITE_ENABLE_DILIGENT "Enable experimental Diligent Engine integration hooks" OFF)

include(cmake/Dependencies.cmake)
include(cmake/Targets.cmake)
include(cmake/Packaging.cmake)
