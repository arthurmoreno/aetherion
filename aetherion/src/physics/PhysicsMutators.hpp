#ifndef PHYSICS_MUTATORS_H
#define PHYSICS_MUTATORS_H

#include <entt/entt.hpp>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <memory>

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

#include "physics/ComponentMutators.hpp"
/**
 * @file PhysicsMutators.hpp
 * @brief Centralized access point for physics state mutators.
 *
 * This header centralizes and documents the set of functions that mutate
 * physics-related state in the engine. Its primary purpose is to provide a
 * single, well-documented surface for callers to locate and use mutators that
 * affect either ECS component storage, terrain repository storage, or both.
 *
 * The module also classifies each mutator by three concerns so callers can
 * reason about safety and side-effects: where state is stored, what
 * synchronization or atomicity guarantees (if any) exist, and the expected
 * scope of side-effects. That classification reduces accidental misuse and
 * centralizes locking guidance for terrain/ECS interactions.
 *
 * Purpose
 * - Centralize: Expose physics mutators from a single, discoverable header.
 * - Classify: Make storage target, locking model, and scope explicit.
 * - Guide: Help callers pick the correct mutator and adhere to locking
 *   contracts so repository and ECS state remain consistent.
 *
 * Classification Dimensions:
 * 1. Storage Target
 * - ECS-only: Mutates only `entt` component storage (Position, Velocity, MatterContainer...).
 *   Example: `updateEntityVelocity`, `convertIntoSoftEmpty`, `setEmptyWaterComponentsEnTT`.
 * - Repository-only: Mutates only `TerrainGridRepository`/`VoxelGrid` storage (tile id, matter container, SI).
 *   Example: `setEmptyWaterComponentsStorage`, `setVaporSI`.
 * - Hybrid: Touches both ECS and repository to keep them consistent.
 *   Example: `createVaporTerrainEntity`, `createWaterTerrainFromFall`.
 *
 * 2. Synchronization & Atomicity
 * - Lock-Free / Caller-Synchronized: No locks taken; caller must ensure safety.
 *   Example: `updateEntityVelocity`, `setEmptyWaterComponentsEnTT`.
 * - Conditional Locking: Take a lock only when needed / via `takeLock` flag.
 *   Example: `addOrCreateVaporAbove`.
 * - Internal Atomic (Self-locking): Acquire `TerrainGridLock` internally for atomic repo writes.
 *   Example: `createWaterTerrainFromFall`, `convertTerrainTileToEmpty`.
 *
 * 3. Scope & Side-Effects
 * - Single-Entity-Local: Changes limited to one entity's components.
 *   Example: `ensurePositionComponentForTerrain`, `convertIntoSoftEmpty`.
 * - Multi-Entity / Multi-Tile: Updates multiple entities/tiles or repository maps.
 *   Example: `reviveColdTerrainEntities`, `addOrCreateVaporAbove`.
 * - Orchestration / Global Effects: Triggers events or lifecycle transitions.
 *   Example: `destroyEntityWithGridCleanup`, `handleInvalidEntityForMovement`.
 *
 * Placement guidance: add a short tag comment on each function: `[Storage:ECS|Repo|Hybrid] [Lock:None|Cond|Internal] [Scope:Entity|Multi|Orch]`
 */

// Forward declarations for functions used across categories
// Note: direct component mutators are declared in ComponentMutators.hpp
static void setEmptyWaterComponentsStorage(entt::registry& registry, VoxelGrid& voxelGrid,
                                           int terrainId, int x, int y, int z,
                                           MatterState matterState);
