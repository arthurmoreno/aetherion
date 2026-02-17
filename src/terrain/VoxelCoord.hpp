#ifndef VOXEL_COORD_HPP
#define VOXEL_COORD_HPP

#include <cstddef>

// VoxelCoord: A simple 3D integer coordinate structure for voxel-based spatial indexing
// Used primarily for hash-based lookups in unordered containers
struct VoxelCoord {
    int x, y, z;

    bool operator==(const VoxelCoord& o) const { return x == o.x && y == o.y && z == o.z; }
};

// VoxelCoordHash: Hash functor for VoxelCoord to enable use with std::unordered_map
// Uses a simple but effective integer hash combining strategy
struct VoxelCoordHash {
    std::size_t operator()(const VoxelCoord& k) const noexcept {
        // Simple integer hash combine
        std::size_t h = static_cast<std::size_t>(k.x);
        h ^= static_cast<std::size_t>(k.y) + 0x9e3779b97f4a7c15ULL + (h << 6) + (h >> 2);
        h ^= static_cast<std::size_t>(k.z) + 0x9e3779b97f4a7c15ULL + (h << 6) + (h >> 2);
        return h;
    }
};

#endif  // VOXEL_COORD_HPP
