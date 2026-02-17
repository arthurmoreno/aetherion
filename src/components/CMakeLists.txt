# src/components/CMakeLists.txt

# Define components sources and headers

file(GLOB_RECURSE COMPONENTS_HEADERS
    "${CMAKE_SOURCE_DIR}/src/components/*.hpp"
)

file(GLOB_RECURSE COMPONENTS_SOURCES "${CMAKE_SOURCE_DIR}/src/components/*.cpp")

# Create library for components
add_library(components STATIC ${COMPONENTS_SOURCES} ${COMPONENTS_HEADERS})

# Specify include directories for components
target_include_directories(components PUBLIC
    ${CMAKE_SOURCE_DIR}/include/YourProject
    ${FLATBUFFERS_GENERATED_CPP_DIR}
)

# Link dependencies for components
target_link_libraries(components PUBLIC
    ${OPENVDB_LIBRARIES}
    ${TBB_LIBRARIES}
    flatbuffers
)

# Ensure components depends on FlatBuffers generation
add_dependencies(components GenerateFlatBuffers)