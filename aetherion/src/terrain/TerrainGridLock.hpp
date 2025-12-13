#ifndef TERRAIN_GRID_LOCK_HPP
#define TERRAIN_GRID_LOCK_HPP

#include "terrain/TerrainGridRepository.hpp"

// RAII lock guard for TerrainGridRepository's manual locking mechanism.
// Automatically releases the lock on destruction, providing exception safety
// and preventing deadlocks from missed unlock calls.
//
// Usage:
//   TerrainGridLock lock(voxelGrid.terrainGridRepository.get());
//   // ... perform locked operations ...
//   // Lock automatically released when lock goes out of scope
//
class TerrainGridLock {
   private:
    TerrainGridRepository* repo_;

   public:
    explicit TerrainGridLock(TerrainGridRepository* repo) : repo_(repo) {
        if (repo_) {
            repo_->lockTerrainGrid();
        }
    }

    ~TerrainGridLock() {
        if (repo_) {
            repo_->unlockTerrainGrid();
        }
    }

    // Non-copyable, non-movable to ensure single ownership
    TerrainGridLock(const TerrainGridLock&) = delete;
    TerrainGridLock& operator=(const TerrainGridLock&) = delete;
    TerrainGridLock(TerrainGridLock&&) = delete;
    TerrainGridLock& operator=(TerrainGridLock&&) = delete;
};

#endif  // TERRAIN_GRID_LOCK_HPP
