# src/voxelgrid/CMakeLists.txt

# Define voxelgrid sources and headers

file(GLOB VOXELGRID_HEADERS
    "${CMAKE_SOURCE_DIR}/src/voxelgrid/*.hpp"
)

file(GLOB VOXELGRID_SOURCES "${CMAKE_SOURCE_DIR}/src/voxelgrid/*.cpp")

# Create library for voxelgrid
add_library(voxelgrid STATIC ${VOXELGRID_SOURCES} ${VOXELGRID_HEADERS})

# Ensure the library is built with PIC
set_target_properties(voxelgrid PROPERTIES POSITION_INDEPENDENT_CODE ON)
# Specify include directories for voxelgrid
target_include_directories(voxelgrid PUBLIC
    ${CMAKE_SOURCE_DIR}/src
    ${FLATBUFFERS_GENERATED_CPP_DIR}
)

# add_compile_definitions(ENTT_ENTITY_TYPE=int)
# Link dependencies for voxelgrid
target_link_libraries(voxelgrid PUBLIC
    components
    ${OPENVDB_LIBRARIES}
    ${TBB_LIBRARIES}
    ${LMDB_LIBRARIES}
    ${MSGPACK_LIBRARIES}
    flatbuffers
)

# Ensure voxelgrid depends on FlatBuffers generation
add_dependencies(voxelgrid GenerateFlatBuffers)