cmake_minimum_required(VERSION 3.15)

# Set project name for the tests
project(AetherionCppTests)

# Find required packages (minimal for C++ tests)
find_package(PkgConfig REQUIRED)
find_package(TBB REQUIRED)

# Set OpenVDB paths manually (same as main project)
set(OPENVDB_INCLUDE_DIR /usr/local/include/openvdb)
set(OPENVDB_LIBRARIES /usr/local/lib/libopenvdb.so)

message(STATUS "Using OpenVDB include directory: ${OPENVDB_INCLUDE_DIR}")
message(STATUS "Using OpenVDB library: ${OPENVDB_LIBRARIES}")

# Include directories from main project
include_directories(${CMAKE_SOURCE_DIR}/../../src)
include_directories(${CMAKE_SOURCE_DIR}/../../include)
include_directories(${CMAKE_SOURCE_DIR}/../../libs)  # For entt.hpp
include_directories(${OPENVDB_INCLUDE_DIR})

# Add the same definitions as main project
add_compile_definitions(ENTT_ENTITY_TYPE=int)

# Get all source files from main project (only what we need for terrain)
file(GLOB TERRAIN_SOURCES 
    "${CMAKE_SOURCE_DIR}/../../src/terrain/TerrainStorage.cpp"
    "${CMAKE_SOURCE_DIR}/../../src/terrain/TerrainGridRepository.cpp"
)

# Only include basic component files we need
file(GLOB COMPONENT_SOURCES 
    "${CMAKE_SOURCE_DIR}/../../src/components/ConsoleLogsComponent.cpp"
    "${CMAKE_SOURCE_DIR}/../../src/components/MetabolismComponents.cpp"
    "${CMAKE_SOURCE_DIR}/../../src/components/ItemsComponents.cpp"
)

# Create the water simulation test executable
add_executable(test_water_simulation 
    test_water_simulation.cpp
    ${TERRAIN_SOURCES}
    ${COMPONENT_SOURCES}
)

# Link required libraries
target_link_libraries(test_water_simulation PRIVATE
    ${OPENVDB_LIBRARIES}
    TBB::tbb
    ${CMAKE_DL_LIBS}
    pthread
)

# Set C++ standard
target_compile_features(test_water_simulation PRIVATE cxx_std_20)

# Add compile flags
target_compile_options(test_water_simulation PRIVATE
    -Wall
    -Wextra
    -O2
)

# Add include directories for OpenVDB
target_include_directories(test_water_simulation PRIVATE ${OPENVDB_INCLUDE_DIR})

# Add test to CTest if enabled
enable_testing()
add_test(NAME WaterSimulation COMMAND test_water_simulation)

message(STATUS "C++ water simulation test configured")
