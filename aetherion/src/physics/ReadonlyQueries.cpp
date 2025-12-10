
// =========================================================================
// ================ 1. READ-ONLY QUERY FUNCTIONS ================
// =========================================================================
// These functions perform read-only queries on game state without making
// any modifications. They are used for validation, collision detection,
// and state inspection.
// =========================================================================

#include "ReadonlyQueries.hpp"

#include <tuple>

#include <entt/entt.hpp>

// #include "MoveEntityEvent.hpp"
// #include "PhysicsEngine.hpp"
// #include "VoxelGrid.hpp"
#include "components/EntityTypeComponent.hpp"
#include "components/PhysicsComponents.hpp"
#include "components/TerrainComponents.hpp"

// Helper function to determine direction
int getDirection(float velocity) {
    if (velocity > 0.0f) {
        return 1;
    } else if (velocity < 0.0f) {
        return -1;
    } else {
        return 0;
    }
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


// inline bool hasCollision(entt::registry& registry, VoxelGrid& voxelGrid, entt::entity entity,
//                          int movingFromX, int movingFromY, int movingFromZ,
//                          int movingToX, int movingToY, int movingToZ, bool isTerrain) {
//     bool collision = false;
//     // Check if the movement is within bounds for x, y, z
//     if ((0 <= movingToX && movingToX < voxelGrid.width) &&
//         (0 <= movingToY && movingToY < voxelGrid.height) &&
//         (0 <= movingToZ && movingToZ < voxelGrid.depth)) {
//         int movingToEntityId = voxelGrid.getEntity(movingToX, movingToY, movingToZ);
//         bool terrainExists = voxelGrid.checkIfTerrainExists(movingToX, movingToY, movingToZ);

//         bool entityCollision = false;
//         if (movingToEntityId != -1) {
//             entityCollision = true;
//         }

//         bool terrainCollision = false;
//         if (terrainExists) {
//             EntityTypeComponent etc = getEntityTypeComponent(
//                 registry, voxelGrid, entity, movingFromX, movingFromY, movingFromZ, isTerrain);
//             EntityTypeComponent terrainEtc = voxelGrid.terrainGridRepository->getTerrainEntityType(
//                 movingToX, movingToY, movingToZ);
//             // Any terrain that is different than water
//             if (etc.mainType == static_cast<int>(EntityEnum::TERRAIN)) {
//                 terrainCollision = true;
//             } else if (terrainEtc.subType0 != static_cast<int>(TerrainEnum::EMPTY) &&
//                        terrainEtc.subType0 != static_cast<int>(TerrainEnum::WATER)) {
//                 terrainCollision = true;
//             }
//         }

//         // Check if there is an entity or terrain blocking the destination
//         if (entityCollision || terrainCollision) {
//             collision = true;
//         }
//     } else {
//         // Out of bounds collision with the world boundary
//         collision = true;
//     }

//     return collision;
// }


// std::tuple<bool, int, int, int> hasSpecialCollision(entt::registry& registry, VoxelGrid& voxelGrid,
//                                                     Position position, int movingToX, int movingToY,
//                                                     int movingToZ) {
//     bool collision = false;
//     int newMovingToX, newMovingToY, newMovingToZ;
//     // Check if the movement is within bounds for x, y, z
//     if ((0 <= movingToX && movingToX < voxelGrid.width) &&
//         (0 <= movingToY && movingToY < voxelGrid.height) &&
//         (0 <= movingToZ && movingToZ < voxelGrid.depth)) {
//         // int movingToEntityId = voxelGrid.getEntity(movingToX, movingToY, movingToZ);
//         bool movingToSameZTerrainExists =
//             voxelGrid.checkIfTerrainExists(movingToX, movingToY, movingToZ);
//         bool movingToBellowTerrainExists =
//             voxelGrid.checkIfTerrainExists(movingToX, movingToY, movingToZ - 1);

//         // Check if there is an entity or terrain blocking the destination
//         if (movingToSameZTerrainExists) {
//             EntityTypeComponent etc = voxelGrid.terrainGridRepository->getTerrainEntityType(
//                 movingToX, movingToY, movingToZ);
//             // subType1 == 1 means ramp_east
//             if (etc.subType1 == 1) {
//                 collision = true;
//                 newMovingToX = movingToX - 1;
//                 newMovingToY = movingToY;
//                 newMovingToZ = movingToZ + 1;
//             } else if (etc.subType1 == 2) {
//                 collision = true;
//                 newMovingToX = movingToX + 1;
//                 newMovingToY = movingToY;
//                 newMovingToZ = movingToZ + 1;
//             } else if (etc.subType1 == 7) {
//                 collision = true;
//                 newMovingToX = movingToX;
//                 newMovingToY = movingToY - 1;
//                 newMovingToZ = movingToZ + 1;
//             } else if (etc.subType1 == 8) {
//                 collision = true;
//                 newMovingToX = movingToX;
//                 newMovingToY = movingToY + 1;
//                 newMovingToZ = movingToZ + 1;
//             }
//         } else if (movingToBellowTerrainExists) {
//             EntityTypeComponent etc = voxelGrid.terrainGridRepository->getTerrainEntityType(
//                 movingToX, movingToY, movingToZ - 1);
//             // subType1 == 1 means ramp_east
//             if (etc.subType1 == 1) {
//                 collision = true;
//                 newMovingToX = movingToX + 1;
//                 newMovingToY = movingToY;
//                 newMovingToZ = movingToZ - 1;
//             } else if (etc.subType1 == 2) {
//                 collision = true;
//                 newMovingToX = movingToX - 1;
//                 newMovingToY = movingToY;
//                 newMovingToZ = movingToZ - 1;
//             } else if (etc.subType1 == 7) {
//                 collision = true;
//                 newMovingToX = movingToX;
//                 newMovingToY = movingToY + 1;
//                 newMovingToZ = movingToZ - 1;
//             } else if (etc.subType1 == 8) {
//                 collision = true;
//                 newMovingToX = movingToX;
//                 newMovingToY = movingToY - 1;
//                 newMovingToZ = movingToZ - 1;
//             }
//         }
//     }

//     return std::make_tuple(collision, newMovingToX, newMovingToY, newMovingToZ);
// }



// bool checkIfCanFall(entt::registry& registry, VoxelGrid& voxelGrid, int i, int j, int k) {
//     // return false;

//     int movingToEntityId = voxelGrid.getEntity(i, j, k - 1);
//     bool canFallOnterrain = false;
//     if (voxelGrid.checkIfTerrainExists(i, j, k - 1)) {
//         EntityTypeComponent etc =
//             voxelGrid.terrainGridRepository->getTerrainEntityType(i, j, k - 1);
//         // Any terrain that is different than water
//         if (etc.subType0 == 1) {
//             canFallOnterrain = true;
//         }
//     } else {
//         canFallOnterrain = true;
//     }

//     return (k > 0 and movingToEntityId == -1 and canFallOnterrain);
// }
