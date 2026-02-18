# src/LowLevelRenderer/CMakeLists.txt

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/../cmake/")


find_package(SDL2 REQUIRED)
find_package(SDL2_image REQUIRED)
find_package(SDL2_ttf REQUIRED)
find_package(OpenGL REQUIRED)
find_package(Python3 COMPONENTS Interpreter Development REQUIRED)
set(NANOBIND_DIR "${CMAKE_SOURCE_DIR}/libs/nanobind")
set(nanobind_DIR "${CMAKE_SOURCE_DIR}/libs/nanobind/cmake")
find_package(nanobind CONFIG REQUIRED)

# Collect all .cpp source files in LowLevelRenderer
file(GLOB LOWLEVELRENDERER_SOURCES
    "${CMAKE_SOURCE_DIR}/src/LowLevelRenderer/*.cpp"
)

# Collect all .hpp header files in LowLevelRenderer
file(GLOB LOWLEVELRENDERER_HEADERS
    "${CMAKE_SOURCE_DIR}/src/LowLevelRenderer/*.hpp"
)

# Create a static library for LowLevelRenderer
add_library(LowLevelRenderer STATIC ${LOWLEVELRENDERER_SOURCES} ${LOWLEVELRENDERER_HEADERS})

# Ensure the library is built with PIC
set_target_properties(LowLevelRenderer PROPERTIES POSITION_INDEPENDENT_CODE ON)

# Specify include directories for LowLevelRenderer
target_include_directories(LowLevelRenderer PUBLIC
    ${CMAKE_SOURCE_DIR}/src
    ${FLATBUFFERS_GENERATED_CPP_DIR}
    ${SDL2_INCLUDE_DIRS}
    ${SDL2_IMAGE_INCLUDE_DIRS}
    ${SDL2_TTF_INCLUDE_DIRS}
    ${OPENGL_INCLUDE_DIRS}
    ${Python3_INCLUDE_DIRS}
)

# Link dependencies for LowLevelRenderer
target_link_libraries(LowLevelRenderer PUBLIC
    components
    ${SDL2_LIBRARIES}
    ${SDL2_IMAGE_LIBRARIES}
    ${SDL2_TTF_LIBRARIES}
    ${OPENGL_LIBRARIES}
    SDL2_ttf
    Python3::Python
)

# Ensure LowLevelRenderer depends on FlatBuffers generation
add_dependencies(LowLevelRenderer GenerateFlatBuffers)