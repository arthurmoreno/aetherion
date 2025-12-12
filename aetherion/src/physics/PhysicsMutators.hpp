#ifndef PHYSICS_MUTATORS_H
#define PHYSICS_MUTATORS_H

#include <entt/entt.hpp>

#include "components/EntityTypeComponent.hpp"
#include "components/PhysicsComponents.hpp"
#include "physics/PhysicsExceptions.hpp"
#include "physics/ReadonlyQueries.hpp"
#include "voxelgrid/VoxelGrid.hpp"

// Helper: Clean up zero velocity
inline void cleanupZeroVelocity(entt::registry& registry, VoxelGrid& voxelGrid, entt::entity entity,
                                const Position& position, const Velocity& velocity,
                                bool isTerrain) {
    if (velocity.vx == 0 && velocity.vy == 0 && velocity.vz == 0) {
        if (isTerrain) {
            // std::cout << "[cleanupZeroVelocity] Zeroing Velocity from Terrain!\n";
            voxelGrid.terrainGridRepository->setVelocity(position.x, position.y, position.z,
                                                         {0.0f, 0.0f, 0.0f});
            // voxelGrid.terrainGridRepository->setTerrainId(position.x, position.y, position.z,
            // static_cast<int>(TerrainIdTypeEnum::ON_GRID_STORAGE));
            // registry.remove<Velocity>(entity);
            // if (registry.valid(entity)) {
            //     registry.destroy(entity);
            // }
        } else {
            registry.remove<Velocity>(entity);
        }
    }
}

// Helper: Convert terrain into soft empty
static void convertIntoSoftEmpty(entt::registry& registry, entt::entity& terrain) {
    EntityTypeComponent* terrainType = registry.try_get<EntityTypeComponent>(terrain);
    bool shouldEmplaceTerrainType{terrainType == nullptr};
    if (terrainType == nullptr) {
        terrainType = new EntityTypeComponent();
    }
    terrainType->mainType = static_cast<int>(EntityEnum::TERRAIN);
    terrainType->subType0 = static_cast<int>(TerrainEnum::EMPTY);
    terrainType->subType1 = 0;
    if (shouldEmplaceTerrainType) {
        registry.emplace<EntityTypeComponent>(terrain, *terrainType);
    }

    StructuralIntegrityComponent* terrainSI =
        registry.try_get<StructuralIntegrityComponent>(terrain);
    bool shouldEmplaceTerrainSI{terrainSI == nullptr};
    if (terrainSI == nullptr) {
        terrainSI = new StructuralIntegrityComponent();
    }
    terrainSI->canStackEntities = false;
    terrainSI->maxLoadCapacity = -1;
    terrainSI->matterState = MatterState::GAS;
    if (shouldEmplaceTerrainSI) {
        registry.emplace<StructuralIntegrityComponent>(terrain, *terrainSI);
    }
}

// Helper: Set empty water components in EnTT registry
static void setEmptyWaterComponentsEnTT(entt::registry& registry, entt::entity& terrain,
                                        MatterState matterState) {
    EntityTypeComponent* terrainType = registry.try_get<EntityTypeComponent>(terrain);
    bool shouldEmplaceTerrainType{terrainType == nullptr};
    if (terrainType == nullptr) {
        terrainType = new EntityTypeComponent();
    }
    terrainType->mainType = static_cast<int>(EntityEnum::TERRAIN);
    terrainType->subType0 = static_cast<int>(TerrainEnum::WATER);
    terrainType->subType1 = 0;
    if (shouldEmplaceTerrainType) {
        registry.emplace<EntityTypeComponent>(terrain, *terrainType);
    }

    StructuralIntegrityComponent* terrainSI =
        registry.try_get<StructuralIntegrityComponent>(terrain);
    bool shouldEmplaceTerrainSI{terrainSI == nullptr};
    if (terrainSI == nullptr) {
        terrainSI = new StructuralIntegrityComponent();
    }
    terrainSI->canStackEntities = false;
    terrainSI->maxLoadCapacity = -1;
    terrainSI->matterState = matterState;
    if (shouldEmplaceTerrainSI) {
        registry.emplace<StructuralIntegrityComponent>(terrain, *terrainSI);
    }

    MatterContainer* terrainMC = registry.try_get<MatterContainer>(terrain);
    bool shouldEmplaceTerrainMC{terrainMC == nullptr};
    if (terrainMC == nullptr) {
        terrainMC = new MatterContainer();
    }
    terrainMC->TerrainMatter = 0;
    terrainMC->WaterMatter = 0;
    terrainMC->WaterVapor = 0;
    terrainMC->BioMassMatter = 0;
    if (shouldEmplaceTerrainMC) {
        registry.emplace<MatterContainer>(terrain, *terrainMC);
    }
}

