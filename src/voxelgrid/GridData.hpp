#ifndef VOXELGRID_DATA_HPP
#define VOXELGRID_DATA_HPP

#include <msgpack.hpp>

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

#endif  // VOXELGRID_DATA_HPP