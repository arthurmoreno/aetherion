#ifndef PHYSICS_MUTATORS_H
#define PHYSICS_MUTATORS_H

#include <entt/entt.hpp>
#include <iostream>
#include <sstream>
#include <stdexcept>

#include "components/EntityTypeComponent.hpp"
#include "components/PhysicsComponents.hpp"
#include "physics/PhysicsExceptions.hpp"
#include "physics/ReadonlyQueries.hpp"
#include "voxelgrid/VoxelGrid.hpp"
#include "physics/PhysicsEvents.hpp"
#include "physics/PhysicalMath.hpp"
#include "physics/PhysicsUtils.hpp"
#include "physics/PhysicsValidators.hpp"
#include "ecosystem/EcosystemEvents.hpp"
#include "components/MetabolismComponents.hpp"
#include "components/MovingComponent.hpp"
#include "terrain/TerrainGridLock.hpp"
#include <spdlog/spdlog.h>

// Forward declarations for functions used across categories
static void convertIntoSoftEmpty(entt::registry& registry, entt::entity& terrain);
static void setEmptyWaterComponentsEnTT(entt::registry& registry, entt::entity& terrain,
                                        MatterState matterState);
static void setEmptyWaterComponentsStorage(entt::registry& registry, VoxelGrid& voxelGrid,
                                           int terrainId, int x, int y, int z,
                                           MatterState matterState);
inline entt::entity createVaporTerrainEntity(entt::registry& registry, VoxelGrid& voxelGrid, int x,
                                             int y, int z, int vaporAmount);

// =========================================================================
// ================ 1. Direct Component Mutators = ================
// =========================================================================

/**
 * @brief Directly modifies the fields of a Velocity component.
 * @param velocity Reference to the Velocity component to modify.
 * @param newVx The new velocity on the X-axis.
 * @param newVy The new velocity on the Y-axis.
 * @param newVz The new velocity on the Z-axis.
 */
inline void updateEntityVelocity(Velocity& velocity, float newVx, float newVy, float newVz) {
    velocity.vx = newVx;
    velocity.vy = newVy;
    velocity.vz = newVz;
}

/**
 * @brief Ensures a terrain entity has a Position component, adding one if it's missing.
 * @details This is a safety check for terrain entities (e.g., vapor) that might be processed
 * by physics before being fully initialized. It fetches the position from the TerrainGridRepository.
 * @param registry The entt::registry.
 * @param voxelGrid The VoxelGrid for accessing the terrain repository.
 * @param entity The entity to check.
 * @param isTerrain Flag indicating if the entity is a terrain entity.
 * @throws std::runtime_error if the entity is missing a position and it cannot be found in the repository.
 */
inline void ensurePositionComponentForTerrain(entt::registry& registry, VoxelGrid& voxelGrid,
                                              entt::entity entity, bool isTerrain) {
    // For terrain entities, verify they have Position component
    // This ensures vapor entities are fully initialized before physics processes them
    if (isTerrain && !registry.all_of<Position>(entity)) {
        std::ostringstream error;
        error << "[handleMovement] Terrain entity " << static_cast<int>(entity)
              << " missing Position component (not fully initialized yet)";

        // delete from terrain repository mapping.
        Position pos = voxelGrid.terrainGridRepository->getPositionOfEntt(entity);
        int entityId = static_cast<int>(entity);
        if (pos.x == -1 && pos.y == -1 && pos.z == -1) {
            std::cout << "[handleMovement] Could not find position of entity " << entityId
                      << " in TerrainGridRepository, skipping entity." << std::endl;
            throw std::runtime_error(error.str());
        }
        registry.emplace<Position>(entity, pos);
        // throw std::runtime_error(error.str());
    }
}

/**
 * @brief Transforms an existing entity into "soft empty" terrain in the ECS.
 * @details This is done by updating its EntityTypeComponent and StructuralIntegrityComponent
 * to reflect the properties of an empty, gaseous tile.
 * @param registry The entt::registry.
 * @param terrain The entity to convert.
 */
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

/**
 * @brief Sets or overwrites components of an entity in the ECS to represent an empty water tile.
 * @param registry The entt::registry.
 * @param terrain The entity to modify.
 * @param matterState The matter state (e.g., LIQUID) to assign to the entity.
 */
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

// =========================================================================
// ================ 2. Entity Lifecycle Mutators = ================
// =========================================================================

