#ifndef READONLY_QUERIES_HPP
#define READONLY_QUERIES_HPP

#include <entt/entt.hpp>
#include <stdexcept>
#include <tuple>

#include "components/EntityTypeComponent.hpp"
#include "components/PhysicsComponents.hpp"
#include "components/TerrainComponents.hpp"
#include "physics/Collision.hpp"
#include "physics/PhysicalMath.hpp"
#include "voxelgrid/VoxelGrid.hpp"

// Forward declarations
struct MoveSolidEntityEvent;

// Helper function to determine direction from velocity
int getDirectionFromVelocity(float velocity);

// Helper: Get DirectionEnum from velocities
DirectionEnum getDirectionFromVelocities(float velocityX, float velocityY, float velocityZ);

// Helper: Get EntityTypeComponent for terrain or ECS entity
inline EntityTypeComponent getEntityType(entt::registry& registry, VoxelGrid& voxelGrid,
                                         entt::entity entity, const Position& position,
                                         bool isTerrain) {
    if (isTerrain) {
        return voxelGrid.terrainGridRepository->getTerrainEntityType(position.x, position.y,
                                                                     position.z);
    }

    EntityTypeComponent* etc = registry.try_get<EntityTypeComponent>(entity);
    if (etc) {
        return *etc;
    }

    throw std::runtime_error("Missing EntityTypeComponent in getEntityType");
}

// Helper: Get matter state for entity or terrain
inline MatterState getMatterState(entt::registry& registry, VoxelGrid& voxelGrid,
                                  entt::entity entity, const Position& position, bool isTerrain) {
    if (!isTerrain) {
        StructuralIntegrityComponent* sic = registry.try_get<StructuralIntegrityComponent>(entity);
        if (sic) {
            return sic->matterState;
        }
        return MatterState::SOLID;
    } else {
        StructuralIntegrityComponent bellowTerrainSic =
            voxelGrid.terrainGridRepository->getTerrainStructuralIntegrity(position.x, position.y,
                                                                           position.z - 1);
        return bellowTerrainSic.matterState;
    }
}

// Helper: Get EntityTypeComponent for terrain or regular entity
inline EntityTypeComponent getEntityTypeComponent(entt::registry& registry, VoxelGrid& voxelGrid,
                                                  entt::entity entity, int x, int y, int z,
                                                  bool isTerrain) {
    if (isTerrain) {
        return voxelGrid.terrainGridRepository->getTerrainEntityType(x, y, z);
    } else {
        EntityTypeComponent* etc = registry.try_get<EntityTypeComponent>(entity);
        if (etc) {
            return *etc;
        }
        return EntityTypeComponent{};
    }
}

bool checkIfCanFall(entt::registry& registry, VoxelGrid& voxelGrid, int i, int j, int k);

bool getTypeAndCheckSoftEmpty(entt::registry& registry, VoxelGrid& voxelGrid, int terrainId, int x,
                              int y, int z);

bool isTerrainSoftEmpty(EntityTypeComponent& terrainType);

// Helper: Check if position below entity is stable
inline bool checkBelowStability(entt::registry& registry, VoxelGrid& voxelGrid,
                                const Position& position) {
    int bellowEntityId = voxelGrid.getEntity(position.x, position.y, position.z - 1);
    bool bellowTerrainExists =
        voxelGrid.checkIfTerrainExists(position.x, position.y, position.z - 1);

    if (bellowEntityId != -1) {
        entt::entity bellowEntity = static_cast<entt::entity>(bellowEntityId);
        StructuralIntegrityComponent* bellowEntitySic =
            registry.try_get<StructuralIntegrityComponent>(bellowEntity);
        return bellowEntitySic && bellowEntitySic->canStackEntities;
    } else if (bellowTerrainExists) {
        StructuralIntegrityComponent bellowTerrainSic =
            voxelGrid.terrainGridRepository->getTerrainStructuralIntegrity(position.x, position.y,
                                                                           position.z - 1);
        return bellowTerrainSic.canStackEntities;
    }
    return false;
}

std::tuple<int, int, int, float> calculateMovementDestination(
    entt::registry& registry, VoxelGrid& voxelGrid, const Position& position, Velocity& velocity,
    const PhysicsStats& physicsStats, float newVelocityX, float newVelocityY, float newVelocityZ);

bool hasCollision(entt::registry& registry, VoxelGrid& voxelGrid, entt::entity entity,
                  int movingFromX, int movingFromY, int movingFromZ, int movingToX, int movingToY,
                  int movingToZ, bool isTerrain);

#endif  // READONLY_QUERIES_HPP
