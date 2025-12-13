# src/ecosystem/CMakeLists.txt

# Define ecosystem sources and headers

file(GLOB ECOSYSTEM_HEADERS
    "${CMAKE_SOURCE_DIR}/src/ecosystem/*.hpp"
)

file(GLOB ECOSYSTEM_SOURCES "${CMAKE_SOURCE_DIR}/src/ecosystem/*.cpp")

# Create library for ecosystem
add_library(ecosystem STATIC ${ECOSYSTEM_SOURCES} ${ECOSYSTEM_HEADERS})

# Ensure the library is built with PIC
set_target_properties(ecosystem PROPERTIES POSITION_INDEPENDENT_CODE ON)
# Specify include directories for ecosystem
target_include_directories(ecosystem PUBLIC
    ${CMAKE_SOURCE_DIR}/src
    ${FLATBUFFERS_GENERATED_CPP_DIR}
)

# add_compile_definitions(ENTT_ENTITY_TYPE=int)
# Link dependencies for ecosystem
target_link_libraries(ecosystem PUBLIC
    components
    ${OPENVDB_LIBRARIES}
    ${TBB_LIBRARIES}
    ${LMDB_LIBRARIES}
    flatbuffers
)

# Ensure ecosystem depends on FlatBuffers generation
add_dependencies(ecosystem GenerateFlatBuffers)