/**
 * @brief Creates a new entity and initializes it as a vapor terrain block.
 * @details Creates an entity in the ECS and sets its corresponding properties (Position, Type, Matter, etc.)
 * directly in the TerrainGridRepository.
 * @param registry The entt::registry to create the entity in.
 * @param voxelGrid The VoxelGrid for accessing the terrain repository.
 * @param x The x-coordinate for the new entity.
 * @param y The y-coordinate for the new entity.
 * @param z The z-coordinate for the new entity.
 * @param vaporAmount The amount of vapor to initialize the entity with.
 * @return The newly created vapor entity.
 */
inline entt::entity createVaporTerrainEntity(entt::registry& registry, VoxelGrid& voxelGrid, int x,
                                             int y, int z, int vaporAmount) {
    auto newVaporEntity = registry.create();
    Position newPosition = {x, y, z, DirectionEnum::DOWN};
    registry.emplace<Position>(newVaporEntity, newPosition);

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

    VoxelCoord key{newPosition.x, newPosition.y, newPosition.z};
    voxelGrid.terrainGridRepository->addToTrackingMaps(key, newVaporEntity);

    return newVaporEntity;
}

/**
 * @brief Destroys an entity and cleans up its associated data from the TerrainGridRepository.
 * @param registry The entt::registry.
 * @param voxelGrid The VoxelGrid for accessing the terrain repository.
 * @param entity The entity to destroy.
 * @param e The exception that triggered the cleanup.
 */
inline void cleanupInvalidTerrainEntity(entt::registry& registry, VoxelGrid& voxelGrid,
                                        entt::entity entity,
                                        const aetherion::InvalidEntityException& e) {
    std::cout << "[cleanupInvalidTerrainEntity] InvalidEntityException: " << e.what()
              << " - entity ID=" << static_cast<int>(entity) << std::endl;

    Position pos;
    try {
        pos = voxelGrid.terrainGridRepository->getPositionOfEntt(entity);
    } catch (const aetherion::InvalidEntityException& e) {
        // Exception indicates we should stop processing this entity.
        // The mutator function already logged the details.
        Position *_pos = registry.try_get<Position>(entity);
        pos = _pos ? *_pos : Position{-1, -1, -1, DirectionEnum::UP};
        if (pos.x == -1 && pos.y == -1 && pos.z == -1) {
            std::cout << "[cleanupInvalidTerrainEntity] Could not find position of entity "
                      << static_cast<int>(entity)
                      << " in TerrainGridRepository or registry - just delete it." << std::endl;
            throw std::runtime_error("Could not find entity position for cleanup");
        }
    }

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
                pos.x, pos.y, pos.z,
                static_cast<int>(TerrainIdTypeEnum::ON_GRID_STORAGE));
        }
    }
}

/**
 * @brief Destroys an entity in the registry.
 * @param registry The entt::registry.
 * @param entity The entity to destroy.
 */
inline void _destroyEntity(entt::registry& registry, VoxelGrid& voxelGrid, entt::entity entity, bool shouldLock=true) {
    std::unique_ptr<TerrainGridLock> terrainLockGuard;
    if (shouldLock) {
        terrainLockGuard = std::make_unique<TerrainGridLock>(voxelGrid.terrainGridRepository.get());
    }

    registry.destroy(entity);
}

/**
 * @brief Ensures that a terrain entity is active in the ECS.
 * @param voxelGrid The VoxelGrid.
 * @param x The x-coordinate.
 * @param y The y-coordinate.
 * @param z The z-coordinate.
 * @return The active entity.
 */
inline entt::entity _ensureEntityActive(VoxelGrid& voxelGrid, int x, int y, int z) {
    TerrainGridLock lock(voxelGrid.terrainGridRepository.get());

    return voxelGrid.terrainGridRepository->ensureActive(x, y, z);
}

/**
 * @brief Dispatches an event to kill an entity or converts it to soft empty.
 * @details If the entity has no active tile effects, it enqueues a `KillEntityEvent`.
 * Otherwise, it converts the entity into a "soft empty" terrain block to allow effects to resolve.
 * @param registry The entt::registry.
 * @param dispatcher The entt::dispatcher to enqueue events.
 * @param terrain The entity to delete or convert.
 */
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


// =========================================================================
// ================ 3. VoxelGrid State Mutators = ================
// =========================================================================

