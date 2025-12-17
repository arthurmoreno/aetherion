#ifndef VOXELGRID_HPP
#define VOXELGRID_HPP

#include <flatbuffers/flatbuffers.h>
#include <nanobind/nanobind.h>
#include <openvdb/openvdb.h>
#include <openvdb/tools/Interpolation.h>

#include <entt/entt.hpp>
#include <functional>
#include <iostream>
#include <map>
#include <memory>
#include <msgpack.hpp>
#include <mutex>
#include <shared_mutex>
#include <sstream>
#include <tuple>
#include <vector>

#include "VoxelGridView_generated.h"
#include "terrain/TerrainGridRepository.hpp"
#include "terrain/TerrainStorage.hpp"
#include "voxelgrid/GridData.hpp"
#include "voxelgrid/VoxelGridView.hpp"

namespace nb = nanobind;

class VoxelGrid {
   public:
    int width, height, depth;

    // Storage and ECS components
    entt::registry& registry;
    std::unique_ptr<TerrainStorage> terrainStorage;
    std::unique_ptr<TerrainGridRepository> terrainGridRepository;

    // Other grids (not terrain - that's handled by TerrainGridRepository)
    openvdb::Int32Grid::Ptr entityGrid;    // Grid for entity ID
    openvdb::Int32Grid::Ptr eventGrid;     // Grid for event ID
    openvdb::FloatGrid::Ptr lightingGrid;  // Grid for lighting level

    VoxelGrid(entt::registry& registry);  // Constructor that takes registry
    ~VoxelGrid();                         // Destructor

    void initializeGrids();                                    // Initializes the grids
    void setVoxel(int x, int y, int z, const GridData& data);  // Set voxel data
    GridData getVoxel(int x, int y, int z) const;              // Get voxel data

    // Individual component setters and getters
    bool checkIfTerrainExists(int x, int y, int z) const;
    EntityTypeComponent getTerrainEntityTypeComponent(int x, int y, int z) const;
    void setTerrain(int x, int y, int z, int terrainID);
    int getTerrain(int x, int y, int z) const;
    // Forward to TerrainGridRepository::deleteTerrain. If `takeLock` is false,
    // the repository will assume the caller already holds the terrain grid lock.
    void deleteTerrain(entt::dispatcher& dispatcher, int x, int y, int z,
                       bool takeLock = true);

    void setEntity(int x, int y, int z, int entityID);
    int getEntity(int x, int y, int z) const;
    int getEntityUnsafe(int x, int y,
                        int z) const;  // Fast unsafe read for performance-critical paths
    void deleteEntity(int x, int y, int z);
    void moveEntity(entt::entity entity, Position movingToPosition);

    void setEvent(int x, int y, int z, int eventID);
    int getEvent(int x, int y, int z) const;

    void setLightingLevel(int x, int y, int z, float lightingLevel);
    float getLightingLevel(int x, int y, int z) const;

    // Serialization and Deserialization methods using OpenVDB stream
    std::vector<char> serializeToBytes() const;                    // Serialize to a byte stream
    void deserializeFromBytes(const std::vector<char>& byteData);  // Deserialize from a byte stream

    // Msgpack-compatible serialization and deserialization methods
    template <typename Packer>
    void msgpack_pack(Packer& pk) const;  // Msgpack serialization method

    void msgpack_unpack(msgpack::object const& o);  // Msgpack deserialization method

    // --- New Utility Search Methods ---

    std::vector<VoxelGridCoordinates> getAllTerrainInRegion(int x_min, int y_min, int z_min,
                                                            int x_max, int y_max, int z_max) const;

    std::vector<VoxelGridCoordinates> getAllEntityInRegion(int x_min, int y_min, int z_min,
                                                           int x_max, int y_max, int z_max) const;

    std::vector<VoxelGridCoordinates> getAllEventInRegion(int x_min, int y_min, int z_min,
                                                          int x_max, int y_max, int z_max) const;

    std::vector<VoxelGridCoordinates> getAllLightingInRegion(int x_min, int y_min, int z_min,
                                                             int x_max, int y_max, int z_max) const;

    // Methods to retrieve entity IDs within a region
    std::vector<int> getAllTerrainIdsInRegion(int x_min, int y_min, int z_min, int x_max, int y_max,
                                              int z_max, VoxelGridView& gridView) const;

    std::vector<int> getAllEntityIdsInRegion(int x_min, int y_min, int z_min, int x_max, int y_max,
                                             int z_max, VoxelGridView& gridView) const;

    std::vector<int> getAllEventIdsInRegion(int x_min, int y_min, int z_min, int x_max, int y_max,
                                            int z_max) const;

    std::vector<int> getAllLightingIdsInRegion(int x_min, int y_min, int z_min, int x_max,
                                               int y_max, int z_max) const;

   private:
    int defaultEmptyValue = -1;

    // Mutex specifically for entityGrid thread safety
    mutable std::shared_mutex entityGridMutex;
};

#endif  // VOXELGRID_HPP