inline entt::entity createVaporTerrainEntity(entt::registry& registry, VoxelGrid& voxelGrid, int x,
                                             int y, int z, int vaporAmount);

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
// [Storage:Hybrid] [Lock:None] [Scope:Entity]
inline entt::entity createVaporTerrainEntity(entt::registry& registry, VoxelGrid& voxelGrid, int x,
                                             int y, int z, int vaporAmount) {
    if (!voxelGrid.terrainGridRepository) {
        spdlog::warn("createVaporTerrainEntity: missing terrainGridRepository");
        return entt::null;
    }
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
// [Storage:Hybrid] [Lock:Cond] [Scope:Entity]
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
                      << " in TerrainGridRepository or registry - soft-deactivating it." << std::endl;
            // Soft-deactivate instead of immediate destroy to avoid TOCTOU races
            voxelGrid.terrainGridRepository->softDeactivateEntity(entity);
            throw std::runtime_error("Could not find entity position for cleanup");
        }
    }

    int entityId = static_cast<int>(entity);

    if (pos.x == -1 && pos.y == -1 && pos.z == -1) {
        std::cout << "[cleanupInvalidTerrainEntity] Could not find position of entity " << entityId
                  << " in TerrainGridRepository - soft-deactivating it." << std::endl;
        voxelGrid.terrainGridRepository->softDeactivateEntity(entity);
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
            // Ensure repository mapping cleaned up and transient components removed
            voxelGrid.terrainGridRepository->softDeactivateEntity(entity);
        } else {
            std::cout
                << "[cleanupInvalidTerrainEntity] Terrain does exist at the given position in "
                   "repository or grid ???"
                << entityId << " at position: " << pos.x << ", " << pos.y << ", " << pos.z
                << std::endl;
            voxelGrid.terrainGridRepository->softDeactivateEntity(entity);
            voxelGrid.terrainGridRepository->setTerrainId(
                pos.x, pos.y, pos.z,
                static_cast<int>(TerrainIdTypeEnum::ON_GRID_STORAGE));
        }
    }
}


/**
 * @brief Wrapper around `TerrainGridRepository::softDeactivateEntity` to centralize
 * state changes in this module.
 * @param voxelGrid The VoxelGrid containing the `terrainGridRepository`.
 * @param entity The entity to soft-deactivate.
 * @param takeLock Whether the repository should take its internal lock.
 */
// [Storage:Repo] [Lock:Cond] [Scope:Entity]
inline void softDeactivateTerrainEntity(VoxelGrid& voxelGrid, entt::entity entity, bool takeLock) {
    if (!voxelGrid.terrainGridRepository) return;

    voxelGrid.terrainGridRepository->softDeactivateEntity(entity, takeLock);
}

/**
 * @brief Destroys an entity in the registry.
 * @param registry The entt::registry.
 * @param entity The entity to destroy.
 */