/**
 * @brief Sets the components for a coordinate in the TerrainGridRepository to represent an empty water tile.
 * @param registry The entt::registry (used for context, not modified).
 * @param voxelGrid The VoxelGrid for accessing the terrain repository.
 * @param terrainId The ID of the terrain.
 * @param x The x-coordinate to modify.
 * @param y The y-coordinate to modify.
 * @param z The z-coordinate to modify.
 * @param matterState The matter state to assign.
 */
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

/**
 * @brief Modifies the StructuralIntegrityComponent of a tile in the VoxelGrid to have vapor properties.
 * @param x The x-coordinate of the tile.
 * @param y The y-coordinate of the tile.
 * @param z The z-coordinate of the tile.
 * @param voxelGrid The VoxelGrid for accessing the terrain repository.
 */
inline void setVaporSI(int x, int y, int z, VoxelGrid& voxelGrid) {
    StructuralIntegrityComponent terrainSI =
        voxelGrid.terrainGridRepository->getTerrainStructuralIntegrity(x, y, z);
    terrainSI.canStackEntities = false;
    terrainSI.maxLoadCapacity = -1;
    terrainSI.matterState = MatterState::GAS;
    voxelGrid.terrainGridRepository->setTerrainStructuralIntegrity(x, y, z, terrainSI);
}

// =========================================================================
// ================ 4. Compound & Orchestration Mutators = ================
// =========================================================================

/**
 * @brief Cleans up entities with zero velocity.
 * @details For non-terrain entities, it removes the Velocity component. For terrain entities,
 * it resets the velocity to zero directly in the TerrainGridRepository.
 * @param registry The entt::registry.
 * @param voxelGrid The VoxelGrid for accessing the terrain repository.
 * @param entity The entity to check.
 * @param position The position of the entity.
 * @param velocity The velocity of the entity.
 * @param isTerrain Flag indicating if the entity is a terrain entity.
 */
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

/**
 * @brief Orchestrates the conversion of a terrain block to water.
 * @details It calls different underlying mutators based on whether the terrain
 * data is stored in the ECS or directly in the VoxelGrid storage.
 * @param registry The entt::registry.
 * @param voxelGrid The VoxelGrid.
 * @param terrainId The ID of the terrain entity/tile.
 * @param x The x-coordinate of the tile.
 * @param y The y-coordinate of the tile.
 * @param z The z-coordinate of the tile.
 */
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

/**
 * @brief A wrapper that performs a read-only check before converting a tile to water.
 * @param registry The entt::registry.
 * @param voxelGrid The VoxelGrid.
 * @param terrainId The ID of the terrain entity/tile.
 * @param x The x-coordinate of the tile.
 * @param y The y-coordinate of the tile.
 * @param z The z-coordinate of the tile.
 */
inline void checkAndConvertSoftEmptyIntoWater(entt::registry& registry, VoxelGrid& voxelGrid,
                                              int terrainId, int x, int y, int z) {
    if (getTypeAndCheckSoftEmpty(registry, voxelGrid, terrainId, x, y, z)) {
        convertSoftEmptyIntoWater(registry, voxelGrid, terrainId, x, y, z);
    }
}

/**
 * @brief Converts a soft empty terrain tile into vapor. (Currently a placeholder).
 * @param registry The entt::registry.
 * @param voxelGrid The VoxelGrid.
 * @param terrainId The ID of the terrain entity/tile.
 * @param x The x-coordinate of the tile.
 * @param y The y-coordinate of the tile.
 * @param z The z-coordinate of the tile.
 */
static void convertSoftEmptyIntoVapor(entt::registry& registry, VoxelGrid& voxelGrid, int terrainId,
                                      int x, int y, int z) {
    std::cout << "[convertSoftEmptyIntoVapor] Just marking a checkpoint on logs." << std::endl;
    // TODO: This might be involved in the bug I am debugging.
    // setEmptyWaterComponents(registry, terrain, MatterState::GAS);
}

// Helper: A wrapper that performs a read-only check before converting a tile to vapor.
inline void checkAndConvertSoftEmptyIntoVapor(entt::registry& registry, VoxelGrid& voxelGrid,
                                              int terrainId, int x, int y, int z) {
    if (getTypeAndCheckSoftEmpty(registry, voxelGrid, terrainId, x, y, z)) {
        convertSoftEmptyIntoVapor(registry, voxelGrid, terrainId, x, y, z);
    }
}

/**
 * @brief Handles dropping items from a dying entity's inventory into the world.
 * @details (Placeholder) This function is intended to read an entity's DropRates component,
 * create new item entities, and place them in the inventory of the tile below the dying entity.
 * The logic is currently commented out in the original LifeEngine.
 * @param registry The entt::registry.
 * @param voxelGrid The VoxelGrid.
 * @param entity The entity dropping items.
 */
