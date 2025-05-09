#ifndef VOXELGRID_HPP
#define VOXELGRID_HPP

#include <flatbuffers/flatbuffers.h>
#include <nanobind/nanobind.h>
#include <openvdb/openvdb.h>
#include <openvdb/tools/Interpolation.h>

#include <functional>
#include <iostream>
#include <map>
#include <msgpack.hpp>
#include <sstream>
#include <tuple>
#include <vector>

#include "VoxelGridView_generated.h"

namespace nb = nanobind;

enum class GridType { TERRAIN, ENTITY };

struct VoxelGridCoordinates {
    int x, y, z;

    // Default constructor
    VoxelGridCoordinates() : x(0), y(0), z(0) {}

    // Parameterized constructor
    VoxelGridCoordinates(int x_val, int y_val, int z_val) : x(x_val), y(y_val), z(z_val) {}

    // Overload comparison operators to use this structure as a key in std::map
    bool operator<(const VoxelGridCoordinates& other) const {
        return std::tie(x, y, z) < std::tie(other.x, other.y, other.z);
    }

    // Equality operator
    bool operator==(const VoxelGridCoordinates& other) const {
        return x == other.x && y == other.y && z == other.z;
    }

    MSGPACK_DEFINE(x, y, z);  // Msgpack macro for serialization
};

// Structure for storing voxel data
struct GridData {
    int terrainID;        // Stores terrain type or ID
    int entityID;         // Stores entity ID (if any)
    int eventID;          // Stores the event type or ID
    float lightingLevel;  // Stores lighting level

    MSGPACK_DEFINE(terrainID, entityID, eventID, lightingLevel);
};

class VoxelGridViewFlatB {
   public:
    // Constructor accepts the raw FlatBuffer data pointer
    VoxelGridViewFlatB(const GameEngine::VoxelGridView* fbVoxelGridView)
        : fbVoxelGridView(fbVoxelGridView) {}

    // Constructor accepts nb::bytes
    VoxelGridViewFlatB(nb::bytes serialized_data) {
        const char* data_ptr = serialized_data.c_str();
        size_t data_size = serialized_data.size();

        if (data_size == 0) {
            throw std::runtime_error("Serialized data is empty");
        }

        fbVoxelGridView = GameEngine::GetVoxelGridView(data_ptr);  // Deserialize FlatBuffer
    }

    // Accessor methods to access fields directly
    int getWidth() const { return fbVoxelGridView->width(); }

    int getHeight() const { return fbVoxelGridView->height(); }

    int getDepth() const { return fbVoxelGridView->depth(); }

    int getXOffset() const { return fbVoxelGridView->x_offset(); }

    int getYOffset() const { return fbVoxelGridView->y_offset(); }

    int getZOffset() const { return fbVoxelGridView->z_offset(); }

    // Access voxel grid data without deserializing
    int getTerrainVoxel(int x, int y, int z) const {
        int local_x = x - getXOffset();
        int local_y = y - getYOffset();
        int local_z = z - getZOffset();

        if (local_x >= 0 && local_x < getWidth() && local_y >= 0 && local_y < getHeight() &&
            local_z >= 0 && local_z < getDepth()) {
            // Calculate the index in the FlatBuffer data array
            int index = local_x + local_y * getWidth() + local_z * getWidth() * getHeight();

            // Access the GridData at the calculated index
            return fbVoxelGridView->terrainData()->Get(index);
        } else {
            // Handle out-of-bounds access
            // std::cerr << "Attempted to get voxel out of bounds at (" << x << ", " << y << ", " <<
            // z
            //           << ")" << std::endl;
            return -1;  // Return null pointer to indicate out-of-bounds access
        }
    }

    // Access voxel grid data without deserializing
    int getEntityVoxel(int x, int y, int z) const {
        int local_x = x - getXOffset();
        int local_y = y - getYOffset();
        int local_z = z - getZOffset();

        if (local_x >= 0 && local_x < getWidth() && local_y >= 0 && local_y < getHeight() &&
            local_z >= 0 && local_z < getDepth()) {
            // Calculate the index in the FlatBuffer data array
            int index = local_x + local_y * getWidth() + local_z * getWidth() * getHeight();

            // Access the GridData at the calculated index
            return fbVoxelGridView->entityData()->Get(index);
        } else {
            // Handle out-of-bounds access
            // std::cerr << "Attempted to get voxel out of bounds at (" << x << ", " << y << ", " <<
            // z
            //           << ")" << std::endl;
            return -1;  // Return null pointer to indicate out-of-bounds access
        }
    }

