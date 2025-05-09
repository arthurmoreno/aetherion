
file(GLOB COMPONENTS_HEADERS
    "${CMAKE_SOURCE_DIR}/src/neat/*.hpp"
)

file(GLOB COMPONENTS_SOURCES "${CMAKE_SOURCE_DIR}/src/neat/*.cpp")

# Create library for neat
add_library(neat STATIC ${COMPONENTS_SOURCES} ${COMPONENTS_HEADERS})

# Specify include directories for neat
target_include_directories(neat PUBLIC
    ${CMAKE_SOURCE_DIR}/include/YourProject
    ${FLATBUFFERS_GENERATED_CPP_DIR}
)

# Link dependencies for neat
target_link_libraries(neat PUBLIC
    ${OPENVDB_LIBRARIES}
    ${TBB_LIBRARIES}
    flatbuffers
)

# Ensure neat depends on FlatBuffers generation
add_dependencies(neat GenerateFlatBuffers)