inline void dropEntityItems(entt::registry& registry, VoxelGrid& voxelGrid, entt::entity entity) {
    // NOTE: The logic for this function is based on the commented-out `dropItems`
    // function in `LifeEvents.cpp` and serves as a placeholder for the intended functionality.
    std::cout << "Checking for item drops from entity " << static_cast<int>(entity)
              << " (placeholder)." << std::endl;

    // if (registry.valid(entity) && registry.all_of<Position, DropRates>(entity)) {
    //     auto&& [pos, dropRates] = registry.get<Position, DropRates>(entity);

    //     auto terrainBellowId = voxelGrid->getTerrain(pos.x, pos.y, pos.z - 1);

    //     // TODO: TerrainRepository is not supporting Inventory yet. (Only Pure ECS entities)
    //     if (terrainBellowId != -1 && terrainBellowId != -2) {
    //         entt::entity terrainBellow = static_cast<entt::entity>(terrainBellowId);

    //         Inventory* inventory = registry.try_get<Inventory>(terrainBellow);
    //         bool shouldEmplaceInventory{inventory == nullptr};
    //         if (inventory == nullptr) {
    //             inventory = new Inventory();
    //         }

    //         if (!dropRates.itemDropRates.empty()) {
    //             for (const auto& [combinedItemId, valuesTuple] : dropRates.itemDropRates) {
    //                 auto [itemMainType, itemSubType0] = splitStringToInts(combinedItemId);

    //                 if (itemMainType == static_cast<int>(ItemEnum::FOOD)) {
    //                     std::shared_ptr<ItemConfiguration> itemConfiguration = 
    //                         getItemConfigurationOnManager(combinedItemId);
    //                     auto newFoodItem = itemConfiguration->createFoodItem(registry);

    //                     auto entityId = entt::to_integral(newFoodItem);
    //                     inventory->itemIDs.push_back(entityId);
    //                 }
    //             }

    //             if (shouldEmplaceInventory) {
    //                 registry.emplace<Inventory>(terrainBellow, *inventory);
    //             }
    //         }
    //     }
    // }
}

/**
 * @brief Removes an entity from its position in the VoxelGrid.
 * @details It checks the entity's type to call the appropriate grid deletion method
 * (e.g., `deleteTerrain` or `deleteEntity`). It includes safety checks to ensure the
 * correct entity is being removed from the grid.
 * @param registry The entt::registry.
 * @param voxelGrid The VoxelGrid.
 * @param dispatcher The entt::dispatcher, used for terrain deletion events.
 * @param entity The entity to remove from the grid.
 */
inline void removeEntityFromGrid(entt::registry& registry, VoxelGrid& voxelGrid,
                                 entt::dispatcher& dispatcher, entt::entity entity,
                                 bool takeLock = true) {
    int entityId = static_cast<int>(entity);
    bool isSpecialId = entityId == -1 || entityId == -2;
    if (!isSpecialId && registry.valid(entity) &&
        registry.all_of<Position, EntityTypeComponent>(entity)) {
        std::cout << "Removing entity from grid: " << entityId << std::endl;
        auto&& [pos, type] = registry.get<Position, EntityTypeComponent>(entity);

        int currentGridEntity = voxelGrid.getEntity(pos.x, pos.y, pos.z);
        if (currentGridEntity != entityId) {
            std::cout << "WARNING: Grid position (" << pos.x << "," << pos.y << "," << pos.z
                      << ") contains entity " << currentGridEntity
                      << " but trying to remove entity " << entityId << std::endl;
            return;
        }

        if (type.mainType == static_cast<int>(EntityEnum::TERRAIN)) {
            voxelGrid.deleteTerrain(dispatcher, pos.x, pos.y, pos.z, takeLock);
        } else if (type.mainType == static_cast<int>(EntityEnum::BEAST) ||
                   type.mainType == static_cast<int>(EntityEnum::PLANT)) {
            voxelGrid.deleteEntity(pos.x, pos.y, pos.z);
        }
    } else if (isSpecialId) {
        std::cout << "Entity " << entityId << " is a special ID, skipping grid removal."
                  << std::endl;
    } else if (!isSpecialId && registry.valid(entity)) {
        Position* position = registry.try_get<Position>(entity);
        EntityTypeComponent* entityType = registry.try_get<EntityTypeComponent>(entity);
        if (position) {
            std::cout << "Entity " << entityId << " has Position component at ("
                      << position->x << ", " << position->y << ", " << position->z << ")." << std::endl;
        } else {
            std::cout << "Entity " << entityId << " is missing Position component." << std::endl;
            Position _pos = voxelGrid.terrainGridRepository->getPositionOfEntt(entity);
            position = &_pos;
        }

        std::cout << "Entity " << entityId
                  << " is missing Position or EntityTypeComponent, checking TerrainGridRepository."
                  << std::endl;
        if (position->x == -1 && position->y == -1 && position->z == -1) {
            std::cout << "Could not find position of entity " << entityId
                      << " in TerrainGridRepository, skipping grid removal."
                      << std::endl;
            throw std::runtime_error(
                "Entity is missing Position component and not found in TerrainGridRepository.");
            } else {
            std::cout << "Removing entity " << entityId
                      << " from grid using position from TerrainGridRepository at ("
                      << position->x << ", " << position->y << ", " << position->z << ")."
                      << std::endl;
            voxelGrid.deleteTerrain(dispatcher, position->x, position->y, position->z, takeLock);
        }
    } else {
        std::cout << "Entity " << entityId << " is invalid, skipping grid removal."
                  << std::endl;
    }
}

