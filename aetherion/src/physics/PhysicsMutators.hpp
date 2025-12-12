#ifndef PHYSICS_MUTATORS_H
#define PHYSICS_MUTATORS_H

#include <entt/entt.hpp>

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
void checkAndConvertSoftEmptyIntoWater(entt::registry& registry, VoxelGrid& voxelGrid,
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
void checkAndConvertSoftEmptyIntoVapor(entt::registry& registry, VoxelGrid& voxelGrid,
                                       int terrainId, int x, int y, int z) {
    if (getTypeAndCheckSoftEmpty(registry, voxelGrid, terrainId, x, y, z)) {
        convertSoftEmptyIntoVapor(registry, voxelGrid, terrainId, x, y, z);
    }
}

// Helper: Delete entity or convert to empty
void deleteEntityOrConvertInEmpty(entt::registry& registry, entt::dispatcher& dispatcher,
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
void printTerrainDiagnostics(entt::registry& registry, VoxelGrid& voxelGrid,
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
entt::entity reviveColdTerrainEntities(entt::registry& registry, VoxelGrid& voxelGrid,
                                       entt::dispatcher& dispatcher, Position& positionOfEntt,
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

#endif  // PHYSICS_MANAGER_H