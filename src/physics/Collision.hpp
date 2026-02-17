#ifndef PHYSICAL_COLLISION_HPP
#define PHYSICAL_COLLISION_HPP

#include <entt/entt.hpp>

#include "components/EntityTypeComponent.hpp"
#include "voxelgrid/VoxelGrid.hpp"

inline std::tuple<bool, int, int, int> hasSpecialCollision(entt::registry& registry,
                                                           VoxelGrid& voxelGrid, Position position,
                                                           int movingToX, int movingToY,
                                                           int movingToZ) {
    bool collision = false;
    int newMovingToX, newMovingToY, newMovingToZ;
    // Check if the movement is within bounds for x, y, z
    if ((0 <= movingToX && movingToX < voxelGrid.width) &&
        (0 <= movingToY && movingToY < voxelGrid.height) &&
        (0 <= movingToZ && movingToZ < voxelGrid.depth)) {
        // int movingToEntityId = voxelGrid.getEntity(movingToX, movingToY, movingToZ);
        bool movingToSameZTerrainExists =
            voxelGrid.checkIfTerrainExists(movingToX, movingToY, movingToZ);
        bool movingToBellowTerrainExists =
            voxelGrid.checkIfTerrainExists(movingToX, movingToY, movingToZ - 1);

        // Check if there is an entity or terrain blocking the destination
        if (movingToSameZTerrainExists) {
            EntityTypeComponent etc = voxelGrid.terrainGridRepository->getTerrainEntityType(
                movingToX, movingToY, movingToZ);
            // subType1 == 1 means ramp_east
            if (etc.subType1 == 1) {
                collision = true;
                newMovingToX = movingToX - 1;
                newMovingToY = movingToY;
                newMovingToZ = movingToZ + 1;
            } else if (etc.subType1 == 2) {
                collision = true;
                newMovingToX = movingToX + 1;
                newMovingToY = movingToY;
                newMovingToZ = movingToZ + 1;
            } else if (etc.subType1 == 7) {
                collision = true;
                newMovingToX = movingToX;
                newMovingToY = movingToY - 1;
                newMovingToZ = movingToZ + 1;
            } else if (etc.subType1 == 8) {
                collision = true;
                newMovingToX = movingToX;
                newMovingToY = movingToY + 1;
                newMovingToZ = movingToZ + 1;
            }
        } else if (movingToBellowTerrainExists) {
            EntityTypeComponent etc = voxelGrid.terrainGridRepository->getTerrainEntityType(
                movingToX, movingToY, movingToZ - 1);
            // subType1 == 1 means ramp_east
            if (etc.subType1 == 1) {
                collision = true;
                newMovingToX = movingToX + 1;
                newMovingToY = movingToY;
                newMovingToZ = movingToZ - 1;
            } else if (etc.subType1 == 2) {
                collision = true;
                newMovingToX = movingToX - 1;
                newMovingToY = movingToY;
                newMovingToZ = movingToZ - 1;
            } else if (etc.subType1 == 7) {
                collision = true;
                newMovingToX = movingToX;
                newMovingToY = movingToY + 1;
                newMovingToZ = movingToZ - 1;
            } else if (etc.subType1 == 8) {
                collision = true;
                newMovingToX = movingToX;
                newMovingToY = movingToY - 1;
                newMovingToZ = movingToZ - 1;
            }
        }
    }

    return std::make_tuple(collision, newMovingToX, newMovingToY, newMovingToZ);
}

#endif  // PHYSICAL_COLLISION_HPP