/**
 * @brief Removes an entity from terrain storage (VoxelGrid / TerrainGridRepository)
 * @details This is a free function variant of the previous World::removeEntityFromTerrain.
 * The caller is responsible for holding any lifecycle locks (e.g., entityLifecycleMutex)
 * if required by the caller's locking contract. This function will acquire a
 * TerrainGridLock when modifying the repository if `removeFromGrid` is true.
 */
inline void removeEntityFromTerrain(entt::registry& registryRef, VoxelGrid& voxelGridRef,
                                    entt::dispatcher& dispatcherRef, entt::entity entity,
                                    bool removeFromGrid) {
    if (!registryRef.valid(entity)) {
        std::cout << "removeEntityFromTerrain: entity invalid, skipping: "
                  << static_cast<int>(entity) << std::endl;
        return;
    }

    const int entityId = static_cast<int>(entity);

    if (removeFromGrid) {
        std::cout << "removeEntityFromTerrain: removing entity from grid: " << entityId
                  << std::endl;
        // Acquire TerrainGridLock for the duration of grid modification
        if (voxelGridRef.terrainGridRepository) {
            TerrainGridLock terrainLock(voxelGridRef.terrainGridRepository.get());
            // removeEntityFromGrid handles voxel bookkeeping; do not destroy here
            removeEntityFromGrid(registryRef, voxelGridRef, dispatcherRef, entity, false);
        } else {
            // Fallback: still call removeEntityFromGrid even if repo pointer missing
            removeEntityFromGrid(registryRef, voxelGridRef, dispatcherRef, entity, false);
        }
    } else {
        std::cout << "removeEntityFromTerrain: skip grid removal for entity: " << entityId
                  << std::endl;
    }
}

/**
 * @brief Performs a "soft kill" on an entity, removing its life components and grid representation.
 * @details A soft kill removes essential life components like Health and Metabolism, effectively
 * making the entity "dead" without immediately destroying the entity handle. It also removes the
 * entity from the main VoxelGrid representation.
 * @param registry The entt::registry.
 * @param voxelGrid The VoxelGrid.
 * @param dispatcher The entt::dispatcher, used for terrain deletion events.
 * @param entity The entity to be soft-killed.
 */
inline void softKillEntity(entt::registry& registry, VoxelGrid& voxelGrid,
                           entt::dispatcher& dispatcher,
                           entt::entity entity) {
    int entityId = static_cast<int>(entity);
    std::cout << "Performing soft kill on entity: " << entityId << std::endl;

    // Safely remove MetabolismComponent if it exists
    if (registry.all_of<MetabolismComponent>(entity)) {
        registry.remove<MetabolismComponent>(entity);
        std::cout << "Removed MetabolismComponent from entity " << entityId << std::endl;
    }

    // Safely remove HealthComponent if it exists
    if (registry.all_of<HealthComponent>(entity)) {
        registry.remove<HealthComponent>(entity);
        std::cout << "Removed HealthComponent from entity " << entityId << std::endl;
    }

    removeEntityFromGrid(registry, voxelGrid, dispatcher, entity);
}

