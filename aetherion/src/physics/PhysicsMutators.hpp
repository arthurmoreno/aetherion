#ifndef PHYSICS_MUTATORS_H
#define PHYSICS_MUTATORS_H

#include <entt/entt.hpp>

#include "voxelgrid/VoxelGrid.hpp"
#include "physics/ReadonlyQueries.hpp"

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

#endif  // PHYSICS_MANAGER_H