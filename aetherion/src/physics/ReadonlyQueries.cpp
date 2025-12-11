
// =========================================================================
// ================ 1. READ-ONLY QUERY FUNCTIONS ================
// =========================================================================
// These functions perform read-only queries on game state without making
// any modifications. They are used for validation, collision detection,
// and state inspection.
// =========================================================================

#include "ReadonlyQueries.hpp"

#include <entt/entt.hpp>
#include <tuple>

// #include "MoveEntityEvent.hpp"
// #include "PhysicsEngine.hpp"
#include "components/EntityTypeComponent.hpp"
#include "components/PhysicsComponents.hpp"
#include "components/TerrainComponents.hpp"
#include "voxelgrid/VoxelGrid.hpp"

// Helper function to determine direction
int getDirectionFromVelocity(float velocity) {
    if (velocity > 0.0f) {
        return 1;
    } else if (velocity < 0.0f) {
        return -1;
    } else {
        return 0;
    }
}

// Helper: Check if terrain is soft empty
bool isTerrainSoftEmpty(EntityTypeComponent& terrainType) {
    return (terrainType.mainType == static_cast<int>(EntityEnum::TERRAIN) &&
            terrainType.subType0 == static_cast<int>(TerrainEnum::EMPTY));
}

// Helper: Get DirectionEnum from velocities
DirectionEnum getDirectionFromVelocities(float velocityX, float velocityY, float velocityZ) {
    DirectionEnum direction;
    if (velocityX > 0) {
        direction = DirectionEnum::RIGHT;
    } else if (velocityX < 0) {
        direction = DirectionEnum::LEFT;
    } else if (velocityY < 0) {
        direction = DirectionEnum::UP;
    } else if (velocityY > 0) {
        direction = DirectionEnum::DOWN;
    } else if (velocityZ > 0) {
        direction = DirectionEnum::UPWARD;
    } else {
        direction = DirectionEnum::DOWNWARD;
    }

    return direction;
}

bool checkIfCanJump(const MoveSolidEntityEvent& event) {
    // Implement the logic to determine if the entity can jump
    // Placeholder implementation:
    return true;
}

bool checkIfCanFall(entt::registry& registry, VoxelGrid& voxelGrid, int i, int j, int k) {
    // return false;

    int movingToEntityId = voxelGrid.getEntity(i, j, k - 1);
    bool canFallOnterrain = false;
    if (voxelGrid.checkIfTerrainExists(i, j, k - 1)) {
        EntityTypeComponent etc =
            voxelGrid.terrainGridRepository->getTerrainEntityType(i, j, k - 1);
        // Any terrain that is different than water
        if (etc.subType0 == 1) {
            canFallOnterrain = true;
        }
    } else {
        canFallOnterrain = true;
    }

    return (k > 0 and movingToEntityId == -1 and canFallOnterrain);
}

// Helper: Get type and check if soft empty
bool getTypeAndCheckSoftEmpty(entt::registry& registry, VoxelGrid& voxelGrid, int terrainId, int x,
                              int y, int z) {
    if (terrainId == static_cast<int>(TerrainIdTypeEnum::NONE)) {
        return false;  // Means terrain voxel is empty.
    } else if (terrainId == -1) {
        return false;  // Means terrain voxel is completely empty (no entity).
        EntityTypeComponent terrainEntityType =
            voxelGrid.terrainGridRepository->getTerrainEntityType(x, y, z);
        const bool isTerrainNeighborSoftEmpty{isTerrainSoftEmpty(terrainEntityType)};
        return isTerrainNeighborSoftEmpty;
    } else {
        // TODO: This is not being handled. It should be handled when active EnTT terrains start to
        // be used.
        std::cout << "[getTypeAndCheckSoftEmpty]: terrainId is neither -1 nor -2. Not handled yet."
                  << std::endl;
        return false;
    }
}