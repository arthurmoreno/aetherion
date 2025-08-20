#ifndef TERRAIN_GRID_REPOSITORY_HPP
#define TERRAIN_GRID_REPOSITORY_HPP

#include "terrain/TerrainStorage.hpp"
#include "components/PhysicsComponents.hpp"

// Intermediate layer between ECS and OpenVDB for terrain-related synchronization.
// Bare-bones declaration only; implementation to be added later.
class TerrainGridRepository {
   TerrainStorage terrainStorage;
   public:
    TerrainGridRepository() = default;

    // MatterContainer getMatterContainer(int x, int y, int z) const {
    //     return terrainStorage.getTerrainMatterContainer(x, y, z);
    // }
};

#endif  // TERRAIN_GRID_REPOSITORY_HPP
