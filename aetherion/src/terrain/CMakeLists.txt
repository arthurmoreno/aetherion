# src/terrain/CMakeLists.txt

# Define terrain sources and headers

file(GLOB TERRAIN_HEADERS
    "${CMAKE_SOURCE_DIR}/src/terrain/*.hpp"
)

file(GLOB TERRAIN_SOURCES "${CMAKE_SOURCE_DIR}/src/terrain/*.cpp")

# Create library for terrain
add_library(terrain STATIC ${TERRAIN_SOURCES} ${TERRAIN_HEADERS})

# Ensure the library is built with PIC
set_target_properties(terrain PROPERTIES POSITION_INDEPENDENT_CODE ON)
# Specify include directories for terrain
target_include_directories(terrain PUBLIC
    ${CMAKE_SOURCE_DIR}/src
    ${FLATBUFFERS_GENERATED_CPP_DIR}
)

# add_compile_definitions(ENTT_ENTITY_TYPE=int)
# Link dependencies for terrain
target_link_libraries(terrain PUBLIC
    components
    ${OPENVDB_LIBRARIES}
    ${TBB_LIBRARIES}
    ${LMDB_LIBRARIES}
    flatbuffers
)

# Ensure terrain depends on FlatBuffers generation
add_dependencies(terrain GenerateFlatBuffers)