/**
 * @brief A complex handler for "dormant" or invalid terrain entities that still have a Velocity component.
 * @details It attempts to reactivate a valid terrain entity from the `TerrainGridRepository`.
 * If revival fails, it may destroy the entity or convert it to an EMPTY tile.
 * @param registry The entt::registry.
 * @param voxelGrid The VoxelGrid.
 * @param dispatcher The entt::dispatcher.
 * @param positionOfEntt The last known position of the entity.
 * @param invalidTerrain The entity handle, which may be invalid.
 * @return A valid, revived entity handle.
 * @throws aetherion::InvalidEntityException if the entity cannot be revived.
 * @throws std::runtime_error for other fatal errors.
 */
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

/**
 * @brief Orchestrates the handling of an invalid entity detected during physics movement.
 * @details It attempts to revive the entity by calling `reviveColdTerrainEntities`. If that fails,
 * or if the entity's position is not found, it destroys the entity.
 * @param registry The entt::registry.
 * @param voxelGrid The VoxelGrid.
 * @param dispatcher The entt::dispatcher.
 * @param entity The entity handle, which may be invalid.
 * @return A valid, potentially new, entity handle if revival was successful.
 * @throws aetherion::InvalidEntityException if the entity cannot be handled and must be skipped.
 */
inline entt::entity handleInvalidEntityForMovement(entt::registry& registry, VoxelGrid& voxelGrid,
                                                   entt::dispatcher& dispatcher,
                                                   entt::entity entity) {
    // Entity is invalid but still in Velocity component storage
    // This happens during the timing window between registry.destroy() and hook execution
    // The onDestroyVelocity hook will clean up tracking maps - just skip for now
    std::cout << "[handleMovement] WARNING: Invalid entity in velocityView - skipping; entity ID="
              << static_cast<int>(entity) << " (cleanup will be handled by hooks)" << std::endl;

    Position pos = voxelGrid.terrainGridRepository->getPositionOfEntt(entity);
    int entityId = static_cast<int>(entity);
    if (pos.x == -1 && pos.y == -1 && pos.z == -1) {
        std::cout << "[handleMovement] Could not find position of entity " << entityId
                  << " in TerrainGridRepository - just delete it." << std::endl;
        registry.destroy(entity);
        // Throw exception to signal to caller that processing for this entity should stop.
        throw aetherion::InvalidEntityException(
            "Entity destroyed as it could not be found in TerrainGridRepository");

    } else {
        try {
            return reviveColdTerrainEntities(registry, voxelGrid, dispatcher, pos, entity);
        } catch (const aetherion::InvalidEntityException& e) {
            // Entity cannot be revived (e.g., zero vapor matter converted to empty)
            std::cout << "[handleMovement] Revival failed: " << e.what() << " - entity ID=" << entityId << std::endl;
            throw;  // Re-throw to be caught by handleMovement
        }
    }
}

/**
 * @brief Creates a new water entity from a "water fall" event.
 * @details This is a highly compound function that:
 * 1. Creates a new entity in the ECS.
 * 2. Emplaces all required components (`Position`, `Velocity`, `MatterContainer`, etc.).
 * 3. Updates the `VoxelGrid` to reference the new entity.
 * 4. Modifies the `MatterContainer` of the source entity that produced the water.
 * 5. Potentially destroys the source entity if its matter is depleted.
 * @note This function performs its own manual grid locking.
 * @param registry The entt::registry.
 * @param voxelGrid The VoxelGrid.
 * @param x The x-coordinate for the new water tile.
 * @param y The y-coordinate for the new water tile.
 * @param z The z-coordinate for the new water tile.
 * @param fallingAmount The amount of water matter for the new tile.
 * @param sourceEntity The entity from which the water is falling.
 */
inline void createWaterTerrainFromFall(entt::registry& registry, VoxelGrid& voxelGrid, int x, int y,
                                       int z, double fallingAmount, entt::entity sourceEntity) {
    // Lock for atomic state change
    TerrainGridLock lock(voxelGrid.terrainGridRepository.get());

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
}

/**
 * @brief Adds vapor to an existing tile above a source or creates a new vapor entity if no tile exists.
 * @param registry The entt::registry.
 * @param voxelGrid The VoxelGrid.
 * @param x The x-coordinate of the source tile.
 * @param y The y-coordinate of the source tile.
 * @param z The z-coordinate of the source tile.
 * @param amount The amount of vapor to add.
 */
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