// Helper: Set empty water components in terrain storage
static void setEmptyWaterComponentsStorage(entt::registry& registry, VoxelGrid& voxelGrid,
                                           int terrainId, int x, int y, int z,
                                           MatterState matterState) {
    // Part 1: Set EntityTypeComponent
    EntityTypeComponent* terrainType = new EntityTypeComponent();
    terrainType->mainType = static_cast<int>(EntityEnum::TERRAIN);
    terrainType->subType0 = static_cast<int>(TerrainEnum::WATER);
    terrainType->subType1 = 0;
    voxelGrid.terrainGridRepository->setTerrainEntityType(x, y, z, *terrainType);

    // Part 2: Set StructuralIntegrityComponent
    StructuralIntegrityComponent* terrainSI = new StructuralIntegrityComponent();
    terrainSI->canStackEntities = false;
    terrainSI->maxLoadCapacity = -1;
    terrainSI->matterState = matterState;
    voxelGrid.terrainGridRepository->setTerrainStructuralIntegrity(x, y, z, *terrainSI);

    // Part 3: Set MatterContainer
    MatterContainer* terrainMC = new MatterContainer();
    terrainMC->TerrainMatter = 0;
    terrainMC->WaterMatter = 0;
    terrainMC->WaterVapor = 0;
    terrainMC->BioMassMatter = 0;
    voxelGrid.terrainGridRepository->setTerrainMatterContainer(x, y, z, *terrainMC);
}

// Helper: Convert soft empty terrain into water
static void convertSoftEmptyIntoWater(entt::registry& registry, VoxelGrid& voxelGrid, int terrainId,
                                      int x, int y, int z) {
    if (terrainId == static_cast<int>(TerrainIdTypeEnum::NONE)) {
        // Create new terrain entity for the empty voxel
    } else if (terrainId == static_cast<int>(TerrainIdTypeEnum::ON_GRID_STORAGE)) {
        setEmptyWaterComponentsStorage(registry, voxelGrid, terrainId, x, y, z,
                                       MatterState::LIQUID);
        // Create new terrain entity for the completely empty voxel
    } else {
        // Convert existing soft empty terrain entity to water
        auto terrain = static_cast<entt::entity>(terrainId);
        setEmptyWaterComponentsEnTT(registry, terrain, MatterState::LIQUID);
    }
}

// Helper: Check and convert soft empty into water
inline void checkAndConvertSoftEmptyIntoWater(entt::registry& registry, VoxelGrid& voxelGrid,
                                              int terrainId, int x, int y, int z) {
    if (getTypeAndCheckSoftEmpty(registry, voxelGrid, terrainId, x, y, z)) {
        convertSoftEmptyIntoWater(registry, voxelGrid, terrainId, x, y, z);
    }
}

// Helper: Convert soft empty terrain into vapor
static void convertSoftEmptyIntoVapor(entt::registry& registry, VoxelGrid& voxelGrid, int terrainId,
                                      int x, int y, int z) {
    std::cout << "[convertSoftEmptyIntoVapor] Just marking a checkpoint on logs." << std::endl;
    // TODO: This might be involved in the bug I am debugging.
    // setEmptyWaterComponents(registry, terrain, MatterState::GAS);
}

// Helper: Check and convert soft empty into vapor
inline void checkAndConvertSoftEmptyIntoVapor(entt::registry& registry, VoxelGrid& voxelGrid,
                                              int terrainId, int x, int y, int z) {
    if (getTypeAndCheckSoftEmpty(registry, voxelGrid, terrainId, x, y, z)) {
        convertSoftEmptyIntoVapor(registry, voxelGrid, terrainId, x, y, z);
    }
}

// Helper: Delete entity or convert to empty
inline void deleteEntityOrConvertInEmpty(entt::registry& registry, entt::dispatcher& dispatcher,
                                         entt::entity& terrain) {
    TileEffectsList* terrainEffectsList = registry.try_get<TileEffectsList>(terrain);
    if (terrainEffectsList == nullptr ||
        (terrainEffectsList && terrainEffectsList->tileEffectsIDs.empty())) {
        dispatcher.enqueue<KillEntityEvent>(terrain);
    } else {
        // Convert into empty terrain because there are effects being processed
        std::cout << "terrainEffectsList && terrainEffectsList->tileEffectsIDs.empty(): is "
                     "False... converting into soft empty"
                  << std::endl;
        convertIntoSoftEmpty(registry, terrain);
    }
}

