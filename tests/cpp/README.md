# C++ Tests for TerrainStorage Water Simulation

This directory contains C++ tests that demonstrate the new iterator-based approach for water simulation using the TerrainStorage system.

## Overview

The main test file `test_water_simulation.cpp` shows how to replace the ECS-based water processing from `EcosystemEngine::loopTiles()` with the new TerrainStorage iterators.

### Key Features Tested

1. **Basic Water Iteration**: Direct iteration over water matter using `storage.iterateWaterMatter()`
2. **Water Flow Simulation**: Simple gravity-based water distribution 
3. **Evaporation Simulation**: Converting water to vapor based on sun intensity
4. **Random Processing**: Shuffled processing similar to EcosystemEngine pattern
5. **Minimum Water Generation**: Automatic water creation when levels are low
6. **High-Level Repository Iterators**: Integration with ECS components via TerrainGridRepository

## Building and Running

### Using Make (Recommended)
```bash
# Build and run C++ tests
make cpp-tests

# Run tests only (after building once)
make cpp-tests-only
```

### Using CMake Directly
```bash
# Configure with tests enabled
cmake -S . -B build-tests -DBUILD_CPP_TESTS=ON -DCMAKE_PREFIX_PATH="$PREFIX_PATH"

# Build tests
cd build-tests && make test_water_simulation

# Run tests
./tests/cpp/test_water_simulation
```

## Performance Benefits

This iterator-based approach provides several advantages over the ECS approach:

- **Sparse Iteration**: Only processes voxels that actually contain water
- **Direct Access**: No ECS overhead for basic terrain data access
- **OpenVDB Optimization**: Leverages OpenVDB's efficient sparse data structures
- **Memory Efficiency**: Reduced memory allocation/deallocation compared to ECS views
- **Template Flexibility**: Generic callbacks allow custom processing logic

## Test Structure

The test creates a `WaterSimulator` class that demonstrates:

1. **Water Flow Logic**: Uses `iterateWaterMatter()` to collect sources, then redistributes water based on gravity
2. **Evaporation Logic**: Processes surface water and converts to vapor at higher altitudes  
3. **Conservation Checks**: Verifies total water+vapor remains constant during transformations
4. **Random Processing**: Shows how to shuffle processing order like the original EcosystemEngine

## Replacing ECS Pattern

The original ECS pattern:
```cpp
auto matter_container_view = registry.view<MatterContainer>();
std::vector<entt::entity> entities(matter_container_view.begin(), matter_container_view.end());
std::shuffle(entities.begin(), entities.end(), gen);

for (auto entity : entities) {
    processTileWater(entity, registry, ...);
}
```

Becomes:
```cpp
std::vector<std::tuple<int, int, int, int>> waterVoxels;
storage.iterateWaterMatter([&](int x, int y, int z, int amount) {
    waterVoxels.emplace_back(x, y, z, amount);
});
std::shuffle(waterVoxels.begin(), waterVoxels.end(), gen);

for (auto [x, y, z, amount] : waterVoxels) {
    processWaterVoxel(x, y, z, amount, ...);
}
```

This eliminates ECS entity lookups and provides direct coordinate-based access to terrain data.
