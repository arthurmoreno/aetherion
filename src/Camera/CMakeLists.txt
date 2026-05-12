# src/Camera/CMakeLists.txt — aetherion::Camera
#
# Camera-pipeline rendering (was previously the top-level `CameraUtils.cpp`
# free functions plus the new `DimetricTileWalker.cpp` from the
# Phase-0/1/2 migration). Hosting both under one module groups the
# camera-side render code in a single static lib and matches the
# per-module organisation of Gui / LowLevelRenderer / diag.
#
# Migration plan:
# .claude/docs/epics-plans/2026-05-11-dimetric-tile-walker-cpp-migration.md

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/../cmake/")

find_package(SDL2 REQUIRED)
find_package(SDL2_image REQUIRED)
find_package(Python3 COMPONENTS Interpreter Development REQUIRED)
set(NANOBIND_DIR "${CMAKE_SOURCE_DIR}/libs/nanobind")
set(nanobind_DIR "${CMAKE_SOURCE_DIR}/libs/nanobind/cmake")
find_package(nanobind CONFIG REQUIRED)

file(GLOB CAMERA_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_SOURCE_DIR}/src/Camera/*.cpp"
)

file(GLOB CAMERA_HEADERS CONFIGURE_DEPENDS
    "${CMAKE_SOURCE_DIR}/src/Camera/*.hpp"
)

add_library(Camera STATIC ${CAMERA_SOURCES} ${CAMERA_HEADERS})

set_target_properties(Camera PROPERTIES POSITION_INDEPENDENT_CODE ON)

target_include_directories(Camera PUBLIC
    ${CMAKE_SOURCE_DIR}/src
    ${FLATBUFFERS_GENERATED_CPP_DIR}
    ${SDL2_INCLUDE_DIRS}
    ${SDL2_IMAGE_INCLUDE_DIRS}
    ${Python3_INCLUDE_DIRS}
    ${CMAKE_SOURCE_DIR}/libs
)

target_link_libraries(Camera PUBLIC
    components
    LowLevelRenderer
    ${SDL2_LIBRARIES}
    ${SDL2_IMAGE_LIBRARIES}
    Python3::Python
)

# Camera depends on FlatBuffer-generated headers (transitively, via
# `WorldView.hpp` / `EntityInterface.hpp`).
add_dependencies(Camera GenerateFlatBuffers)
