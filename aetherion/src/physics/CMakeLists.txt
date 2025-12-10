# src/physics/CMakeLists.txt

# Define physics sources and headers

file(GLOB PHYSICS_HEADERS
    "${CMAKE_SOURCE_DIR}/src/physics/*.hpp"
)

file(GLOB PHYSICS_SOURCES "${CMAKE_SOURCE_DIR}/src/physics/*.cpp")

# Create library for physics
add_library(physics STATIC ${PHYSICS_SOURCES} ${PHYSICS_HEADERS})

# Ensure the library is built with PIC
set_target_properties(physics PROPERTIES POSITION_INDEPENDENT_CODE ON)
# Specify include directories for physics
target_include_directories(physics PUBLIC
    ${CMAKE_SOURCE_DIR}/src
    ${FLATBUFFERS_GENERATED_CPP_DIR}
)

# add_compile_definitions(ENTT_ENTITY_TYPE=int)
# Link dependencies for physics
target_link_libraries(physics PUBLIC
    components
    ${OPENVDB_LIBRARIES}
    ${TBB_LIBRARIES}
    ${LMDB_LIBRARIES}
    flatbuffers
)

# Ensure physics depends on FlatBuffers generation
add_dependencies(physics GenerateFlatBuffers)