
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

bool hasCollision(entt::registry& registry, VoxelGrid& voxelGrid, entt::entity entity,
                  int movingFromX, int movingFromY, int movingFromZ, int movingToX, int movingToY,
                  int movingToZ, bool isTerrain) {
    bool collision = false;
    // Check if the movement is within bounds for x, y, z
    if ((0 <= movingToX && movingToX < voxelGrid.width) &&
        (0 <= movingToY && movingToY < voxelGrid.height) &&
        (0 <= movingToZ && movingToZ < voxelGrid.depth)) {
        int movingToEntityId = voxelGrid.getEntity(movingToX, movingToY, movingToZ);
        bool terrainExists = voxelGrid.checkIfTerrainExists(movingToX, movingToY, movingToZ);

        bool entityCollision = false;
        if (movingToEntityId != -1) {
            entityCollision = true;
        }

        bool terrainCollision = false;
        if (terrainExists) {
            EntityTypeComponent etc = getEntityTypeComponent(
                registry, voxelGrid, entity, movingFromX, movingFromY, movingFromZ, isTerrain);
            EntityTypeComponent terrainEtc = voxelGrid.terrainGridRepository->getTerrainEntityType(
                movingToX, movingToY, movingToZ);
            // Any terrain that is different than water
            if (etc.mainType == static_cast<int>(EntityEnum::TERRAIN)) {
                terrainCollision = true;
            } else if (terrainEtc.subType0 != static_cast<int>(TerrainEnum::EMPTY) &&
                       terrainEtc.subType0 != static_cast<int>(TerrainEnum::WATER)) {
                terrainCollision = true;
            }
        }

        // Check if there is an entity or terrain blocking the destination
        if (entityCollision || terrainCollision) {
            collision = true;
        }
    } else {
        // Out of bounds collision with the world boundary
        collision = true;
    }

    return collision;
}

// Helper: Calculate movement destination with special collision handling
std::tuple<int, int, int, float> calculateMovementDestination(
    entt::registry& registry, VoxelGrid& voxelGrid, const Position& position, Velocity& velocity,
    const PhysicsStats& physicsStats, float newVelocityX, float newVelocityY, float newVelocityZ) {
    float completionTime = calculateTimeToMove(newVelocityX, newVelocityY, newVelocityZ);
    int movingToX = position.x + getDirectionFromVelocity(newVelocityX);
    int movingToY = position.y + getDirectionFromVelocity(newVelocityY);
    int movingToZ = position.z + getDirectionFromVelocity(newVelocityZ);

    bool specialCollision;
    int newMovingToX, newMovingToY, newMovingToZ;
    std::tie(specialCollision, newMovingToX, newMovingToY, newMovingToZ) =
        hasSpecialCollision(registry, voxelGrid, position, movingToX, movingToY, movingToZ);

    if (specialCollision) {
        movingToX = newMovingToX;
        movingToY = newMovingToY;
        movingToZ = newMovingToZ;

        float newDirectionX = newMovingToX - position.x;
        float newDirectionY = newMovingToY - position.y;
        float newDirectionZ = newMovingToZ - position.z;

        velocity.vx = newDirectionX * (physicsStats.minSpeed / 2);
        velocity.vy = newDirectionY * (physicsStats.minSpeed / 2);
        velocity.vz = newDirectionZ * (physicsStats.minSpeed / 2);

        completionTime = calculateTimeToMove(velocity.vx, velocity.vy, velocity.vz);
    }

    return {movingToX, movingToY, movingToZ, completionTime};
}