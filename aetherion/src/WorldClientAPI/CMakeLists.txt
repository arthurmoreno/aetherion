# WorldClientAPI Module
# This module provides the main interface for external clients (players, AI agents)
# to interact with the game world

# Collect all source files for WorldClientAPI
set(WORLD_CLIENT_API_SOURCES
    PerceptionCore.cpp
    TerrainVisibility.cpp
    CommandValidator.cpp
    CommandHandlers.cpp
    ResponseSerializer.cpp
    ProcessOptionalQueries.cpp
)

# Collect all header files for WorldClientAPI
set(WORLD_CLIENT_API_HEADERS
    PerceptionCore.hpp
    TerrainVisibility.hpp
    CommandConstants.hpp
    CommandValidator.hpp
    CommandHandlers.hpp
    ResponseSerializer.hpp
    ProcessOptionalQueries.hpp
)

# Create a static library for WorldClientAPI
add_library(WorldClientAPI STATIC ${WORLD_CLIENT_API_SOURCES} ${WORLD_CLIENT_API_HEADERS})

# Ensure the library is built with PIC
set_target_properties(WorldClientAPI PROPERTIES POSITION_INDEPENDENT_CODE ON)

# Specify include directories for WorldClientAPI
target_include_directories(WorldClientAPI PUBLIC
    ${CMAKE_SOURCE_DIR}/src
    ${FLATBUFFERS_GENERATED_CPP_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}
    # Add other necessary include directories if needed
)

# Link dependencies for WorldClientAPI (if any)
target_link_libraries(WorldClientAPI PUBLIC
    components
    # Add other necessary libraries if needed
)

# Ensure WorldClientAPI depends on FlatBuffers generation
add_dependencies(WorldClientAPI GenerateFlatBuffers)

# Optional: Add header files to IDE project view
source_group("WorldClientAPI\\Headers" FILES ${WORLD_CLIENT_API_HEADERS})
source_group("WorldClientAPI\\Sources" FILES ${WORLD_CLIENT_API_SOURCES})