/**
 * @brief Creates a new water tile below a vapor tile during condensation.
 * @details This is a compound function that creates a new water entity with all its components,
 * updates the `VoxelGrid`, modifies the source vapor tile's `MatterContainer`, and may destroy
 * the vapor entity if it's depleted.
 * @param registry The entt::registry.
 * @param voxelGrid The VoxelGrid.
 * @param vaporX The x-coordinate of the source vapor tile.
 * @param vaporY The y-coordinate of the source vapor tile.
 * @param vaporZ The z-coordinate of the source vapor tile.
 * @param condensationAmount The amount of water to create.
 * @param vaporMatter A reference to the `MatterContainer` of the source vapor tile.
 */
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

#endif  // PHYSICS_MUTATORS_HPP

// =========================================================================
// ================ 5. Event-based Mutators = ================
// =========================================================================

inline void _handleInvalidTerrainFound(entt::dispatcher& dispatcher, VoxelGrid& voxelGrid, const InvalidTerrainFoundEvent& event) {
    voxelGrid.deleteTerrain(dispatcher, event.x, event.y, event.z);
}

inline void _handleWaterSpreadEvent(VoxelGrid& voxelGrid, const WaterSpreadEvent& event) {
    TerrainGridLock lock(voxelGrid.terrainGridRepository.get());

    // Transfer water from source to target
    MatterContainer sourceMatter = event.sourceMatter;
    MatterContainer targetMatter = event.targetMatter;

    targetMatter.WaterMatter += event.amount;
    sourceMatter.WaterMatter -= event.amount;

    // Update both voxels atomically
    voxelGrid.terrainGridRepository->setTerrainMatterContainer(event.target.x, event.target.y,
                                                                event.target.z, targetMatter);
    voxelGrid.terrainGridRepository->setTerrainMatterContainer(event.source.x, event.source.y,
                                                                event.source.z, sourceMatter);
}

inline void _handleWaterGravityFlowEvent(VoxelGrid& voxelGrid, const WaterGravityFlowEvent& event) {
    TerrainGridLock lock(voxelGrid.terrainGridRepository.get());

    // TODO: Validate if the exchange is still possible. If not, skip.`

    // Transfer water downward
    MatterContainer sourceMatter = event.sourceMatter;
    MatterContainer targetMatter = event.targetMatter;

    targetMatter.WaterMatter += event.amount;
    sourceMatter.WaterMatter -= event.amount;

    // Update both voxels atomically
    voxelGrid.terrainGridRepository->setTerrainMatterContainer(event.target.x, event.target.y,
                                                                event.target.z, targetMatter);
    voxelGrid.terrainGridRepository->setTerrainMatterContainer(event.source.x, event.source.y,
                                                                event.source.z, sourceMatter);
}

inline void _handleTerrainPhaseConversionEvent(VoxelGrid& voxelGrid, const TerrainPhaseConversionEvent& event) {
    TerrainGridLock lock(voxelGrid.terrainGridRepository.get());

    // Apply terrain phase conversion (e.g., soft-empty -> water/vapor)
    voxelGrid.terrainGridRepository->setTerrainEntityType(event.position.x, event.position.y,
                                                           event.position.z, event.newType);
    voxelGrid.terrainGridRepository->setTerrainMatterContainer(event.position.x, event.position.y,
                                                                event.position.z, event.newMatter);
    voxelGrid.terrainGridRepository->setTerrainStructuralIntegrity(
        event.position.x, event.position.y, event.position.z, event.newStructuralIntegrity);
}

inline void _handleCreateVaporEntityEvent(entt::registry& registry, entt::dispatcher& dispatcher, VoxelGrid& voxelGrid, const CreateVaporEntityEvent& event) {
    // Atomic operation: Create entity and update terrain grid
    TerrainGridLock lock(voxelGrid.terrainGridRepository.get());

    // Create new entity for the vapor
    entt::entity newEntity = registry.create();
    int terrainId = static_cast<int>(newEntity);

    // Set terrain ID atomically
    voxelGrid.terrainGridRepository->setTerrainId(event.position.x, event.position.y,
                                                   event.position.z, terrainId);

    // Dispatch the move event with the newly created entity while holding lock
    // This prevents race condition where entity could be accessed before movement is queued
    MoveGasEntityEvent moveEvent{
        newEntity,
        Position{event.position.x, event.position.y, event.position.z, DirectionEnum::DOWN},
        0.0f,
        0.0f,
        event.rhoEnv,
        event.rhoVapor};
    moveEvent.setForceApplyNewVelocity();
    dispatcher.enqueue<MoveGasEntityEvent>(moveEvent);
}


