#ifndef PHYSICS_READONLY_QUERIES_HPP
#define PHYSICS_READONLY_QUERIES_HPP

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
                                                                           position.z);
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

// Helper: Print exhaustive terrain diagnostics for debugging
inline void printTerrainDiagnostics(entt::registry& registry, VoxelGrid& voxelGrid,
                                    entt::entity invalidTerrain, const Position& position,
                                    const EntityTypeComponent& terrainType, int vaporMatter) {
    int invalidTerrainId = static_cast<int>(invalidTerrain);

    std::cout << "\n========== TERRAIN REVIVAL FAILED - DETAILED DIAGNOSTICS ==========\n";
    std::cout << "[printTerrainDiagnostics] Entity " << invalidTerrainId << "\n";
    std::cout << "Position: (" << position.x << ", " << position.y << ", " << position.z << ")\n";

    // EntityTypeComponent
    std::cout << "\n--- EntityTypeComponent ---\n";
    std::cout << "  mainType: " << terrainType.mainType
              << " (expected TERRAIN=" << static_cast<int>(EntityEnum::TERRAIN) << ")\n";
    std::cout << "  subType0: " << terrainType.subType0 << "\n";
    std::cout << "  subType1: " << terrainType.subType1 << "\n";

    // MatterContainer
    MatterContainer matterContainer = voxelGrid.terrainGridRepository->getTerrainMatterContainer(
        position.x, position.y, position.z);
    std::cout << "\n--- MatterContainer ---\n";
    std::cout << "  TerrainMatter: " << matterContainer.TerrainMatter << "\n";
    std::cout << "  WaterMatter: " << matterContainer.WaterMatter << "\n";
    std::cout << "  WaterVapor: " << matterContainer.WaterVapor << " (checked: " << vaporMatter
              << ")\n";
    std::cout << "  BioMassMatter: " << matterContainer.BioMassMatter << "\n";

    // PhysicsStats
    PhysicsStats physicsStats =
        voxelGrid.terrainGridRepository->getPhysicsStats(position.x, position.y, position.z);
    std::cout << "\n--- PhysicsStats ---\n";
    std::cout << "  mass: " << physicsStats.mass << "\n";
    std::cout << "  maxSpeed: " << physicsStats.maxSpeed << "\n";
    std::cout << "  minSpeed: " << physicsStats.minSpeed << "\n";
    std::cout << "  heat: " << physicsStats.heat << "\n";

    // StructuralIntegrityComponent
    StructuralIntegrityComponent structuralIntegrity =
        voxelGrid.terrainGridRepository->getTerrainStructuralIntegrity(position.x, position.y,
                                                                       position.z);
    std::cout << "\n--- StructuralIntegrityComponent ---\n";
    std::cout << "  matterState: " << static_cast<int>(structuralIntegrity.matterState) << "\n";
    std::cout << "  canStackEntities: " << structuralIntegrity.canStackEntities << "\n";
    std::cout << "  maxLoadCapacity: " << structuralIntegrity.maxLoadCapacity << "\n";

    // Storage info
    std::optional<int> terrainIdInStorage =
        voxelGrid.terrainGridRepository->getTerrainIdIfExists(position.x, position.y, position.z);
    std::cout << "\n--- Storage Info ---\n";
    std::cout << "  Terrain exists in storage: " << (terrainIdInStorage.has_value() ? "YES" : "NO")
              << "\n";
    if (terrainIdInStorage.has_value()) {
        std::cout << "  Terrain ID in storage: " << terrainIdInStorage.value() << "\n";
    }
    std::cout << "  Invalid entity being revived: " << invalidTerrainId << "\n";
    std::cout << "  Entity valid in registry: " << registry.valid(invalidTerrain) << "\n";

    std::cout << "\n--- Revival Failure Reason ---\n";
    if (terrainType.mainType != static_cast<int>(EntityEnum::TERRAIN)) {
        std::cout << "  REASON: mainType is not TERRAIN\n";
    }
    if (vaporMatter <= 0) {
        std::cout << "  REASON: vaporMatter is " << vaporMatter << " (must be > 0)\n";
    }
    std::cout << "==================================================================\n\n";
}

inline std::pair<float, bool> calculateVelocityAfterGravityStep(entt::registry& registry,
                                                                VoxelGrid& voxelGrid, int i, int j,
                                                                int k, float velocityZ, int dt) {
    float gravity = PhysicsManager::Instance()->getGravity();
    float newVelocityZ;

    if (velocityZ > 0.0f || checkIfCanFall(registry, voxelGrid, i, j, k)) {
        newVelocityZ = velocityZ - gravity * dt;
    } else {
        newVelocityZ = velocityZ;
    }

    bool willStop = false;
    if (velocityZ * newVelocityZ < 0.0f) {
        newVelocityZ = 0.0f;
        willStop = true;
    }

    return std::make_pair(newVelocityZ, willStop);
}

#endif  // PHYSICS_READONLY_QUERIES_HPP