// [Storage:Hybrid] [Lock:Cond] [Scope:Entity]
inline void _destroyEntity(entt::registry& registry, VoxelGrid& voxelGrid, entt::entity entity, bool shouldLock=true) {
    std::unique_ptr<TerrainGridLock> terrainLockGuard;
    if (shouldLock) {
        terrainLockGuard = std::make_unique<TerrainGridLock>(voxelGrid.terrainGridRepository.get());
    }

    // Ensure repository mapping cleaned before destroying entity to avoid stale mappings
    // Use centralized wrapper to keep state-changes in this module.
    softDeactivateTerrainEntity(voxelGrid, entity, !shouldLock);

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
// [Storage:Repo] [Lock:Internal] [Scope:Entity]
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
// [Storage:Hybrid] [Lock:None] [Scope:Entity]
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
// Part 1: Set EntityTypeComponent
static void setEmptyWaterComponentsStorage(entt::registry& registry, VoxelGrid& voxelGrid,
                                           int terrainId, int x, int y, int z,
                                           MatterState matterState) {
    // [Storage:Repo] [Lock:None] [Scope:Entity]
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
 * @brief Convert a repository-backed terrain tile into EMPTY and clear its storage state.
 * @details Acquires a `TerrainGridLock` for the duration of the operation to ensure
 * atomic updates to TerrainGridRepository fields (id, type, matter container, SI).
 * @param registry The entt::registry (kept for symmetry / future use).
 * @param voxelGrid The VoxelGrid containing the repository to modify.
 * @param pos The position of the tile to convert.
 * @param invalidTerrain The entity handle that is being converted (may be invalid).
 */
// [Storage:Repo] [Lock:Internal] [Scope:Entity]
static void convertTerrainTileToEmpty(entt::registry& registry, VoxelGrid& voxelGrid,
                                         const Position& pos, entt::entity invalidTerrain) {
    if (!voxelGrid.terrainGridRepository) return;

    // RAII lock for repository modifications
    TerrainGridLock lock(voxelGrid.terrainGridRepository.get());

    // Soft-deactivate mapping/components for the entity while we mutate storage
    voxelGrid.terrainGridRepository->softDeactivateEntity(invalidTerrain, false);

    // Mark the tile as NONE / EMPTY in repository
    voxelGrid.terrainGridRepository->setTerrainId(pos.x, pos.y, pos.z,
                                                 static_cast<int>(TerrainIdTypeEnum::NONE));
    voxelGrid.terrainGridRepository->setTerrainEntityType(
        pos.x, pos.y, pos.z,
        EntityTypeComponent{static_cast<int>(EntityEnum::TERRAIN),
                            static_cast<int>(TerrainEnum::EMPTY), 0});

    // Clear the repository-backed matter container for this tile
    MatterContainer zeroedMatter = {};
    zeroedMatter.TerrainMatter = 0;
    zeroedMatter.WaterVapor = 0;
    zeroedMatter.WaterMatter = 0;
    zeroedMatter.BioMassMatter = 0;
    voxelGrid.terrainGridRepository->setTerrainMatterContainer(pos.x, pos.y, pos.z, zeroedMatter);

    // Reset Structural Integrity (SI) to the EMPTY defaults
    StructuralIntegrityComponent emptySI = {};
    emptySI.canStackEntities = false;
    emptySI.maxLoadCapacity = -1;
    emptySI.matterState = MatterState::GAS;
    emptySI.gradientVector = GradientVector{};
    voxelGrid.terrainGridRepository->setTerrainStructuralIntegrity(pos.x, pos.y, pos.z, emptySI);
}

/**
 * @brief Modifies the StructuralIntegrityComponent of a tile in the VoxelGrid to have vapor properties.
 * @param x The x-coordinate of the tile.
 * @param y The y-coordinate of the tile.
 * @param z The z-coordinate of the tile.
 * @param voxelGrid The VoxelGrid for accessing the terrain repository.
 */
// [Storage:Repo] [Lock:None] [Scope:Entity]
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
    // std::cout << "Checking for item drops from entity " << static_cast<int>(entity)
    //           << " (placeholder)." << std::endl;

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
        std::ostringstream ossMessage;
        ossMessage << "[processPhysics:Velocity] Removing entity from grid: " << entityId;
        spdlog::get("console")->debug(ossMessage.str());
        auto&& [pos, type] = registry.get<Position, EntityTypeComponent>(entity);

        int currentGridEntity = voxelGrid.getEntity(pos.x, pos.y, pos.z);
        if (currentGridEntity != entityId) {
            std::ostringstream ossMessage2;
            ossMessage2 << "[processPhysics:Velocity] WARNING: Grid position (" << pos.x << "," << pos.y << "," << pos.z
                        << ") contains entity " << currentGridEntity
                        << " but trying to remove entity " << entityId;
            spdlog::get("console")->debug(ossMessage2.str());
            return;
        }

        if (type.mainType == static_cast<int>(EntityEnum::TERRAIN)) {
            voxelGrid.deleteTerrain(dispatcher, pos.x, pos.y, pos.z, takeLock);
        } else if (type.mainType == static_cast<int>(EntityEnum::BEAST) ||
                   type.mainType == static_cast<int>(EntityEnum::PLANT)) {
            voxelGrid.deleteEntity(pos.x, pos.y, pos.z);
        }
    } else if (isSpecialId) {
        std::ostringstream ossMessage;
        ossMessage << "[processPhysics:Velocity] Entity " << entityId << " is a special ID, skipping grid removal.";
        spdlog::get("console")->debug(ossMessage.str());
    } else if (!isSpecialId && registry.valid(entity)) {
        Position* position = registry.try_get<Position>(entity);
        EntityTypeComponent* entityType = registry.try_get<EntityTypeComponent>(entity);
        if (position) {
            std::ostringstream ossMessage;
            ossMessage << "[processPhysics:Velocity] Entity " << entityId << " has Position component at ("
                       << position->x << ", " << position->y << ", " << position->z << ").";
            spdlog::get("console")->debug(ossMessage.str());
        } else {
            std::ostringstream ossMessage;
            ossMessage << "[processPhysics:Velocity] Entity " << entityId << " is missing Position component.";
            spdlog::get("console")->debug(ossMessage.str());
            Position _pos = voxelGrid.terrainGridRepository->getPositionOfEntt(entity);
            position = &_pos;
        }

        std::ostringstream ossMessage3;
        ossMessage3 << "[processPhysics:Velocity] Entity " << entityId
                     << " is missing Position or EntityTypeComponent, checking TerrainGridRepository.";
        spdlog::get("console")->debug(ossMessage3.str());
        if (position->x == -1 && position->y == -1 && position->z == -1) {
            std::ostringstream ossMessage4;
            ossMessage4 << "[processPhysics:Velocity] Could not find position of entity " << entityId
                        << " in TerrainGridRepository, skipping grid removal.";
            spdlog::get("console")->debug(ossMessage4.str());
            throw std::runtime_error(
                "Entity is missing Position component and not found in TerrainGridRepository.");
            } else {
            std::ostringstream ossMessage5;
            ossMessage5 << "[processPhysics:Velocity] Removing entity " << entityId
                        << " from grid using position from TerrainGridRepository at ("
                        << position->x << ", " << position->y << ", " << position->z << ").";
            spdlog::get("console")->debug(ossMessage5.str());
            voxelGrid.deleteTerrain(dispatcher, position->x, position->y, position->z, takeLock);
        }
    } else {
        std::ostringstream ossMessage;
        ossMessage << "[processPhysics:Velocity] Entity " << entityId << " is invalid, skipping grid removal.";
        spdlog::get("console")->debug(ossMessage.str());
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
 * @brief Destroys an entity and performs grid/repository cleanup.
 * @param registry The entt::registry.
 * @param voxelGrid The VoxelGrid for repository access.
 * @param dispatcher The dispatcher used by removeEntityFromGrid if necessary.
 * @param entity The entity to destroy.
 * @param takeGridLock If true, callers will attempt to take repository locks when modifying the grid.
 *
 * Locking contract: this function DOES NOT acquire World::entityLifecycleMutex. The caller must
 * hold any lifecycle locks required to prevent races with perception/creation. This function will
 * acquire a TerrainGridLock when performing grid modifications if `takeGridLock` is true.
 */
inline void destroyEntityWithGridCleanup(entt::registry& registry, VoxelGrid& voxelGrid,
                                         entt::dispatcher& dispatcher, entt::entity entity,
                                         bool takeGridLock = true) {
    if (!registry.valid(entity)) {
        std::cout << "destroyEntityWithGridCleanup: entity invalid, skipping: "
                  << static_cast<int>(entity) << std::endl;
        return;
    }

    const int entityId = static_cast<int>(entity);

    // Special terrain markers (-1, -2) should not be destroyed through ECS
    if (entityId == -1 || entityId == -2) {
        std::cout << "destroyEntityWithGridCleanup: skipping special ID " << entityId << std::endl;
        return;
    }

    try {
        // Remove references from VoxelGrid / TerrainGridRepository first
        removeEntityFromGrid(registry, voxelGrid, dispatcher, entity, takeGridLock);
    } catch (const std::exception& e) {
        std::cout << "destroyEntityWithGridCleanup: removeEntityFromGrid failed for entity "
                  << entityId << ": " << e.what() << std::endl;
    }

    // Ensure the entity is destroyed in the registry. Do not attempt to re-lock the repository here
    // because removeEntityFromGrid already handled grid locking when requested.
    if (registry.valid(entity)) {
        _destroyEntity(registry, voxelGrid, entity, false);
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
    int waterMatter = voxelGrid.terrainGridRepository->getWaterMatter(
        positionOfEntt.x, positionOfEntt.y, positionOfEntt.z);

    if (terrainType.mainType == static_cast<int>(EntityEnum::TERRAIN) &&
        terrainType.subType0 == static_cast<int>(TerrainEnum::WATER) &&
        vaporMatter > 0 && waterMatter == 0) {
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
            // Convert repository-backed tile into EMPTY and clear storage (under repo lock)
            convertTerrainTileToEmpty(registry, voxelGrid, positionOfEntt, invalidTerrain);
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

    Position pos = {-1, -1, -1, DirectionEnum::DOWN};
    try {
        pos = voxelGrid.terrainGridRepository->getPositionOfEntt(entity);
    } catch (const aetherion::InvalidEntityException& e) {
        convertTerrainTileToEmpty(registry, voxelGrid, pos, entity);
        // voxelGrid.terrainGridRepository->softDeactivateEntity(entity);
        throw;  // Re-throw to be caught by handleMovement
    }
    int entityId = static_cast<int>(entity);
    if (pos.x == -1 && pos.y == -1 && pos.z == -1) {
        std::cout << "[handleMovement] Could not find position of entity " << entityId
                  << " in TerrainGridRepository - soft-deactivating it." << std::endl;

        convertTerrainTileToEmpty(registry, voxelGrid, pos, entity);
        // voxelGrid.terrainGridRepository->softDeactivateEntity(entity);
        // Throw exception to signal to caller that processing for this entity should stop.
        throw aetherion::InvalidEntityException(
            "Entity soft-deactivated as it could not be found in TerrainGridRepository");

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
            // We're already holding TerrainGridLock for this operation, avoid double-locking
            voxelGrid.terrainGridRepository->softDeactivateEntity(sourceEntity, false);
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
    // Ensure repository lock for atomic check+write (avoid TOCTOU if caller didn't hold lock)
    std::unique_ptr<TerrainGridLock> lockGuard;
    if (voxelGrid.terrainGridRepository && !voxelGrid.terrainGridRepository->isTerrainGridLocked()) {
        lockGuard = std::make_unique<TerrainGridLock>(voxelGrid.terrainGridRepository.get());
    }

    int terrainAboveId = voxelGrid.getTerrain(x, y, z + 1);

    if (terrainAboveId != static_cast<int>(TerrainIdTypeEnum::NONE)) {
        EntityTypeComponent typeAbove =
            voxelGrid.terrainGridRepository->getTerrainEntityType(x, y, z + 1);
        MatterContainer matterContainerAbove =
            voxelGrid.terrainGridRepository->getTerrainMatterContainer(x, y, z + 1);

        // Check if it's vapor based on MatterContainer and not liquid water
        if (typeAbove.mainType == static_cast<int>(EntityEnum::TERRAIN) &&
            typeAbove.subType0 == static_cast<int>(TerrainEnum::WATER) &&
            matterContainerAbove.WaterVapor >= 0 && matterContainerAbove.WaterMatter == 0) {
            matterContainerAbove.WaterVapor += amount;
            std::ostringstream oss;
            oss << "[addOrCreateVaporAbove] Added vapor at (" << x << ", " << y << ", "
                << (z + 1) << ")."
                << " type=" << typeAbove.mainType << ", subtype=" << typeAbove.subType0
                << ", WaterMatter=" << matterContainerAbove.WaterMatter
                << ", WaterVapor=" << matterContainerAbove.WaterVapor;
            spdlog::get("console")->info(oss.str());
            voxelGrid.terrainGridRepository->setTerrainMatterContainer(x, y, z + 1,
                                                                       matterContainerAbove);
        } else {
            std::ostringstream oss;
            oss << "[addOrCreateVaporAbove] Cannot add vapor at (" << x << ", " << y << ", "
                << (z + 1) << ") - target not vapor-transitory or is liquid."
                << " type=" << typeAbove.mainType << ", subtype=" << typeAbove.subType0
                << ", WaterMatter=" << matterContainerAbove.WaterMatter
                << ", WaterVapor=" << matterContainerAbove.WaterVapor;
            spdlog::get("console")->warn(oss.str());
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
            // Caller did not hold the repository lock here; allow softDeactivateEntity to take the lock
            voxelGrid.terrainGridRepository->softDeactivateEntity(static_cast<entt::entity>(vaporTerrainId));
        }
    }
}


// =========================================================================
// ================ 5. Event-based Mutators = ================
// =========================================================================

inline void _handleInvalidTerrainFound(entt::dispatcher& dispatcher, VoxelGrid& voxelGrid, const InvalidTerrainFoundEvent& event) {
    voxelGrid.deleteTerrain(dispatcher, event.x, event.y, event.z);
}

inline void _handleWaterSpreadEvent(VoxelGrid& voxelGrid, const WaterSpreadEvent& event) {
    TerrainGridLock lock(voxelGrid.terrainGridRepository.get());

    // Re-read current repository state to avoid TOCTOU races
    MatterContainer currentSource = voxelGrid.terrainGridRepository->getTerrainMatterContainer(
        event.source.x, event.source.y, event.source.z);
    MatterContainer currentTarget = voxelGrid.terrainGridRepository->getTerrainMatterContainer(
        event.target.x, event.target.y, event.target.z);

    // Validate that the transfer is still possible: source has enough water
    // and target does not currently contain vapor (flow into vapor is invalid).
    if (currentSource.WaterMatter < event.amount) {
        spdlog::get("console")->warn(
            "[_handleWaterSpreadEvent] Source no longer has required amount of water.");
        return; // Source no longer has required amount
    }
    if (currentTarget.WaterVapor > 0) {
        spdlog::get("console")->warn(
            "[_handleWaterSpreadEvent] Target currently has vapor; aborting transfer.");
        return; // Target currently has vapor; abort transfer
    }

    // Apply transfer using up-to-date state
    currentTarget.WaterMatter += event.amount;
    currentSource.WaterMatter -= event.amount;

    // Update both voxels atomically
    voxelGrid.terrainGridRepository->setTerrainMatterContainer(event.target.x, event.target.y,
                                                                event.target.z, currentTarget);
    voxelGrid.terrainGridRepository->setTerrainMatterContainer(event.source.x, event.source.y,
                                                                event.source.z, currentSource);
}

inline void _handleWaterGravityFlowEvent(VoxelGrid& voxelGrid, const WaterGravityFlowEvent& event) {
    TerrainGridLock lock(voxelGrid.terrainGridRepository.get());

    // Re-read current repository state to avoid TOCTOU races
    MatterContainer currentSource = voxelGrid.terrainGridRepository->getTerrainMatterContainer(
        event.source.x, event.source.y, event.source.z);
    MatterContainer currentTarget = voxelGrid.terrainGridRepository->getTerrainMatterContainer(
        event.target.x, event.target.y, event.target.z);

    // Validate that the downward flow is still possible.
    if (currentSource.WaterMatter < event.amount) {
        spdlog::get("console")->warn(
            "[_handleWaterGravityFlowEvent] Source no longer has required amount of water.");
        return; // Source no longer has required amount
    }
    // Prevent flowing water into a tile that currently has vapor
    if (currentTarget.WaterVapor > 0) {
        spdlog::get("console")->warn(
            "[_handleWaterGravityFlowEvent] Target currently has vapor; aborting flow.");
        return; // Target has vapor; skip flow
    }

    // Apply transfer using up-to-date state
    currentTarget.WaterMatter += event.amount;
    currentSource.WaterMatter -= event.amount;

    // Update both voxels atomically
    voxelGrid.terrainGridRepository->setTerrainMatterContainer(event.target.x, event.target.y,
                                                                event.target.z, currentTarget);
    voxelGrid.terrainGridRepository->setTerrainMatterContainer(event.source.x, event.source.y,
                                                                event.source.z, currentSource);
}

inline void _handleTerrainPhaseConversionEvent(VoxelGrid& voxelGrid, const TerrainPhaseConversionEvent& event) {
    TerrainGridLock lock(voxelGrid.terrainGridRepository.get());

    // Re-read current repository state to avoid TOCTOU races and validate conversion
    EntityTypeComponent currentType = voxelGrid.terrainGridRepository->getTerrainEntityType(
        event.position.x, event.position.y, event.position.z);
    MatterContainer currentMatter = voxelGrid.terrainGridRepository->getTerrainMatterContainer(
        event.position.x, event.position.y, event.position.z);
    StructuralIntegrityComponent currentSI =
        voxelGrid.terrainGridRepository->getTerrainStructuralIntegrity(event.position.x, event.position.y,
                                                                      event.position.z);

    // Basic validation: avoid converting to water if target currently has vapor,
    // and avoid converting to vapor if target currently has liquid water.
    if (event.newMatter.WaterMatter > 0) {
        if (currentMatter.WaterVapor > 0) {
            return; // conflict: target currently contains vapor
        }
    }
    if (event.newMatter.WaterVapor > 0) {
        if (currentMatter.WaterMatter > 0) {
            return; // conflict: target currently contains liquid water
        }
    }

    // Optionally could validate structural integrity preconditions here
    (void)currentSI; // keep unused-variable warnings away if not used

    // Apply terrain phase conversion (safe under lock)
    voxelGrid.terrainGridRepository->setTerrainEntityType(event.position.x, event.position.y,
                                                           event.position.z, event.newType);
    voxelGrid.terrainGridRepository->setTerrainMatterContainer(event.position.x, event.position.y,
                                                                event.position.z, event.newMatter);
    voxelGrid.terrainGridRepository->setTerrainStructuralIntegrity(
        event.position.x, event.position.y, event.position.z, event.newStructuralIntegrity);
}

// Helper: Create an Entt entity and register its terrain id in the TerrainGridRepository.
// If `takeLock` is true (default) a TerrainGridLock is acquired for atomic update.
inline entt::entity createAndRegisterVaporEntity(entt::registry& registry, VoxelGrid& voxelGrid,
                                                int x, int y, int z, bool takeLock = true) {
    std::unique_ptr<TerrainGridLock> lockGuard;
    if (takeLock) {
        lockGuard = std::make_unique<TerrainGridLock>(voxelGrid.terrainGridRepository.get());
    }

    // Respect the "only vapor or only water" rule: if there is already liquid
    // water at this position, do not create/register a vapor entity.
    MatterContainer currentMatter = voxelGrid.terrainGridRepository->getTerrainMatterContainer(x, y, z);
    if (currentMatter.WaterMatter > 0) {
        spdlog::get("console")->warn(
            "[createAndRegisterVaporEntity] Cannot create vapor entity at ({}, {}, {}) - liquid water present.",
            x, y, z);
        return entt::null; // indicate we did not create a vapor entity
    }

    // Safe to create vapor entity
    entt::entity newEntity = registry.create();
    int terrainId = static_cast<int>(newEntity);
    voxelGrid.terrainGridRepository->setTerrainId(x, y, z, terrainId);
    return newEntity;
}

inline void _handleCreateVaporEntityEvent(entt::registry& registry, entt::dispatcher& dispatcher, VoxelGrid& voxelGrid, const CreateVaporEntityEvent& event) {
    // Create and register the vapor entity atomically (helper takes the repo lock)
    entt::entity newEntity = createAndRegisterVaporEntity(registry, voxelGrid, event.position.x,
                                                          event.position.y, event.position.z, true);

    // Dispatch the move event for the newly created entity (no need to hold repo lock here)
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

#endif  // PHYSICS_MUTATORS_HPP