inline void _handleVaporMergeSidewaysEvent(entt::registry& registry, entt::dispatcher& dispatcher, VoxelGrid& voxelGrid, const VaporMergeSidewaysEvent& event) {
    // Lock terrain grid for atomic state change (prevents race conditions with other systems)
    TerrainGridLock lock(voxelGrid.terrainGridRepository.get());

    // Get target vapor and merge
    MatterContainer targetMatter = voxelGrid.terrainGridRepository->getTerrainMatterContainer(
        event.target.x, event.target.y, event.target.z);
    targetMatter.WaterVapor += event.amount;
    voxelGrid.terrainGridRepository->setTerrainMatterContainer(event.target.x, event.target.y,
                                                                event.target.z, targetMatter);

    // Clear source vapor
    MatterContainer sourceMatter = voxelGrid.terrainGridRepository->getTerrainMatterContainer(
        event.source.x, event.source.y, event.source.z);
    sourceMatter.WaterVapor = 0;
    voxelGrid.terrainGridRepository->setTerrainMatterContainer(event.source.x, event.source.y,
                                                                event.source.z, sourceMatter);

    // Delete or convert source entity if it's a valid entity (not ON_GRID_STORAGE or NONE)
    if (event.sourceTerrainId != static_cast<int>(TerrainIdTypeEnum::ON_GRID_STORAGE) &&
        event.sourceTerrainId != static_cast<int>(TerrainIdTypeEnum::NONE)) {
        entt::entity sourceEntity = static_cast<entt::entity>(event.sourceTerrainId);
        if (registry.valid(sourceEntity)) {
            std::ostringstream ossMessage;
            ossMessage << "[VaporMergeSidewaysEvent] Deleting source vapor entity ID="
                      << event.sourceTerrainId << " at (" << event.source.x << ", "
                      << event.source.y << ", " << event.source.z << ")";
            spdlog::get("console")->debug(ossMessage.str());
            dispatcher.enqueue<KillEntityEvent>(sourceEntity);
        }
    }
}


// Helper: Update position to destination
inline void updatePositionToDestination(Position& position,
                                        const MovingComponent& movingComponent) {
    position.x = movingComponent.movingToX;
    position.y = movingComponent.movingToY;
    position.z = movingComponent.movingToZ;
}


// Helper: Apply terrain movement in VoxelGrid
inline void applyTerrainMovement(VoxelGrid& voxelGrid, entt::entity entity,
                                 const MovingComponent& movingComponent) {
    // std::cout << "Setting movingTo positions for terrain moving."
    //           << "moving to: " << movingComponent.movingToX << ", " << movingComponent.movingToY
    //           << ", " << movingComponent.movingToZ << "\n";

    validateTerrainEntityId(entity);
    voxelGrid.terrainGridRepository->moveTerrain(const_cast<MovingComponent&>(movingComponent));
}

// Helper: Apply regular entity movement in VoxelGrid
inline void applyEntityMovement(VoxelGrid& voxelGrid, entt::entity entity,
                                const MovingComponent& movingComponent) {
    Position movingToPosition;
    movingToPosition.x = movingComponent.movingToX;
    movingToPosition.y = movingComponent.movingToY;
    movingToPosition.z = movingComponent.movingToZ;
    voxelGrid.moveEntity(entity, movingToPosition);
}

// Main function: Create and apply movement component
inline void createMovingComponent(entt::registry& registry, entt::dispatcher& dispatcher,
                           VoxelGrid& voxelGrid, entt::entity entity, Position& position,
                           Velocity& velocity, int movingToX, int movingToY, int movingToZ,
                           float completionTime, bool willStopX, bool willStopY, bool willStopZ,
                           bool isTerrain) {
    MovingComponent movingComponent =
        initializeMovingComponent(position, velocity, movingToX, movingToY, movingToZ,
                                  completionTime, willStopX, willStopY, willStopZ);

    registry.emplace<MovingComponent>(entity, movingComponent);

    EntityTypeComponent entityType =
        getEntityType(registry, voxelGrid, entity, position, isTerrain);

    bool isTerrainType =
        (entityType.mainType == static_cast<int>(EntityEnum::TERRAIN)) || isTerrain;

    if (isTerrainType) {
        applyTerrainMovement(voxelGrid, entity, movingComponent);
    } else {
        applyEntityMovement(voxelGrid, entity, movingComponent);
    }

    updatePositionToDestination(position, movingComponent);
}