// Helper: Print exhaustive terrain diagnostics for debugging
inline void printTerrainDiagnostics(entt::registry& registry, VoxelGrid& voxelGrid,
                                    entt::entity invalidTerrain, const Position& position,
                                    const EntityTypeComponent& terrainType, int vaporMatter) {
    int invalidTerrainId = static_cast<int>(invalidTerrain);

    std::cout << "\n========== TERRAIN REVIVAL FAILED - DETAILED DIAGNOSTICS ==========\n";
    std::cout << "[reviveColdTerrainEntities] Entity " << invalidTerrainId
              << " cannot be revived\n";
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

// Helper: Revive cold terrain entities (e.g., vapor that went dormant)
inline entt::entity reviveColdTerrainEntities(entt::registry& registry, VoxelGrid& voxelGrid,
                                              entt::dispatcher& dispatcher,
                                              Position& positionOfEntt,
                                              entt::entity& invalidTerrain) {
    int invalidTerrainId = static_cast<int>(invalidTerrain);
    Position positionOnTerrainGrid =
        voxelGrid.terrainGridRepository->getPositionOfEntt(invalidTerrain);

    std::cout << "[processPhysics] Found position of entity " << invalidTerrainId
              << " in TerrainGridRepository at (" << positionOfEntt.x << ", " << positionOfEntt.y
              << ", " << positionOfEntt.z << ")" << " - checking if vapor terrain needs revival"
              << std::endl;

    // Check if this is vapor terrain that needs to be revived
    EntityTypeComponent terrainType = voxelGrid.terrainGridRepository->getTerrainEntityType(
        positionOfEntt.x, positionOfEntt.y, positionOfEntt.z);
    int vaporMatter = voxelGrid.terrainGridRepository->getVaporMatter(
        positionOfEntt.x, positionOfEntt.y, positionOfEntt.z);

    if (terrainType.mainType == static_cast<int>(EntityEnum::TERRAIN) && vaporMatter > 0) {
        std::cout << "[processPhysics] Reviving cold vapor terrain at (" << positionOfEntt.x << ", "
                  << positionOfEntt.y << ", " << positionOfEntt.z
                  << ") with vapor matter: " << vaporMatter << std::endl;

        // Revive the terrain by ensuring it's active in ECS
        entt::entity entity = voxelGrid.terrainGridRepository->ensureActive(
            positionOfEntt.x, positionOfEntt.y, positionOfEntt.z);

        std::cout << "[processPhysics] Revived vapor terrain as entity " << static_cast<int>(entity)
                  << std::endl;
        // Continue processing with the newly revived entity (don't skip)
        return entity;
    } else {
        // Print detailed diagnostics before throwing exception

        MatterContainer matterContainer =
            voxelGrid.terrainGridRepository->getTerrainMatterContainer(
                positionOfEntt.x, positionOfEntt.y, positionOfEntt.z);
        if (matterContainer.WaterVapor == 0 && matterContainer.WaterMatter == 0 &&
            terrainType.mainType == static_cast<int>(EntityEnum::TERRAIN) &&
            terrainType.subType0 == static_cast<int>(TerrainEnum::WATER)) {
            std::cout << "[reviveColdTerrainEntities] WARNING: Discrepancy in vapor matter values! "
                         "VoxelGrid reports "
                      << vaporMatter << ", but MatterContainer has " << matterContainer.WaterVapor
                      << std::endl;
            registry.destroy(invalidTerrain);
            voxelGrid.terrainGridRepository->setTerrainId(
                positionOfEntt.x, positionOfEntt.y, positionOfEntt.z,
                static_cast<int>(TerrainIdTypeEnum::NONE));
            voxelGrid.terrainGridRepository->setTerrainEntityType(
                positionOfEntt.x, positionOfEntt.y, positionOfEntt.z,
                EntityTypeComponent{static_cast<int>(EntityEnum::TERRAIN),
                                    static_cast<int>(TerrainEnum::EMPTY), 0});
            std::cout << "[reviveColdTerrainEntities] Converted terrain entity " << invalidTerrainId
                      << " into empty terrain due to zero water matter." << std::endl;
            throw aetherion::InvalidEntityException(
                "Entity with Velocity had zero vapor matter; converted to empty terrain");

        } else if (terrainType.mainType == static_cast<int>(EntityEnum::TERRAIN) &&
                   terrainType.subType0 == static_cast<int>(TerrainEnum::EMPTY)) {
            throw aetherion::InvalidEntityException("Terrain is EMPTY; cannot be revived");

        } else {
            printTerrainDiagnostics(registry, voxelGrid, invalidTerrain, positionOfEntt,
                                    terrainType, vaporMatter);
            throw std::runtime_error(
                "Entity with Velocity is invalid and cannot be revived; skipping");
        }
    }
}

// Helper: Create a new vapor terrain entity with all required components
inline entt::entity createVaporTerrainEntity(entt::registry& registry, VoxelGrid& voxelGrid, int x,
                                             int y, int z, int vaporAmount) {
    auto newVaporEntity = registry.create();
    Position newPosition = {x, y, z, DirectionEnum::DOWN};

    EntityTypeComponent newType = {};
    newType.mainType = 0;  // Terrain type
    newType.subType0 = 1;  // Water terrain (vapor)
    newType.subType1 = 0;

    MatterContainer newMatterContainer = {};
    newMatterContainer.WaterVapor = vaporAmount;
    newMatterContainer.WaterMatter = 0;

    PhysicsStats newPhysicsStats = {};
    newPhysicsStats.mass = 0.1;
    newPhysicsStats.maxSpeed = 10;
    newPhysicsStats.minSpeed = 0.0;

    StructuralIntegrityComponent newStructuralIntegrityComponent = {};
    newStructuralIntegrityComponent.canStackEntities = false;
    newStructuralIntegrityComponent.maxLoadCapacity = -1;
    newStructuralIntegrityComponent.matterState = MatterState::GAS;

    voxelGrid.terrainGridRepository->setPosition(x, y, z, newPosition);
    voxelGrid.terrainGridRepository->setTerrainEntityType(x, y, z, newType);
    voxelGrid.terrainGridRepository->setTerrainMatterContainer(x, y, z, newMatterContainer);
    voxelGrid.terrainGridRepository->setTerrainStructuralIntegrity(x, y, z,
                                                                   newStructuralIntegrityComponent);
    voxelGrid.terrainGridRepository->setPhysicsStats(x, y, z, newPhysicsStats);
    int newTerrainId = static_cast<int>(newVaporEntity);
    voxelGrid.terrainGridRepository->setTerrainId(x, y, z, newTerrainId);

    return newVaporEntity;
}

// Helper: Create water terrain entity from falling water
inline void createWaterTerrainFromFall(entt::registry& registry, VoxelGrid& voxelGrid, int x, int y,
                                       int z, double fallingAmount, entt::entity sourceEntity) {
    // Lock for atomic state change
    voxelGrid.terrainGridRepository->lockTerrainGrid();

    // Create a new water tile
    entt::entity newWaterEntity = registry.create();
    Position newPosition = {x, y, z, DirectionEnum::DOWN};

    EntityTypeComponent newType = {};
    newType.mainType = static_cast<int>(EntityEnum::TERRAIN);
    newType.subType0 = static_cast<int>(TerrainEnum::WATER);
    newType.subType1 = 0;

    MatterContainer newMatterContainer = {};
    newMatterContainer.WaterMatter = fallingAmount;
    newMatterContainer.WaterVapor = 0;

    PhysicsStats newPhysicsStats = {};
    newPhysicsStats.mass = 20;
    newPhysicsStats.maxSpeed = 10;
    newPhysicsStats.minSpeed = 0.0;

    Velocity newVelocity = {};

    StructuralIntegrityComponent newStructuralIntegrityComponent = {};
    newStructuralIntegrityComponent.canStackEntities = false;
    newStructuralIntegrityComponent.maxLoadCapacity = -1;
    newStructuralIntegrityComponent.matterState = MatterState::LIQUID;

    registry.emplace<Position>(newWaterEntity, newPosition);
    registry.emplace<Velocity>(newWaterEntity, newVelocity);
    registry.emplace<EntityTypeComponent>(newWaterEntity, newType);
    registry.emplace<MatterContainer>(newWaterEntity, newMatterContainer);
    registry.emplace<StructuralIntegrityComponent>(newWaterEntity, newStructuralIntegrityComponent);
    registry.emplace<PhysicsStats>(newWaterEntity, newPhysicsStats);

    voxelGrid.setTerrain(x, y, z, static_cast<int>(newWaterEntity));

    // Update source entity's water matter
    auto& sourceMatterContainer = registry.get<MatterContainer>(sourceEntity);
    sourceMatterContainer.WaterMatter -= fallingAmount;

    // Cleanup source entity if depleted
    if (sourceMatterContainer.WaterVapor <= 0 && sourceMatterContainer.WaterMatter <= 0) {
        auto& sourceType = registry.get<EntityTypeComponent>(sourceEntity);
        if (sourceType.mainType == static_cast<int>(EntityEnum::TERRAIN) &&
            sourceType.subType0 == static_cast<int>(TerrainEnum::WATER)) {
            auto& sourcePos = registry.get<Position>(sourceEntity);
            voxelGrid.setTerrain(sourcePos.x, sourcePos.y, sourcePos.z, -1);
            registry.destroy(sourceEntity);
        }
    }

    voxelGrid.terrainGridRepository->unlockTerrainGrid();
}

// Helper function to add vapor to existing tile above or create new vapor terrain
inline void addOrCreateVaporAbove(entt::registry& registry, VoxelGrid& voxelGrid, int x, int y,
                                  int z, int amount) {
    int terrainAboveId = voxelGrid.getTerrain(x, y, z + 1);

    if (terrainAboveId != static_cast<int>(TerrainIdTypeEnum::NONE)) {
        EntityTypeComponent typeAbove =
            voxelGrid.terrainGridRepository->getTerrainEntityType(x, y, z + 1);
        MatterContainer matterContainerAbove =
            voxelGrid.terrainGridRepository->getTerrainMatterContainer(x, y, z + 1);

        // Check if it's vapor based on MatterContainer
        if (typeAbove.mainType == static_cast<int>(EntityEnum::TERRAIN) &&
            typeAbove.subType0 == static_cast<int>(TerrainEnum::WATER) &&
            matterContainerAbove.WaterVapor >= 0 && matterContainerAbove.WaterMatter == 0) {
            matterContainerAbove.WaterVapor += amount;
            voxelGrid.terrainGridRepository->setTerrainMatterContainer(x, y, z + 1,
                                                                       matterContainerAbove);
        }
    } else {
        // No entity above; create new vapor terrain entity
        createVaporTerrainEntity(registry, voxelGrid, x, y, z + 1, amount);
    }
}

// Standalone helper: Create water terrain below vapor and clean up depleted vapor
inline void createWaterTerrainBelowVapor(entt::registry& registry, VoxelGrid& voxelGrid, int vaporX,
                                         int vaporY, int vaporZ, double condensationAmount,
                                         MatterContainer& vaporMatter) {
    // Create a new water tile below
    entt::entity newWaterEntity = registry.create();
    Position newPosition = {vaporX, vaporY, vaporZ - 1, DirectionEnum::DOWN};

    EntityTypeComponent newType = {};
    newType.mainType = static_cast<int>(EntityEnum::TERRAIN);
    newType.subType0 = static_cast<int>(TerrainEnum::WATER);
    newType.subType1 = 0;

    MatterContainer newMatterContainer = {};
    newMatterContainer.WaterMatter = condensationAmount;
    newMatterContainer.WaterVapor = 0;

    PhysicsStats newPhysicsStats = {};
    newPhysicsStats.mass = 20;
    newPhysicsStats.maxSpeed = 10;
    newPhysicsStats.minSpeed = 0.0;

    Velocity newVelocity = {};

    StructuralIntegrityComponent newStructuralIntegrityComponent = {};
    newStructuralIntegrityComponent.canStackEntities = false;
    newStructuralIntegrityComponent.maxLoadCapacity = -1;
    newStructuralIntegrityComponent.matterState = MatterState::LIQUID;

    registry.emplace<Position>(newWaterEntity, newPosition);
    registry.emplace<Velocity>(newWaterEntity, newVelocity);
    registry.emplace<EntityTypeComponent>(newWaterEntity, newType);
    registry.emplace<MatterContainer>(newWaterEntity, newMatterContainer);
    registry.emplace<StructuralIntegrityComponent>(newWaterEntity, newStructuralIntegrityComponent);
    registry.emplace<PhysicsStats>(newWaterEntity, newPhysicsStats);

    voxelGrid.setTerrain(vaporX, vaporY, vaporZ - 1, static_cast<int>(newWaterEntity));

    // Reduce vapor amount
    vaporMatter.WaterVapor -= condensationAmount;
    voxelGrid.terrainGridRepository->setTerrainMatterContainer(vaporX, vaporY, vaporZ, vaporMatter);

    // Cleanup vapor entity if depleted
    if (vaporMatter.WaterVapor <= 0) {
        int vaporTerrainId = voxelGrid.getTerrain(vaporX, vaporY, vaporZ);
        if (vaporTerrainId != static_cast<int>(TerrainIdTypeEnum::NONE)) {
            voxelGrid.setTerrain(vaporX, vaporY, vaporZ, static_cast<int>(TerrainIdTypeEnum::NONE));
            registry.destroy(static_cast<entt::entity>(vaporTerrainId));
        }
    }
}

// Helper: Clean up invalid terrain entity with proper tracking map removal
inline void cleanupInvalidTerrainEntity(entt::registry& registry, VoxelGrid& voxelGrid,
                                        entt::entity entity,
                                        const aetherion::InvalidEntityException& e) {
    std::cout << "[cleanupInvalidTerrainEntity] InvalidEntityException: " << e.what()
              << " - entity ID=" << static_cast<int>(entity) << std::endl;

    Position pos = voxelGrid.terrainGridRepository->getPositionOfEntt(entity);
    int entityId = static_cast<int>(entity);

    if (pos.x == -1 && pos.y == -1 && pos.z == -1) {
        std::cout << "[cleanupInvalidTerrainEntity] Could not find position of entity " << entityId
                  << " in TerrainGridRepository - just delete it." << std::endl;
        registry.destroy(entity);
        VoxelCoord key{pos.x, pos.y, pos.z};
        voxelGrid.terrainGridRepository->removeFromTrackingMaps(key, entity);
    } else {
        std::optional<int> terrainIdOnGrid =
            voxelGrid.terrainGridRepository->getTerrainIdIfExists(pos.x, pos.y, pos.z);
        if (terrainIdOnGrid.has_value()) {
            // Terrain exists on grid - remove from tracking maps and destroy entity
            std::cout
                << "[cleanupInvalidTerrainEntity] Terrain does exist at the given position in "
                   "repository - checking terrainIdOnGrid: "
                << terrainIdOnGrid.value() << " for entity ID: " << entityId
                << " at position: " << pos.x << ", " << pos.y << ", " << pos.z << std::endl;
            VoxelCoord key{pos.x, pos.y, pos.z};
            voxelGrid.terrainGridRepository->removeFromTrackingMaps(key, entity);
            registry.destroy(entity);
        } else {
            std::cout
                << "[cleanupInvalidTerrainEntity] Terrain does exist at the given position in "
                   "repository or grid ???"
                << entityId << " at position: " << pos.x << ", " << pos.y << ", " << pos.z
                << std::endl;
            registry.destroy(entity);
            voxelGrid.terrainGridRepository->setTerrainId(
                pos.x, pos.y, pos.z, static_cast<int>(TerrainIdTypeEnum::ON_GRID_STORAGE));
        }
    }
}

// Helper: Set vapor structural integrity properties
inline void setVaporSI(int x, int y, int z, VoxelGrid& voxelGrid) {
    StructuralIntegrityComponent terrainSI =
        voxelGrid.terrainGridRepository->getTerrainStructuralIntegrity(x, y, z);
    terrainSI.canStackEntities = false;
    terrainSI.maxLoadCapacity = -1;
    terrainSI.matterState = MatterState::GAS;
    voxelGrid.terrainGridRepository->setTerrainStructuralIntegrity(x, y, z, terrainSI);
}

// // Helper: Delete entity or convert to empty
// void deleteEntityOrConvertInEmpty(entt::registry& registry,
//                                                  entt::dispatcher& dispatcher,
//                                                  entt::entity& terrain) {
//     TileEffectsList* terrainEffectsList = registry.try_get<TileEffectsList>(terrain);
//     if (terrainEffectsList == nullptr ||
//         (terrainEffectsList && terrainEffectsList->tileEffectsIDs.empty())) {
//         dispatcher.enqueue<KillEntityEvent>(terrain);
//     } else {
//         // Convert into empty terrain because there are effects being processed
//         std::cout << "terrainEffectsList && terrainEffectsList->tileEffectsIDs.empty(): is "
//                      "False... converting into soft empty"
//                   << std::endl;
//         convertIntoSoftEmpty(registry, terrain);
//     }
// }

#endif  // PHYSICS_MUTATORS_HPP