   private:
    const GameEngine::VoxelGridView* fbVoxelGridView;
};

class VoxelGridView {
   public:
    int width, height, depth;
    int x_offset, y_offset, z_offset;
    std::vector<int> terrainData;
    std::vector<int> entityData;

    // Parameterized constructor
    void initVoxelGridView(int width, int height, int depth, int x_offset, int y_offset,
                           int z_offset);

    // Set and get voxel data
    void setTerrainVoxel(int x, int y, int z, const int voxelData);
    int getTerrainVoxel(int x, int y, int z) const;

    // Set and get voxel data
    void setEntityVoxel(int x, int y, int z, const int voxelData);
    int getEntityVoxel(int x, int y, int z) const;

    // FlatBuffers serialization (returns FlatBuffers offset)
    flatbuffers::Offset<GameEngine::VoxelGridView> serializeFlatBuffers(
        flatbuffers::FlatBufferBuilder& builder) const {
        // Serialize terrainData vector
        auto terrainDataOffset = builder.CreateVector(terrainData);

        // Serialize entityData vector
        auto entityDataOffset = builder.CreateVector(entityData);

        // Create the VoxelGridView FlatBuffer object
        return GameEngine::CreateVoxelGridView(builder, width, height, depth, x_offset, y_offset,
                                               z_offset, terrainDataOffset, entityDataOffset);
    }

    // FlatBuffers deserialization
    static VoxelGridView deserializeFlatBuffers(const GameEngine::VoxelGridView* fbVoxelGridView) {
        if (!fbVoxelGridView) {
            throw std::invalid_argument("fbVoxelGridView pointer is null");
        }

        VoxelGridView vgv;

        // Deserialize basic properties
        vgv.width = fbVoxelGridView->width();
        vgv.height = fbVoxelGridView->height();
        vgv.depth = fbVoxelGridView->depth();
        vgv.x_offset = fbVoxelGridView->x_offset();
        vgv.y_offset = fbVoxelGridView->y_offset();
        vgv.z_offset = fbVoxelGridView->z_offset();

        // Deserialize terrainData
        auto fbTerrainData = fbVoxelGridView->terrainData();
        if (fbTerrainData) {
            vgv.terrainData.assign(fbTerrainData->begin(), fbTerrainData->end());
        } else {
            // Handle the case where terrainData is missing
            throw std::runtime_error("terrainData is missing in FlatBuffer VoxelGridView");
        }

        // Deserialize entityData
        auto fbEntityData = fbVoxelGridView->entityData();
        if (fbEntityData) {
            vgv.entityData.assign(fbEntityData->begin(), fbEntityData->end());
        } else {
            // Handle the case where entityData is missing
            throw std::runtime_error("entityData is missing in FlatBuffer VoxelGridView");
        }

        return vgv;
    }
};

class VoxelGrid {
   public:
    int width, height, depth;
    openvdb::Int32Grid::Ptr terrainGrid;   // Grid for terrain ID
    openvdb::Int32Grid::Ptr entityGrid;    // Grid for entity ID
    openvdb::Int32Grid::Ptr eventGrid;     // Grid for event ID
    openvdb::FloatGrid::Ptr lightingGrid;  // Grid for lighting level

    VoxelGrid();   // Constructor
    ~VoxelGrid();  // Destructor

    void initializeGrids();                                    // Initializes the grids
    void setVoxel(int x, int y, int z, const GridData& data);  // Set voxel data
    GridData getVoxel(int x, int y, int z) const;              // Get voxel data

    // Individual component setters and getters
    void setTerrain(int x, int y, int z, int terrainID);
    int getTerrain(int x, int y, int z) const;
    void deleteTerrain(int x, int y, int z);

    void setEntity(int x, int y, int z, int entityID);
    int getEntity(int x, int y, int z) const;
    void deleteEntity(int x, int y, int z);

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
};

#endif  // VOXELGRID_HPP