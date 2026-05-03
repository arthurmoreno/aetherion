#ifndef PHYSICS_MUTATORS_H
#define PHYSICS_MUTATORS_H

#include <spdlog/spdlog.h>

#include <entt/entt.hpp>
#include <iostream>
#include <memory>
#include <sstream>
#include <stdexcept>

#include "components/EntityTypeComponent.hpp"
#include "components/MetabolismComponents.hpp"
#include "components/MovingComponent.hpp"
#include "components/PhysicsComponents.hpp"
#include "ecosystem/EcosystemEvents.hpp"
#include "items/ItemConfiguration.hpp"
#include "items/ItemConfigurationManager.hpp"
#include "physics/ComponentMutators.hpp"
#include "physics/PhysicalMath.hpp"
#include "physics/PhysicsEvents.hpp"
#include "physics/PhysicsExceptions.hpp"
#include "physics/PhysicsUtils.hpp"
#include "physics/PhysicsValidators.hpp"
#include "physics/ReadonlyQueries.hpp"
#include "terrain/TerrainGridLock.hpp"
#include "voxelgrid/VoxelGrid.hpp"
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
 * - ECS-only: Mutates only `entt` component storage (Position, Velocity,
 * MatterContainer...). Example: `updateEntityVelocity`, `convertIntoSoftEmpty`,
 * `setEmptyWaterComponentsEnTT`.
 * - Repository-only: Mutates only `TerrainGridRepository`/`VoxelGrid` storage
 * (tile id, matter container, SI). Example: `setEmptyWaterComponentsStorage`,
 * `setVaporSI`.
 * - Hybrid: Touches both ECS and repository to keep them consistent.
 *   Example: `createVaporTerrainEntity`, `createWaterTerrainFromFall`.
 *
 * 2. Synchronization & Atomicity
 * - Lock-Free / Caller-Synchronized: No locks taken; caller must ensure safety.
 *   Example: `updateEntityVelocity`, `setEmptyWaterComponentsEnTT`.
 * - Conditional Locking: Take a lock only when needed / via `takeLock` flag.
 *   Example: `addOrCreateVaporAbove`.
 * - Internal Atomic (Self-locking): Acquire `TerrainGridLock` internally for
 * atomic repo writes. Example: `createWaterTerrainFromFall`,
 * `convertTerrainTileToEmpty`.
 *
 * 3. Scope & Side-Effects
 * - Single-Entity-Local: Changes limited to one entity's components.
 *   Example: `ensurePositionComponentForTerrain`, `convertIntoSoftEmpty`.
 * - Multi-Entity / Multi-Tile: Updates multiple entities/tiles or repository
 * maps. Example: `reviveColdTerrainEntities`, `addOrCreateVaporAbove`.
 * - Orchestration / Global Effects: Triggers events or lifecycle transitions.
 *   Example: `destroyEntityWithGridCleanup`, `handleInvalidEntityForMovement`.
 *
 * Placement guidance: add a short tag comment on each function:
 * `[Storage:ECS|Repo|Hybrid] [Lock:None|Cond|Internal]
 * [Scope:Entity|Multi|Orch]`
 */

// Diagnostic helper for the persistent
// "Entity has both WaterMatter and WaterVapor" invariant violation.
// Call this immediately *before* `setTerrainMatterContainer` from inside any
// mutator. Parameter-only check (no storage reads, no throw, no unwinding —
// the storage-layer probe segfaulted under both forms; instrumentation lives
// here, where the calling thread already holds its mutator's lock and the
// log path is known-safe). When the outgoing container would land the cell
// in the both-fields-positive state, the helper logs an [INVARIANT-WRITE]
// line tagged with the mutator name; the per-tick check in
// `processTileWater` still trips the actual exception, but the log shows
// which mutator's outgoing matter caused it.
inline void _logIfViolatingMatterWrite(const char *mutatorName, int x, int y,
                                       int z, const MatterContainer &outgoing) {
  if (outgoing.WaterMatter > 0 && outgoing.WaterVapor > 0) {
    auto logger = spdlog::get("console");
    if (logger) {
      logger->error("[INVARIANT-WRITE][{}] coord=({}, {}, {}) "
                    "WaterMatter={} WaterVapor={} TerrainMatter={} "
                    "BioMassMatter={}",
                    mutatorName, x, y, z, outgoing.WaterMatter,
                    outgoing.WaterVapor, outgoing.TerrainMatter,
                    outgoing.BioMassMatter);
    }
  }
}

// Forward declarations for functions used across categories
// Note: direct component mutators are declared in ComponentMutators.hpp
static void setEmptyWaterComponentsStorage(entt::registry &registry,
                                           VoxelGrid &voxelGrid, int terrainId,
                                           int x, int y, int z,
                                           MatterState matterState);
inline void createVaporTerrainEntity(entt::registry &registry,
                                     VoxelGrid &voxelGrid, int x, int y, int z,
                                     int vaporAmount);

// =========================================================================
// ================ 2. Entity Lifecycle Mutators = ================
// =========================================================================

/**
 * @brief Materialise a vapor voxel directly into VDB storage.
 * @details Writes type / matter / physics scaffolding plus an
 * `ON_GRID_STORAGE` terrain id at the given coordinate. No EnTT entity is
 * created; the registry parameter is retained for source compatibility
 * with other mutators in this header but is not used.
 * @param registry Unused — kept to match the signature of sibling mutators.
 * @param voxelGrid The VoxelGrid for accessing the terrain repository.
 * @param x The x-coordinate for the new vapor cell.
 * @param y The y-coordinate for the new vapor cell.
 * @param z The z-coordinate for the new vapor cell.
 * @param vaporAmount The amount of vapor to initialise the cell with.
 */
// [Storage:VDB] [Lock:None] [Scope:Cell]
inline void createVaporTerrainEntity(entt::registry &registry,
                                     VoxelGrid &voxelGrid, int x, int y, int z,
                                     int vaporAmount) {
  (void)registry; // No EnTT entity is created — vapor lives in VDB only.
  if (!voxelGrid.terrainGridRepository) {
    spdlog::warn("createVaporTerrainEntity: missing terrainGridRepository");
    return;
  }
  Position newPosition = {x, y, z, DirectionEnum::DOWN};

  EntityTypeComponent newType = {};
  newType.mainType = static_cast<int>(EntityEnum::TERRAIN);
  newType.subType0 = static_cast<int>(TerrainEnum::WATER);
  newType.subType1 = 0;

  MatterContainer newMatterContainer = {};
  newMatterContainer.WaterVapor = vaporAmount;
  newMatterContainer.WaterMatter = 0;

  PhysicsStats newPhysicsStats = {};
  // mass is stored in an Int32Grid in `TerrainStorage`, so fractional values
  // truncate on `setPhysicsStats`. Pre-#41 mass=0.1 worked because PhysicsStats
  // was a float-field ECS component; post-migration `int(0.1) == 0` and the
  // gas handler then computes `accelerationX = forceX / 0` → NaN, polluting
  // the velocity grid and silently stalling vapor. Use the smallest integer
  // that survives truncation while keeping vapor lighter than water (mass=20).
  newPhysicsStats.mass = 1;
  newPhysicsStats.maxSpeed = 10;
  newPhysicsStats.minSpeed = 0.0;

  StructuralIntegrityComponent newStructuralIntegrityComponent = {};
  newStructuralIntegrityComponent.canStackEntities = false;
  newStructuralIntegrityComponent.maxLoadCapacity = -1;
  newStructuralIntegrityComponent.matterState = MatterState::GAS;

  voxelGrid.terrainGridRepository->setPosition(x, y, z, newPosition);
  voxelGrid.terrainGridRepository->setTerrainEntityType(x, y, z, newType);
  voxelGrid.terrainGridRepository->setTerrainMatterContainer(
      x, y, z, newMatterContainer);
  voxelGrid.terrainGridRepository->setTerrainStructuralIntegrity(
      x, y, z, newStructuralIntegrityComponent);
  voxelGrid.terrainGridRepository->setPhysicsStats(x, y, z, newPhysicsStats);
  voxelGrid.terrainGridRepository->setTerrainId(
      x, y, z, static_cast<int>(TerrainIdTypeEnum::ON_GRID_STORAGE));
}

/**
 * @brief Materialise an EMPTY-typed terrain cell directly into VDB storage.
 * @details Writes the EMPTY type tag and an `ON_GRID_STORAGE` terrain id
 * at the given coordinate. No EnTT entity is created; the registry
 * parameter is retained for source compatibility with sibling mutators
 * but is unused.
 * @param registry Unused — kept to match the signature of sibling mutators.
 * @param voxelGrid The VoxelGrid for accessing the terrain repository.
 * @param x The x-coordinate for the new cell.
 * @param y The y-coordinate for the new cell.
 * @param z The z-coordinate for the new cell.
 */
// [Storage:VDB] [Lock:None] [Scope:Cell]
inline void createEmptyActiveTerrain(entt::registry &registry,
                                     VoxelGrid &voxelGrid, int x, int y,
                                     int z) {
  (void)registry; // No EnTT entity is created — cell lives in VDB only.
  if (!voxelGrid.terrainGridRepository) {
    spdlog::warn("createEmptyActiveTerrain: missing terrainGridRepository");
    return;
  }
  Position newPosition = {x, y, z, DirectionEnum::DOWN};

  EntityTypeComponent newType = {};
  newType.mainType = static_cast<int>(EntityEnum::TERRAIN);
  newType.subType0 = static_cast<int>(TerrainEnum::EMPTY);
  newType.subType1 = 0;

  voxelGrid.terrainGridRepository->setPosition(x, y, z, newPosition);
  voxelGrid.terrainGridRepository->setTerrainEntityType(x, y, z, newType);
  voxelGrid.terrainGridRepository->setTerrainId(
      x, y, z, static_cast<int>(TerrainIdTypeEnum::ON_GRID_STORAGE));
}

/**
 * @brief Destroys an entity and cleans up its associated data from the
 * TerrainGridRepository.
 * @param registry The entt::registry.
 * @param voxelGrid The VoxelGrid for accessing the terrain repository.
 * @param entity The entity to destroy.
 * @param e The exception that triggered the cleanup.
 */
// [Storage:Hybrid] [Lock:Cond] [Scope:Entity]
inline void
cleanupInvalidTerrainEntity(entt::registry &registry,
                            entt::dispatcher &dispatcher, VoxelGrid &voxelGrid,
                            entt::entity entity,
                            const aetherion::InvalidEntityException &e) {
  std::cout << "[cleanupInvalidTerrainEntity] InvalidEntityException: "
            << e.what() << " - entity ID=" << static_cast<int>(entity)
            << std::endl;

  Position pos;
  try {
    pos = voxelGrid.terrainGridRepository->getPositionOfEntt(entity);
  } catch (const aetherion::InvalidEntityException &e) {
    // Exception indicates we should stop processing this entity.
    // The mutator function already logged the details.
    Position *_pos = registry.try_get<Position>(entity);
    pos = _pos ? *_pos : Position{-1, -1, -1, DirectionEnum::UP};
    if (pos.x == -1 && pos.y == -1 && pos.z == -1) {
      std::cout
          << "[cleanupInvalidTerrainEntity] Could not find position of entity "
          << static_cast<int>(entity)
          << " in TerrainGridRepository or registry - soft-deactivating it."
          << std::endl;
      // Soft-deactivate instead of immediate destroy to avoid TOCTOU races
      voxelGrid.terrainGridRepository->softDeactivateEntity(dispatcher, entity);
      throw std::runtime_error("Could not find entity position for cleanup");
    }
  }

  int entityId = static_cast<int>(entity);

  if (pos.x == -1 && pos.y == -1 && pos.z == -1) {
    std::cout
        << "[cleanupInvalidTerrainEntity] Could not find position of entity "
        << entityId << " in TerrainGridRepository - soft-deactivating it."
        << std::endl;
    voxelGrid.terrainGridRepository->softDeactivateEntity(dispatcher, entity);
  } else {
    std::optional<int> terrainIdOnGrid =
        voxelGrid.terrainGridRepository->getTerrainIdIfExists(pos.x, pos.y,
                                                              pos.z);
    if (terrainIdOnGrid.has_value()) {
      // Terrain exists on grid - remove from tracking maps and destroy entity
      std::cout << "[cleanupInvalidTerrainEntity] Terrain does exist at the "
                   "given position in "
                   "repository - checking terrainIdOnGrid: "
                << terrainIdOnGrid.value() << " for entity ID: " << entityId
                << " at position: " << pos.x << ", " << pos.y << ", " << pos.z
                << std::endl;
      // Ensure repository mapping cleaned up and transient components removed
      voxelGrid.terrainGridRepository->softDeactivateEntity(dispatcher, entity);
    } else {
      std::cout << "[cleanupInvalidTerrainEntity] Terrain does exist at the "
                   "given position in "
                   "repository or grid ???"
                << entityId << " at position: " << pos.x << ", " << pos.y
                << ", " << pos.z << std::endl;
      voxelGrid.terrainGridRepository->softDeactivateEntity(dispatcher, entity);
      voxelGrid.terrainGridRepository->setTerrainId(
          pos.x, pos.y, pos.z,
          static_cast<int>(TerrainIdTypeEnum::ON_GRID_STORAGE));
    }
  }
}

/**
 * @brief Wrapper around `TerrainGridRepository::softDeactivateEntity` to
 * centralize state changes in this module.
 * @param voxelGrid The VoxelGrid containing the `terrainGridRepository`.
 * @param entity The entity to soft-deactivate.
 * @param takeLock Whether the repository should take its internal lock.
 */
// [Storage:Repo] [Lock:Cond] [Scope:Entity]
inline void softDeactivateTerrainEntity(entt::dispatcher &dispatcher,
                                        VoxelGrid &voxelGrid,
                                        entt::entity entity, bool takeLock) {
  if (!voxelGrid.terrainGridRepository)
    return;

  voxelGrid.terrainGridRepository->softDeactivateEntity(dispatcher, entity,
                                                        takeLock);
}

/**
 * @brief Destroys an entity in the registry.
 * @param registry The entt::registry.
 * @param entity The entity to destroy.
 */
// [Storage:Hybrid] [Lock:Cond] [Scope:Entity]
inline void _destroyEntity(entt::registry &registry,
                           entt::dispatcher &dispatcher, VoxelGrid &voxelGrid,
                           entt::entity entity, bool shouldLock = true) {
  std::unique_ptr<TerrainGridLock> terrainLockGuard;
  if (shouldLock) {
    terrainLockGuard = std::make_unique<TerrainGridLock>(
        voxelGrid.terrainGridRepository.get());
  }

  // Ensure repository mapping cleaned before destroying entity to avoid stale
  // mappings Use centralized wrapper to keep state-changes in this module.
  softDeactivateTerrainEntity(dispatcher, voxelGrid, entity, !shouldLock);

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
inline entt::entity _ensureEntityActive(VoxelGrid &voxelGrid, int x, int y,
                                        int z) {
  TerrainGridLock lock(voxelGrid.terrainGridRepository.get());

  return voxelGrid.terrainGridRepository->ensureActive(x, y, z);
}

/**
 * @brief Dispatches an event to kill an entity or converts it to soft empty.
 * @details If the entity has no active tile effects, it enqueues a
 * `KillEntityEvent`. Otherwise, it converts the entity into a "soft empty"
 * terrain block to allow effects to resolve.
 * @param registry The entt::registry.
 * @param dispatcher The entt::dispatcher to enqueue events.
 * @param terrain The entity to delete or convert.
 */
// [Storage:Hybrid] [Lock:None] [Scope:Entity]
inline void deleteEntityOrConvertInEmpty(entt::registry &registry,
                                         VoxelGrid &voxelGrid,
                                         entt::dispatcher &dispatcher,
                                         entt::entity &terrain,
                                         const Position &pos) {
  // Silenced: per-call entry trace was crowding the log during water-state
  // debugging. Re-enable if entity-id flow needs to be traced again.
  // std::cout << "deleteEntityOrConvertInEmpty: processing entity "
  //           << static_cast<int>(terrain) << " at (" << pos.x << ", " << pos.y
  //           << ", " << pos.z << ")" << std::endl;

  // ON_GRID_STORAGE / NONE / invalid: no entity exists, take the grid-only
  // path using the canonical position the caller supplied.
  const bool hasEntity = terrain != entt::null && registry.valid(terrain);
  if (!hasEntity) {
    voxelGrid.deleteTerrain(dispatcher, pos.x, pos.y, pos.z, false);
    return;
  }

  TileEffectsList *terrainEffectsList =
      registry.try_get<TileEffectsList>(terrain);
  if (terrainEffectsList == nullptr ||
      (terrainEffectsList && terrainEffectsList->tileEffectsIDs.empty())) {
    std::cout << "terrainEffectsList is nullptr or empty... deleting entity"
              << std::endl;
    voxelGrid.deleteTerrain(dispatcher, pos.x, pos.y, pos.z, false);
  } else {
    // Convert into empty terrain because there are effects being processed
    std::cout << "terrainEffectsList && "
                 "terrainEffectsList->tileEffectsIDs.empty(): is "
                 "False... converting into soft empty"
              << std::endl;
    convertIntoSoftEmpty(registry, terrain);
  }
}

// =========================================================================
// ================ 3. VoxelGrid State Mutators = ================
// =========================================================================

/**
 * @brief Sets the components for a coordinate in the TerrainGridRepository to
 * represent an empty water tile.
 * @param registry The entt::registry (used for context, not modified).
 * @param voxelGrid The VoxelGrid for accessing the terrain repository.
 * @param terrainId The ID of the terrain.
 * @param x The x-coordinate to modify.
 * @param y The y-coordinate to modify.
 * @param z The z-coordinate to modify.
 * @param matterState The matter state to assign.
 */
// Part 1: Set EntityTypeComponent
static void setEmptyWaterComponentsStorage(entt::registry &registry,
                                           VoxelGrid &voxelGrid, int terrainId,
                                           int x, int y, int z,
                                           MatterState matterState) {
  // [Storage:Repo] [Lock:None] [Scope:Entity]
  EntityTypeComponent *terrainType = new EntityTypeComponent();
  terrainType->mainType = static_cast<int>(EntityEnum::TERRAIN);
  terrainType->subType0 = static_cast<int>(TerrainEnum::WATER);
  terrainType->subType1 = 0;
  voxelGrid.terrainGridRepository->setTerrainEntityType(x, y, z, *terrainType);

  // Part 2: Set StructuralIntegrityComponent
  StructuralIntegrityComponent *terrainSI = new StructuralIntegrityComponent();
  terrainSI->canStackEntities = false;
  terrainSI->maxLoadCapacity = -1;
  terrainSI->matterState = matterState;
  voxelGrid.terrainGridRepository->setTerrainStructuralIntegrity(x, y, z,
                                                                 *terrainSI);

  // Part 3: Set MatterContainer
  MatterContainer *terrainMC = new MatterContainer();
  terrainMC->TerrainMatter = 0;
  terrainMC->WaterMatter = 0;
  terrainMC->WaterVapor = 0;
  terrainMC->BioMassMatter = 0;
  voxelGrid.terrainGridRepository->setTerrainMatterContainer(x, y, z,
                                                             *terrainMC);
}

/**
 * @brief Convert a repository-backed terrain tile into EMPTY and clear its
 * storage state.
 * @details Acquires a `TerrainGridLock` for the duration of the operation to
 * ensure atomic updates to TerrainGridRepository fields (id, type, matter
 * container, SI).
 * @param registry The entt::registry (kept for symmetry / future use).
 * @param voxelGrid The VoxelGrid containing the repository to modify.
 * @param pos The position of the tile to convert.
 * @param invalidTerrain The entity handle that is being converted (may be
 * invalid).
 */
// [Storage:Repo] [Lock:Internal] [Scope:Entity]
static void convertTerrainTileToEmpty(entt::registry &registry,
                                      entt::dispatcher &dispatcher,
                                      VoxelGrid &voxelGrid, const Position &pos,
                                      entt::entity invalidTerrain) {
  if (!voxelGrid.terrainGridRepository)
    return;

  // RAII lock for repository modifications
  TerrainGridLock lock(voxelGrid.terrainGridRepository.get());

  // Soft-deactivate mapping/components for the entity while we mutate storage
  voxelGrid.terrainGridRepository->softDeactivateEntity(dispatcher,
                                                        invalidTerrain, false);

  // Mark the tile as NONE / EMPTY in repository
  voxelGrid.terrainGridRepository->setTerrainId(
      pos.x, pos.y, pos.z, static_cast<int>(TerrainIdTypeEnum::NONE));
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
  voxelGrid.terrainGridRepository->setTerrainMatterContainer(
      pos.x, pos.y, pos.z, zeroedMatter);

  // Reset Structural Integrity (SI) to the EMPTY defaults
  StructuralIntegrityComponent emptySI = {};
  emptySI.canStackEntities = false;
  emptySI.maxLoadCapacity = -1;
  emptySI.matterState = MatterState::GAS;
  emptySI.gradientVector = GradientVector{};
  voxelGrid.terrainGridRepository->setTerrainStructuralIntegrity(
      pos.x, pos.y, pos.z, emptySI);
}

/**
 * @brief Modifies the StructuralIntegrityComponent of a tile in the VoxelGrid
 * to have vapor properties.
 * @param x The x-coordinate of the tile.
 * @param y The y-coordinate of the tile.
 * @param z The z-coordinate of the tile.
 * @param voxelGrid The VoxelGrid for accessing the terrain repository.
 */
// [Storage:Repo] [Lock:None] [Scope:Entity]
inline void setVaporSI(int x, int y, int z, VoxelGrid &voxelGrid) {
  StructuralIntegrityComponent terrainSI =
      voxelGrid.terrainGridRepository->getTerrainStructuralIntegrity(x, y, z);
  terrainSI.canStackEntities = false;
  terrainSI.maxLoadCapacity = -1;
  terrainSI.matterState = MatterState::GAS;
  voxelGrid.terrainGridRepository->setTerrainStructuralIntegrity(x, y, z,
                                                                 terrainSI);
}

// =========================================================================
// ================ 4. Compound & Orchestration Mutators = ================
// =========================================================================

/**
 * @brief Cleans up entities with zero velocity.
 * @details For non-terrain entities, it removes the Velocity component. For
 * terrain entities, it resets the velocity to zero directly in the
 * TerrainGridRepository.
 * @param registry The entt::registry.
 * @param voxelGrid The VoxelGrid for accessing the terrain repository.
 * @param entity The entity to check.
 * @param position The position of the entity.
 * @param velocity The velocity of the entity.
 * @param isTerrain Flag indicating if the entity is a terrain entity.
 */
inline void cleanupZeroVelocity(entt::registry &registry, VoxelGrid &voxelGrid,
                                entt::entity entity, const Position &position,
                                const Velocity &velocity, bool isTerrain) {
  if (velocity.vx == 0 && velocity.vy == 0 && velocity.vz == 0) {
    if (isTerrain) {
      // std::cout << "[cleanupZeroVelocity] Zeroing Velocity from Terrain!\n";
      voxelGrid.terrainGridRepository->setVelocity(
          position.x, position.y, position.z, {0.0f, 0.0f, 0.0f});
      // voxelGrid.terrainGridRepository->setTerrainId(position.x, position.y,
      // position.z, static_cast<int>(TerrainIdTypeEnum::ON_GRID_STORAGE));
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
static void convertSoftEmptyIntoWater(entt::registry &registry,
                                      VoxelGrid &voxelGrid, int terrainId,
                                      int x, int y, int z) {
  if (terrainId == static_cast<int>(TerrainIdTypeEnum::NONE)) {
    // Create new terrain entity for the empty voxel
  } else if (terrainId ==
             static_cast<int>(TerrainIdTypeEnum::ON_GRID_STORAGE)) {
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
 * @brief A wrapper that performs a read-only check before converting a tile to
 * water.
 * @param registry The entt::registry.
 * @param voxelGrid The VoxelGrid.
 * @param terrainId The ID of the terrain entity/tile.
 * @param x The x-coordinate of the tile.
 * @param y The y-coordinate of the tile.
 * @param z The z-coordinate of the tile.
 */
inline void checkAndConvertSoftEmptyIntoWater(entt::registry &registry,
                                              VoxelGrid &voxelGrid,
                                              int terrainId, int x, int y,
                                              int z) {
  if (getTypeAndCheckSoftEmpty(registry, voxelGrid, terrainId, x, y, z)) {
    convertSoftEmptyIntoWater(registry, voxelGrid, terrainId, x, y, z);
  }
}

/**
 * @brief Converts a soft empty terrain tile into vapor. (Currently a
 * placeholder).
 * @param registry The entt::registry.
 * @param voxelGrid The VoxelGrid.
 * @param terrainId The ID of the terrain entity/tile.
 * @param x The x-coordinate of the tile.
 * @param y The y-coordinate of the tile.
 * @param z The z-coordinate of the tile.
 */
static void convertSoftEmptyIntoVapor(entt::registry &registry,
                                      VoxelGrid &voxelGrid, int terrainId,
                                      int x, int y, int z) {
  std::cout << "[convertSoftEmptyIntoVapor] Just marking a checkpoint on logs."
            << std::endl;
  // TODO: This might be involved in the bug I am debugging.
  // setEmptyWaterComponents(registry, terrain, MatterState::GAS);
}

// Helper: A wrapper that performs a read-only check before converting a tile to
// vapor.
inline void checkAndConvertSoftEmptyIntoVapor(entt::registry &registry,
                                              VoxelGrid &voxelGrid,
                                              int terrainId, int x, int y,
                                              int z) {
  if (getTypeAndCheckSoftEmpty(registry, voxelGrid, terrainId, x, y, z)) {
    convertSoftEmptyIntoVapor(registry, voxelGrid, terrainId, x, y, z);
  }
}

/**
 * @brief Handles dropping items from a dying entity's inventory into the world.
 * @details (Placeholder) This function is intended to read an entity's
 * DropRates component, create new item entities, and place them in the
 * inventory of the tile below the dying entity. The logic is currently
 * commented out in the original LifeEngine.
 * @param registry The entt::registry.
 * @param voxelGrid The VoxelGrid.
 * @param entity The entity dropping items.
 */
inline void dropEntityItems(entt::registry &registry,
                            entt::dispatcher &dispatcher, VoxelGrid &voxelGrid,
                            entt::entity entity) {
  // NOTE: The logic for this function is based on the commented-out `dropItems`
  // function in `LifeEvents.cpp` and serves as a placeholder for the intended
  // functionality.
  std::cout << "Checking for item drops from entity "
            << static_cast<int>(entity) << " (placeholder)." << std::endl;

  if (!registry.valid(entity)) {
    std::cout << "[dropEntityItems] Entity " << static_cast<int>(entity)
              << " is not valid. Aborting." << std::endl;
    return;
  }

  bool hasPosition = registry.all_of<Position>(entity);
  bool hasDropRates = registry.all_of<DropRates>(entity);
  std::cout << "[dropEntityItems] Entity " << static_cast<int>(entity)
            << " hasPosition=" << hasPosition
            << " hasDropRates=" << hasDropRates << std::endl;

  if (hasPosition && hasDropRates) {
    auto &&[pos, dropRates] = registry.get<Position, DropRates>(entity);
    std::cout << "[dropEntityItems] Entity position=(" << pos.x << ", " << pos.y
              << ", " << pos.z << ")"
              << " | itemDropRates count=" << dropRates.itemDropRates.size()
              << std::endl;

    auto terrainBellowId = voxelGrid.getTerrain(pos.x, pos.y, pos.z - 1);
    std::cout << "[dropEntityItems] Terrain below at z-1=" << (pos.z - 1)
              << " terrainBellowId=" << terrainBellowId << std::endl;

    // TODO: TerrainRepository is not supporting Inventory yet. (Only Pure ECS
    // entities)
    if (terrainBellowId == static_cast<int>(TerrainIdTypeEnum::NONE)) {
      std::cout << "[dropEntityItems] No terrain below (terrainBellowId == "
                   "-2). Cannot drop items."
                << std::endl;
    } else {
      entt::entity terrainBellow = static_cast<entt::entity>(terrainBellowId);
      std::cout << "[dropEntityItems] Terrain below entity=" << terrainBellowId
                << " valid=" << registry.valid(terrainBellow) << std::endl;

      Inventory *inventory = registry.try_get<Inventory>(terrainBellow);
      bool shouldEmplaceInventory{inventory == nullptr};
      std::cout << "[dropEntityItems] Terrain below already has inventory="
                << !shouldEmplaceInventory << std::endl;
      if (inventory == nullptr) {
        inventory = new Inventory();
      }

      if (dropRates.itemDropRates.empty()) {
        std::cout
            << "[dropEntityItems] itemDropRates is empty. No items to drop."
            << std::endl;
      } else {
        int itemsAdded = 0;
        for (const auto &[combinedItemId, valuesTuple] :
             dropRates.itemDropRates) {
          auto [itemMainType, itemSubType0] = splitStringToInts(combinedItemId);
          std::cout
              << "[dropEntityItems] Processing drop entry combinedItemId='"
              << combinedItemId << "' itemMainType=" << itemMainType
              << " itemSubType0=" << itemSubType0
              << " FOOD=" << static_cast<int>(ItemEnum::FOOD) << std::endl;

          if (itemMainType == static_cast<int>(ItemEnum::FOOD)) {
            std::shared_ptr<ItemConfiguration> itemConfiguration =
                getItemConfigurationOnManager(combinedItemId);
            if (!itemConfiguration) {
              std::cout << "[dropEntityItems] WARNING: itemConfiguration is "
                           "null for combinedItemId='"
                        << combinedItemId << "'" << std::endl;
              continue;
            }
            auto newFoodItem = itemConfiguration->createFoodItem(registry);
            auto entityId = entt::to_integral(newFoodItem);
            inventory->itemIDs.push_back(entityId);
            itemsAdded++;
            std::cout << "[dropEntityItems] Created food item entity="
                      << entityId << " for combinedItemId='" << combinedItemId
                      << "'" << std::endl;
          } else {
            std::cout << "[dropEntityItems] Item type " << itemMainType
                      << " is not FOOD. Skipping." << std::endl;
          }
        }

        std::cout << "[dropEntityItems] Total items added to inventory="
                  << itemsAdded << std::endl;

        if (shouldEmplaceInventory) {
          // convertTerrainTileToEmpty(registry, dispatcher, voxelGrid, pos,
          // terrainBellow); auto newTerrainBellowId =
          // createEmptyActiveTerrain(registry, voxelGrid, pos.x, pos.y, pos.z -
          // 1);

          entt::entity newTerrainBellow = terrainBellow;
          if (terrainBellowId ==
              static_cast<int>(TerrainIdTypeEnum::ON_GRID_STORAGE)) {
            auto newTerrainEntity = registry.create();
            Position newPosition = {pos.x, pos.y, pos.z - 1,
                                    DirectionEnum::DOWN};
            registry.emplace<Position>(newTerrainEntity, newPosition);
            // voxelGrid.terrainGridRepository->setPosition(x, y, z,
            // newPosition);
            int newTerrainId = static_cast<int>(newTerrainEntity);
            voxelGrid.terrainGridRepository->setTerrainId(
                pos.x, pos.y, pos.z - 1, newTerrainId);

            VoxelCoord key{newPosition.x, newPosition.y, newPosition.z};
            voxelGrid.terrainGridRepository->addToTrackingMaps(
                key, newTerrainEntity);

            newTerrainBellow = newTerrainEntity;
          }
          // Here I also need to make the terrain, empty terrain if it is -2
          registry.emplace<Inventory>(newTerrainBellow, *inventory);
          std::cout
              << "[dropEntityItems] Emplacing new Inventory on terrain entity="
              << static_cast<int>(newTerrainBellow) << std::endl;
        } else {
          std::cout << "[dropEntityItems] Terrain already has Inventory, items "
                       "appended in place."
                    << std::endl;
        }
      }
    }
  }
}

/**
 * @brief Removes an entity from its position in the VoxelGrid.
 * @details It checks the entity's type to call the appropriate grid deletion
 * method (e.g., `deleteTerrain` or `deleteEntity`). It includes safety checks
 * to ensure the correct entity is being removed from the grid.
 * @param registry The entt::registry.
 * @param voxelGrid The VoxelGrid.
 * @param dispatcher The entt::dispatcher, used for terrain deletion events.
 * @param entity The entity to remove from the grid.
 */
inline void removeEntityFromGrid(entt::registry &registry, VoxelGrid &voxelGrid,
                                 entt::dispatcher &dispatcher,
                                 entt::entity entity, bool takeLock = true) {
  int entityId = static_cast<int>(entity);
  bool isSpecialId = entityId == -1 || entityId == -2;
  if (!isSpecialId && registry.valid(entity) &&
      registry.all_of<Position, EntityTypeComponent>(entity)) {
    std::ostringstream ossMessage;
    // ossMessage << "[removeEntityFromGrid] Removing entity from grid: " <<
    // entityId; spdlog::get("console")->info(ossMessage.str());
    auto &&[pos, type] = registry.get<Position, EntityTypeComponent>(entity);

    int currentGridEntity = voxelGrid.getEntity(pos.x, pos.y, pos.z);
    if (currentGridEntity != entityId) {
      // std::ostringstream ossMessage2;
      // ossMessage2 << "[removeEntityFromGrid] WARNING: Grid position (" <<
      // pos.x << ","
      //             << pos.y << "," << pos.z << ") contains entity " <<
      //             currentGridEntity
      //             << " but trying to remove entity " << entityId;
      // spdlog::get("console")->info(ossMessage2.str());
      return;
    }

    if (type.mainType == static_cast<int>(EntityEnum::TERRAIN)) {
      // spdlog::get("console")->info(
      //     "[removeEntityFromGrid] Entity is terrain, calling
      //     deleteTerrain.");
      voxelGrid.deleteTerrain(dispatcher, pos.x, pos.y, pos.z, takeLock);
    } else if (type.mainType == static_cast<int>(EntityEnum::BEAST) ||
               type.mainType == static_cast<int>(EntityEnum::PLANT)) {
      voxelGrid.deleteEntity(pos.x, pos.y, pos.z);
    }
  } else if (isSpecialId) {
    std::ostringstream ossMessage;
    // ossMessage << "[removeEntityFromGrid] Entity " << entityId
    //            << " is a special ID, skipping grid removal.";
    // spdlog::get("console")->info(ossMessage.str());
  } else if (!isSpecialId && registry.valid(entity)) {
    Position *position = registry.try_get<Position>(entity);
    EntityTypeComponent *entityType =
        registry.try_get<EntityTypeComponent>(entity);
    if (position) {
      std::ostringstream ossMessage;
      // ossMessage << "[removeEntityFromGrid] Entity " << entityId
      //            << " has Position component at (" << position->x << ", " <<
      //            position->y
      //            << ", " << position->z << ").";
      spdlog::get("console")->info(ossMessage.str());
    } else {
      std::ostringstream ossMessage;
      // ossMessage << "[removeEntityFromGrid] Entity " << entityId
      //            << " is missing Position component.";
      // spdlog::get("console")->info(ossMessage.str());
      Position _pos =
          voxelGrid.terrainGridRepository->getPositionOfEntt(entity);
      position = &_pos;
    }

    // std::ostringstream ossMessage3;
    // ossMessage3
    //     << "[removeEntityFromGrid] Entity " << entityId
    //     << " is missing Position or EntityTypeComponent, checking
    //     TerrainGridRepository.";
    // spdlog::get("console")->info(ossMessage3.str());
    if (position->x == -1 && position->y == -1 && position->z == -1) {
      // std::ostringstream ossMessage4;
      // ossMessage4 << "[removeEntityFromGrid] Could not find position of
      // entity "
      //             << entityId << " in TerrainGridRepository, skipping grid
      //             removal.";
      // spdlog::get("console")->info(ossMessage4.str());
      throw std::runtime_error("Entity is missing Position component and not "
                               "found in TerrainGridRepository.");
    } else {
      // std::ostringstream ossMessage5;
      // ossMessage5 << "[removeEntityFromGrid] Removing entity " << entityId
      //             << " from grid using position from TerrainGridRepository at
      //             ("
      //             << position->x << ", " << position->y << ", " <<
      //             position->z << ").";
      // spdlog::get("console")->info(ossMessage5.str());
      voxelGrid.deleteTerrain(dispatcher, position->x, position->y, position->z,
                              takeLock);
    }
  } else {
    // std::ostringstream ossMessage;
    // ossMessage << "[removeEntityFromGrid] Entity " << entityId
    //            << " is invalid, skipping grid removal.";
    // spdlog::get("console")->info(ossMessage.str());
  }
}

/**
 * @brief Removes an entity from terrain storage (VoxelGrid /
 * TerrainGridRepository)
 * @details This is a free function variant of the previous
 * World::removeEntityFromTerrain. The caller is responsible for holding any
 * lifecycle locks (e.g., entityLifecycleMutex) if required by the caller's
 * locking contract. This function will acquire a TerrainGridLock when modifying
 * the repository if `removeFromGrid` is true.
 */
inline void removeEntityFromTerrain(entt::registry &registryRef,
                                    VoxelGrid &voxelGridRef,
                                    entt::dispatcher &dispatcherRef,
                                    entt::entity entity, bool removeFromGrid) {
  if (!registryRef.valid(entity)) {
    std::cout << "removeEntityFromTerrain: entity invalid, skipping: "
              << static_cast<int>(entity) << std::endl;
    return;
  }

  const int entityId = static_cast<int>(entity);

  if (removeFromGrid) {
    std::cout << "removeEntityFromTerrain: removing entity from grid: "
              << entityId << std::endl;
    // Acquire TerrainGridLock for the duration of grid modification
    if (voxelGridRef.terrainGridRepository) {
      TerrainGridLock terrainLock(voxelGridRef.terrainGridRepository.get());
      // removeEntityFromGrid handles voxel bookkeeping; do not destroy here
      removeEntityFromGrid(registryRef, voxelGridRef, dispatcherRef, entity,
                           false);
    } else {
      // Fallback: still call removeEntityFromGrid even if repo pointer missing
      removeEntityFromGrid(registryRef, voxelGridRef, dispatcherRef, entity,
                           false);
    }
  } else {
    std::cout << "removeEntityFromTerrain: skip grid removal for entity: "
              << entityId << std::endl;
  }
}

/**
 * @brief Destroys an entity and performs grid/repository cleanup.
 * @param registry The entt::registry.
 * @param voxelGrid The VoxelGrid for repository access.
 * @param dispatcher The dispatcher used by removeEntityFromGrid if necessary.
 * @param entity The entity to destroy.
 * @param takeGridLock If true, callers will attempt to take repository locks
 * when modifying the grid.
 *
 * Locking contract: this function DOES NOT acquire World::entityLifecycleMutex.
 * The caller must hold any lifecycle locks required to prevent races with
 * perception/creation. This function will acquire a TerrainGridLock when
 * performing grid modifications if `takeGridLock` is true.
 */
inline void destroyEntityWithGridCleanup(entt::registry &registry,
                                         VoxelGrid &voxelGrid,
                                         entt::dispatcher &dispatcher,
                                         entt::entity entity,
                                         bool takeGridLock = true) {
  if (!registry.valid(entity)) {
    std::cout << "destroyEntityWithGridCleanup: entity invalid, skipping: "
              << static_cast<int>(entity) << std::endl;
    registry.destroy(entity); // Ensure cleanup of any remaining components if
                              // entity still exists
    return;
  }

  const int entityId = static_cast<int>(entity);

  // Special terrain markers (-1, -2) should not be destroyed through ECS
  if (entityId == -1 || entityId == -2) {
    std::cout << "destroyEntityWithGridCleanup: skipping special ID "
              << entityId << std::endl;

    return;
  }

  try {
    // Remove references from VoxelGrid / TerrainGridRepository first
    removeEntityFromGrid(registry, voxelGrid, dispatcher, entity, takeGridLock);
  } catch (const std::exception &e) {
    std::cout << "destroyEntityWithGridCleanup: removeEntityFromGrid failed "
                 "for entity "
              << entityId << ": " << e.what() << std::endl;
  }

  // Ensure the entity is destroyed in the registry. Do not attempt to re-lock
  // the repository here because removeEntityFromGrid already handled grid
  // locking when requested.
  if (registry.valid(entity)) {
    _destroyEntity(registry, dispatcher, voxelGrid, entity, false);
  } else {
    std::cout << "destroyEntityWithGridCleanup: entity already invalid at "
                 "destroy step for entity "
              << entityId << std::endl;
    registry.destroy(entity); // Ensure cleanup of any remaining components if
                              // entity still exists
  }
}

/**
 * @brief Performs a "soft kill" on an entity, removing its life components and
 * grid representation.
 * @details A soft kill removes essential life components like Health and
 * Metabolism, effectively making the entity "dead" without immediately
 * destroying the entity handle. It also removes the entity from the main
 * VoxelGrid representation.
 * @param registry The entt::registry.
 * @param voxelGrid The VoxelGrid.
 * @param dispatcher The entt::dispatcher, used for terrain deletion events.
 * @param entity The entity to be soft-killed.
 */
inline void softKillEntity(entt::registry &registry, VoxelGrid &voxelGrid,
                           entt::dispatcher &dispatcher, entt::entity entity) {
  int entityId = static_cast<int>(entity);
  std::cout << "Performing soft kill on entity: " << entityId << std::endl;

  // Safely remove MetabolismComponent if it exists
  if (registry.all_of<MetabolismComponent>(entity)) {
    registry.remove<MetabolismComponent>(entity);
    std::cout << "Removed MetabolismComponent from entity " << entityId
              << std::endl;
  }

  // Safely remove HealthComponent if it exists
  if (registry.all_of<HealthComponent>(entity)) {
    registry.remove<HealthComponent>(entity);
    std::cout << "Removed HealthComponent from entity " << entityId
              << std::endl;
  }

  if (registry.all_of<Velocity>(entity)) {
    registry.remove<Velocity>(entity);
    std::cout << "Removed Velocity from entity " << entityId << std::endl;
  }

  if (registry.all_of<MovingComponent>(entity)) {
    registry.remove<MovingComponent>(entity);
    std::cout << "Removed MovingComponent from entity " << entityId
              << std::endl;
  }

  removeEntityFromGrid(registry, voxelGrid, dispatcher, entity);
}

/**
 * @brief A complex handler for "dormant" or invalid terrain entities that still
 * have a Velocity component.
 * @details It attempts to reactivate a valid terrain entity from the
 * `TerrainGridRepository`. If revival fails, it may destroy the entity or
 * convert it to an EMPTY tile.
 * @param registry The entt::registry.
 * @param voxelGrid The VoxelGrid.
 * @param dispatcher The entt::dispatcher.
 * @param positionOfEntt The last known position of the entity.
 * @param invalidTerrain The entity handle, which may be invalid.
 * @return A valid, revived entity handle.
 * @throws aetherion::InvalidEntityException if the entity cannot be revived.
 * @throws std::runtime_error for other fatal errors.
 */
inline entt::entity reviveColdTerrainEntities(entt::registry &registry,
                                              VoxelGrid &voxelGrid,
                                              entt::dispatcher &dispatcher,
                                              Position &positionOfEntt,
                                              entt::entity &invalidTerrain) {
  int invalidTerrainId = static_cast<int>(invalidTerrain);
  Position positionOnTerrainGrid =
      voxelGrid.terrainGridRepository->getPositionOfEntt(invalidTerrain);

  // std::cout << "[processPhysics] Found position of entity " <<
  // invalidTerrainId
  //           << " in TerrainGridRepository at (" << positionOfEntt.x << ", "
  //           << positionOfEntt.y
  //           << ", " << positionOfEntt.z << ")" << " - checking if vapor
  //           terrain needs revival"
  //           << std::endl;

  // Check if this is vapor terrain that needs to be revived
  EntityTypeComponent terrainType =
      voxelGrid.terrainGridRepository->getTerrainEntityType(
          positionOfEntt.x, positionOfEntt.y, positionOfEntt.z);
  int vaporMatter = voxelGrid.terrainGridRepository->getVaporMatter(
      positionOfEntt.x, positionOfEntt.y, positionOfEntt.z);
  int waterMatter = voxelGrid.terrainGridRepository->getWaterMatter(
      positionOfEntt.x, positionOfEntt.y, positionOfEntt.z);

  if (terrainType.mainType == static_cast<int>(EntityEnum::TERRAIN) &&
      terrainType.subType0 == static_cast<int>(TerrainEnum::WATER) &&
      vaporMatter > 0 && waterMatter == 0) {
    std::cout << "[processPhysics] Reviving cold vapor terrain at ("
              << positionOfEntt.x << ", " << positionOfEntt.y << ", "
              << positionOfEntt.z << ") with vapor matter: " << vaporMatter
              << std::endl;

    // Revive the terrain by ensuring it's active in ECS
    entt::entity entity = voxelGrid.terrainGridRepository->ensureActive(
        positionOfEntt.x, positionOfEntt.y, positionOfEntt.z);

    // std::cout << "[processPhysics] Revived vapor terrain as entity " <<
    // static_cast<int>(entity)
    //           << std::endl;
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
      // std::cout << "[reviveColdTerrainEntities] WARNING: Discrepancy in vapor
      // matter values! "
      //              "VoxelGrid reports "
      //           << vaporMatter << ", but MatterContainer has " <<
      //           matterContainer.WaterVapor
      //           << std::endl;
      // Convert repository-backed tile into EMPTY and clear storage (under repo
      // lock)
      convertTerrainTileToEmpty(registry, dispatcher, voxelGrid, positionOfEntt,
                                invalidTerrain);
      // std::cout << "[reviveColdTerrainEntities] Converted terrain entity " <<
      // invalidTerrainId
      //           << " into empty terrain due to zero water matter." <<
      //           std::endl;
      throw aetherion::InvalidEntityException(
          "Entity with Velocity had zero vapor matter; converted to empty "
          "terrain");

    } else if (terrainType.mainType == static_cast<int>(EntityEnum::TERRAIN) &&
               terrainType.subType0 == static_cast<int>(TerrainEnum::EMPTY)) {
      throw aetherion::InvalidEntityException(
          "Terrain is EMPTY; cannot be revived");

    } else {
      // printTerrainDiagnostics(registry, voxelGrid, invalidTerrain,
      // positionOfEntt,
      //                         terrainType, vaporMatter);
      throw aetherion::InvalidEntityException(
          "Entity with Velocity is invalid and cannot be revived; skipping");
    }
  }
}

/**
 * @brief Orchestrates the handling of an invalid entity detected during physics
 * movement.
 * @details It attempts to revive the entity by calling
 * `reviveColdTerrainEntities`. If that fails, or if the entity's position is
 * not found, it destroys the entity.
 * @param registry The entt::registry.
 * @param voxelGrid The VoxelGrid.
 * @param dispatcher The entt::dispatcher.
 * @param entity The entity handle, which may be invalid.
 * @return A valid, potentially new, entity handle if revival was successful.
 * @throws aetherion::InvalidEntityException if the entity cannot be handled and
 * must be skipped.
 */
inline entt::entity handleInvalidEntityForMovement(entt::registry &registry,
                                                   VoxelGrid &voxelGrid,
                                                   entt::dispatcher &dispatcher,
                                                   entt::entity entity) {
  // Entity is invalid but still listed in the velocity view — skip
  // defensively; tracking-map cleanup happens at the explicit remove site
  // (moveTerrain / deleteTerrain).
  std::cout << "[handleMovement] WARNING: Invalid entity in velocityView - "
               "skipping; entity ID="
            << static_cast<int>(entity) << std::endl;

  Position pos = {-1, -1, -1, DirectionEnum::DOWN};
  try {
    pos = voxelGrid.terrainGridRepository->getPositionOfEntt(entity);
  } catch (const aetherion::InvalidEntityException &e) {
    convertTerrainTileToEmpty(registry, dispatcher, voxelGrid, pos, entity);
    // voxelGrid.terrainGridRepository->softDeactivateEntity(entity);
    throw; // Re-throw to be caught by handleMovement
  }
  int entityId = static_cast<int>(entity);
  if (pos.x == -1 && pos.y == -1 && pos.z == -1) {
    std::cout << "[handleMovement] Could not find position of entity "
              << entityId << " in TerrainGridRepository - soft-deactivating it."
              << std::endl;

    convertTerrainTileToEmpty(registry, dispatcher, voxelGrid, pos, entity);
    // voxelGrid.terrainGridRepository->softDeactivateEntity(entity);
    // Throw exception to signal to caller that processing for this entity
    // should stop.
    throw aetherion::InvalidEntityException(
        "Entity soft-deactivated as it could not be found in "
        "TerrainGridRepository");

  } else {
    try {
      return reviveColdTerrainEntities(registry, voxelGrid, dispatcher, pos,
                                       entity);
    } catch (const aetherion::InvalidEntityException &e) {
      // Entity cannot be revived (e.g., zero vapor matter converted to empty)
      std::cout << "[handleMovement] Revival failed: " << e.what()
                << " - entity ID=" << entityId << std::endl;
      throw; // Re-throw to be caught by handleMovement
    }
  }
}

/**
 * @brief Creates a new water terrain voxel from a "water fall" event.
 * @details This function writes the destination voxel directly into terrain
 * storage without creating an ECS entity.
 * @note This function performs its own manual grid locking.
 * @param registry The entt::registry.
 * @param voxelGrid The VoxelGrid.
 * @param x The x-coordinate for the new water tile.
 * @param y The y-coordinate for the new water tile.
 * @param z The z-coordinate for the new water tile.
 * @param fallingAmount The amount of water matter for the new tile.
 * @param sourceEntity The entity from which the water is falling.
 */
// Cap on how many times a water-creation event (fall or condensation)
// will be re-dispatched after hitting a vapor-only destination with no
// resolution available. Three retries gives surrounding vapor a few
// ticks to disperse via the regular vapor physics path before we give
// up; without a cap we'd loop forever on a sealed vapor pocket.
inline constexpr int WATER_VAPOR_CONFLICT_RETRY_LIMIT = 3;

inline void createWaterTerrainFromFall(entt::registry &registry,
                                       entt::dispatcher &dispatcher,
                                       VoxelGrid &voxelGrid, int x, int y,
                                       int z, double fallingAmount,
                                       entt::entity sourceEntity,
                                       Position sourcePos, int retryCount = 0) {
  if (!voxelGrid.terrainGridRepository) {
    spdlog::warn("createWaterTerrainFromFall: missing terrainGridRepository");
    return;
  }
  TerrainGridLock lock(voxelGrid.terrainGridRepository.get());

  // Capture original destination coords for the retry path — the loop
  // below may rewrite x/y if it finds a horizontal redirect.
  const int originalDestX = x;
  const int originalDestY = y;
  const int originalDestZ = z;

  // Snapshot destination state. The matter write below is *additive* so a
  // populated water destination keeps its existing WaterMatter (and any
  // vapor / biomass / terrain matter already present). Type / SIC / physics
  // scaffolding only gets written when the destination was previously NONE.
  //
  // For NONE cells we deliberately do *not* read matter from storage: orphan
  // values can linger in the WaterVapor / WaterMatter VDB grids when an
  // upstream path cleared the terrain id without zeroing the matter
  // container. Reading those would let the additive write below produce a
  // both-WaterMatter-and-WaterVapor container; the subsequent
  // setTerrainMatterContainer would then overwrite the stale grid values
  // with the new ones from the local container, so zero-initialising here
  // both prevents the violation and cleans up the orphan in one step. This
  // matches the conditional-read pattern in `_handleWaterCreationEvent`.
  int64_t destTerrainId = voxelGrid.getTerrain(x, y, z);
  bool destinationIsEmpty =
      destTerrainId == static_cast<int64_t>(TerrainIdTypeEnum::NONE);

  MatterContainer destMatter{};
  if (!destinationIsEmpty) {
    destMatter =
        voxelGrid.terrainGridRepository->getTerrainMatterContainer(x, y, z);
  }

  // Vapor-only safeguard. If the destination cell already exists as a water
  // cell whose matter is purely vapor (WaterVapor > 0, WaterMatter <= 0),
  // writing the falling liquid into it would create a both-WaterMatter-and-
  // WaterVapor cell, which the per-tick water invariant check forbids. We
  // first scan the four horizontal neighbors at the same z for an EMPTY or
  // liquid-water cell to redirect the fall to. If none match, we re-dispatch
  // the same `WaterFallEntityEvent` with an incremented retry counter so the
  // surrounding vapor has a chance to disperse on the next tick. After
  // `WATER_VAPOR_CONFLICT_RETRY_LIMIT` retries we abort with a warn log to keep
  // the dispatcher queue from growing unbounded on a genuinely sealed pocket.
  //
  // The pure-abort alternative (drop the water on retry-exhaust without any
  // log) is *not* used here because the warn log is the only diagnostic we
  // get for sealed-pocket recurrences in the live game. If retries ever
  // prove buggy or sealed-pocket cases turn out to be common, a future
  // change can swap the body of the retry branch for a one-line return —
  // but lose the source water — and a follow-up wake-up trigger would be
  // required to restart it later.
  //
  // The principled fix is to push vapor sideways instead of redirecting
  // the falling liquid. That belongs in the vapor-physics path and is out
  // of scope here.
  EntityTypeComponent destType =
      voxelGrid.terrainGridRepository->getTerrainEntityType(x, y, z);
  bool destinationIsVaporOnly =
      !destinationIsEmpty &&
      destType.mainType == static_cast<int>(EntityEnum::TERRAIN) &&
      destType.subType0 == static_cast<int>(TerrainEnum::WATER) &&
      destMatter.WaterVapor > 0 && destMatter.WaterMatter <= 0;

  bool redirectedAwayFromVapor = false;
  if (destinationIsVaporOnly) {
    static constexpr int dx[] = {1, -1, 0, 0};
    static constexpr int dy[] = {0, 0, 1, -1};
    for (int i = 0; i < 4; ++i) {
      int nx = x + dx[i];
      int ny = y + dy[i];
      if (nx < 0 || nx >= voxelGrid.width || ny < 0 || ny >= voxelGrid.height) {
        continue;
      }
      int64_t neighborId = voxelGrid.getTerrain(nx, ny, z);
      if (neighborId == static_cast<int64_t>(TerrainIdTypeEnum::NONE)) {
        x = nx;
        y = ny;
        destinationIsEmpty = true;
        destMatter =
            voxelGrid.terrainGridRepository->getTerrainMatterContainer(x, y, z);
        redirectedAwayFromVapor = true;
        break;
      }
      EntityTypeComponent nType =
          voxelGrid.terrainGridRepository->getTerrainEntityType(nx, ny, z);
      if (nType.mainType != static_cast<int>(EntityEnum::TERRAIN) ||
          nType.subType0 != static_cast<int>(TerrainEnum::WATER)) {
        continue;
      }
      MatterContainer nMatter =
          voxelGrid.terrainGridRepository->getTerrainMatterContainer(nx, ny, z);
      if (nMatter.WaterMatter > 0) {
        x = nx;
        y = ny;
        destinationIsEmpty = false;
        destMatter = nMatter;
        redirectedAwayFromVapor = true;
        break;
      }
    }

    if (!redirectedAwayFromVapor) {
      if (retryCount < WATER_VAPOR_CONFLICT_RETRY_LIMIT) {
        Position retryDest{originalDestX, originalDestY, originalDestZ,
                           DirectionEnum::DOWN};
        dispatcher.enqueue<WaterFallEntityEvent>(WaterFallEntityEvent{
            sourceEntity, sourcePos, retryDest, static_cast<int>(fallingAmount),
            retryCount + 1});
        return;
      }
      spdlog::get("console")->warn(
          "[createWaterTerrainFromFall] Falling water gave up after {} "
          "retries at ({}, {}, {}); destination cell holds only vapor and "
          "no horizontal neighbor is available — vapor pocket may be "
          "sealed. Source water remains in place at ({}, {}, {}).",
          WATER_VAPOR_CONFLICT_RETRY_LIMIT, originalDestX, originalDestY,
          originalDestZ, sourcePos.x, sourcePos.y, sourcePos.z);
      return;
    }
  }

  if (destinationIsEmpty) {
    Position newPosition = {x, y, z, DirectionEnum::DOWN};

    EntityTypeComponent newType = {};
    newType.mainType = static_cast<int>(EntityEnum::TERRAIN);
    newType.subType0 = static_cast<int>(TerrainEnum::WATER);
    newType.subType1 = 0;

    PhysicsStats newPhysicsStats = {};
    newPhysicsStats.mass = 20;
    newPhysicsStats.maxSpeed = 10;
    newPhysicsStats.minSpeed = 0.0;

    StructuralIntegrityComponent newSIC = {};
    newSIC.canStackEntities = false;
    newSIC.maxLoadCapacity = -1;
    newSIC.matterState = MatterState::LIQUID;

    voxelGrid.terrainGridRepository->setPosition(x, y, z, newPosition);
    voxelGrid.terrainGridRepository->setTerrainEntityType(x, y, z, newType);
    voxelGrid.terrainGridRepository->setTerrainStructuralIntegrity(x, y, z,
                                                                   newSIC);
    voxelGrid.terrainGridRepository->setPhysicsStats(x, y, z, newPhysicsStats);

    voxelGrid.terrainGridRepository->setTerrainId(
        x, y, z, static_cast<int>(TerrainIdTypeEnum::ON_GRID_STORAGE));
  }

  destMatter.WaterMatter += fallingAmount;
  _logIfViolatingMatterWrite("createWaterTerrainFromFall:dest", x, y, z,
                             destMatter);
  voxelGrid.terrainGridRepository->setTerrainMatterContainer(x, y, z,
                                                             destMatter);

  MatterContainer sourceMatter =
      voxelGrid.terrainGridRepository->getTerrainMatterContainer(
          sourcePos.x, sourcePos.y, sourcePos.z);
  sourceMatter.WaterMatter -= fallingAmount;
  _logIfViolatingMatterWrite("createWaterTerrainFromFall:source", sourcePos.x,
                             sourcePos.y, sourcePos.z, sourceMatter);
  voxelGrid.terrainGridRepository->setTerrainMatterContainer(
      sourcePos.x, sourcePos.y, sourcePos.z, sourceMatter);

  // Seed an initial downward gravity velocity so the new ON_GRID_STORAGE
  // voxel enters the VDB velocity grid and the velocity-driven pass
  // picks it up next tick. ON_GRID_STORAGE water has no Position
  // component in the registry and is therefore invisible to the
  // ECS-only iteration in `processPhysicsAsync` — without this seed it
  // would never start falling. Only seed when the cell directly below
  // is truly empty (NONE); water onto water and water onto solid are
  // handled by other paths.
  if (z > 0 && voxelGrid.getTerrain(x, y, z - 1) ==
                   static_cast<int64_t>(TerrainIdTypeEnum::NONE)) {
    float gravityKick = PhysicsManager::Instance()->getGravity();
    voxelGrid.terrainGridRepository->setVelocity(
        x, y, z, Velocity{0.0f, 0.0f, -gravityKick});
  }
}

// Definition: src/physics/mutators/WaterPhysicsMutators.cpp
void setGravityFlowWaterTargetDefaults(VoxelGrid &voxelGrid,
                                       const Position &targetPos,
                                       const PhysicsStats &physicsStats);

// Definition: src/physics/mutators/WaterPhysicsMutators.cpp
void setGravityFlowEmptySourceDefaults(VoxelGrid &voxelGrid,
                                       const Position &sourcePos);

inline void createWaterTerrainFromGravityFlow(
    VoxelGrid &voxelGrid, const Position &targetPos, int targetTerrainId,
    const PhysicsStats &sourcePhysicsStats) {
  if (!voxelGrid.terrainGridRepository) {
    spdlog::warn("createWaterTerrainFromGravityFlow: missing "
                 "terrainGridRepository");
    return;
  }

  Position gravityTargetPos = targetPos;
  gravityTargetPos.direction = DirectionEnum::DOWN;

  // Today the only callers of this function reach it on NONE or EMPTY-typed
  // ON_GRID_STORAGE destinations (per `_handleWaterGravityFlowEvent`'s entry
  // condition) — both pre-water states that need the full water-cell
  // scaffolding written. We still gate the scaffolding defensively against
  // an existing-water destination so a future caller would only get an
  // additive matter merge (matter is owned by the handler), preserving
  // type / SIC / PhysicsStats already in place.
  EntityTypeComponent existingType =
      voxelGrid.terrainGridRepository->getTerrainEntityType(
          gravityTargetPos.x, gravityTargetPos.y, gravityTargetPos.z);
  bool destinationNeedsScaffolding =
      targetTerrainId == static_cast<int>(TerrainIdTypeEnum::NONE) ||
      existingType.mainType != static_cast<int>(EntityEnum::TERRAIN) ||
      existingType.subType0 != static_cast<int>(TerrainEnum::WATER);

  if (destinationNeedsScaffolding) {
    setGravityFlowWaterTargetDefaults(voxelGrid, gravityTargetPos,
                                      sourcePhysicsStats);
  }

  voxelGrid.terrainGridRepository->setTerrainId(
      gravityTargetPos.x, gravityTargetPos.y, gravityTargetPos.z,
      static_cast<int>(TerrainIdTypeEnum::ON_GRID_STORAGE), false);

  // Seed an initial downward gravity velocity so the newly-flowed water
  // voxel enters the VDB velocity grid and keeps falling on subsequent
  // ticks — e.g., when a gravity-flow event lands water at the top of a
  // deep empty column. See `createWaterTerrainFromFall` for the same
  // rationale spelled out in full.
  if (gravityTargetPos.z > 0 &&
      voxelGrid.getTerrain(gravityTargetPos.x, gravityTargetPos.y,
                           gravityTargetPos.z - 1) ==
          static_cast<int64_t>(TerrainIdTypeEnum::NONE)) {
    float gravityKick = PhysicsManager::Instance()->getGravity();
    voxelGrid.terrainGridRepository->setVelocity(
        gravityTargetPos.x, gravityTargetPos.y, gravityTargetPos.z,
        Velocity{0.0f, 0.0f, -gravityKick});
  }
}

/**
 * @brief Adds vapor to an existing tile above a source or creates a new vapor
 * entity if no tile exists.
 * @param registry The entt::registry.
 * @param voxelGrid The VoxelGrid.
 * @param x The x-coordinate of the source tile.
 * @param y The y-coordinate of the source tile.
 * @param z The z-coordinate of the source tile.
 * @param amount The amount of vapor to add.
 */
inline void addOrCreateVaporAbove(entt::registry &registry,
                                  VoxelGrid &voxelGrid, int x, int y, int z,
                                  int amount) {
  // Ensure repository lock for atomic check+write (avoid TOCTOU if caller
  // didn't hold lock)
  std::unique_ptr<TerrainGridLock> lockGuard;
  if (voxelGrid.terrainGridRepository &&
      !voxelGrid.terrainGridRepository->isTerrainGridLocked()) {
    lockGuard = std::make_unique<TerrainGridLock>(
        voxelGrid.terrainGridRepository.get());
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
        matterContainerAbove.WaterVapor >= 0 &&
        matterContainerAbove.WaterMatter == 0) {
      matterContainerAbove.WaterVapor += amount;
      std::ostringstream oss;
      oss << "[addOrCreateVaporAbove] Added vapor at (" << x << ", " << y
          << ", " << (z + 1) << ")." << " type=" << typeAbove.mainType
          << ", subtype=" << typeAbove.subType0
          << ", WaterMatter=" << matterContainerAbove.WaterMatter
          << ", WaterVapor=" << matterContainerAbove.WaterVapor;
      spdlog::get("console")->info(oss.str());
      _logIfViolatingMatterWrite("addOrCreateVaporAbove", x, y, z + 1,
                                 matterContainerAbove);
      voxelGrid.terrainGridRepository->setTerrainMatterContainer(
          x, y, z + 1, matterContainerAbove);
    } else {
      std::ostringstream oss;
      oss << "[addOrCreateVaporAbove] Cannot add vapor at (" << x << ", " << y
          << ", " << (z + 1) << ") - target not vapor-transitory or is liquid."
          << " type=" << typeAbove.mainType
          << ", subtype=" << typeAbove.subType0
          << ", WaterMatter=" << matterContainerAbove.WaterMatter
          << ", WaterVapor=" << matterContainerAbove.WaterVapor;
      spdlog::get("console")->info(oss.str());
    }
  } else {
    // No entity above; create new vapor terrain entity
    createVaporTerrainEntity(registry, voxelGrid, x, y, z + 1, amount);
  }
}

/**
 * @brief Creates a new water tile below a vapor tile during condensation.
 * @details This is a compound function that creates a new water entity with all
 * its components, updates the `VoxelGrid`, modifies the source vapor tile's
 * `MatterContainer`, and may destroy the vapor entity if it's depleted.
 *
 * @note This function acquires a TerrainGridLock internally for atomic state
 * changes. Caller MUST NOT hold the terrain grid lock before calling this
 * function.
 *
 * @note Enforces the invariant: vapor tile must have WaterMatter == 0 (cannot
 * have both liquid and vapor).
 *
 * @param registry The entt::registry.
 * @param voxelGrid The VoxelGrid.
 * @param vaporX The x-coordinate of the source vapor tile.
 * @param vaporY The y-coordinate of the source vapor tile.
 * @param vaporZ The z-coordinate of the source vapor tile.
 * @param condensationAmount The amount of water to create.
 * @param vaporMatter A reference to the `MatterContainer` of the source vapor
 * tile.
 * @throws std::runtime_error if vapor tile already contains WaterMatter (state
 * invariant violation).
 */
inline void createWaterTerrainBelowVapor(entt::registry &registry,
                                         entt::dispatcher &dispatcher,
                                         VoxelGrid &voxelGrid, int vaporX,
                                         int vaporY, int vaporZ,
                                         double condensationAmount,
                                         MatterContainer &vaporMatter,
                                         int retryCount = 0) {
  // Lock for atomic state change
  // TerrainGridLock lock(voxelGrid.terrainGridRepository.get());

  // Validate state invariant: vapor tile must not have liquid water
  if (vaporMatter.WaterMatter > 0) {
    std::ostringstream oss;
    oss << "[createWaterTerrainBelowVapor] State invariant violation at ("
        << vaporX << ", " << vaporY << ", " << vaporZ
        << "): " << "Vapor tile has WaterMatter=" << vaporMatter.WaterMatter
        << " and WaterVapor=" << vaporMatter.WaterVapor
        << ". Cannot condense vapor when liquid water already present.";
    throw std::runtime_error(oss.str());
  }

  // spdlog::get("console")->info(
  //     "[createWaterTerrainBelowVapor] Creating water terrain below vapor at
  //     (" + std::to_string(vaporX) + ", " + std::to_string(vaporY) + ", " +
  //     std::to_string(vaporZ - 1) +
  //     ") with condensation amount: " + std::to_string(condensationAmount));

  // Snapshot destination state. Matter is written *additively* so a future
  // caller hitting an existing-water destination keeps its WaterMatter (and
  // any other matter fields already present). Type / SIC / physics
  // scaffolding only gets written when the destination was previously NONE.
  int destX = vaporX;
  int destY = vaporY;
  int destZ = vaporZ - 1;
  int64_t destTerrainId = voxelGrid.getTerrain(destX, destY, destZ);
  bool destinationIsEmpty =
      destTerrainId == static_cast<int64_t>(TerrainIdTypeEnum::NONE);

  MatterContainer destMatter =
      voxelGrid.terrainGridRepository->getTerrainMatterContainer(destX, destY,
                                                                 destZ);

  // Vapor-only destination guard. The handler `onCondenseWaterEntityEvent`
  // routes here only when its event snapshot says the cell below is NONE,
  // but events sit on the dispatcher queue between enqueue and dispatch,
  // and other handlers (cascading vapor merges, condensation events from
  // adjacent vapor columns) can populate that cell with vapor in the
  // meantime. Writing liquid water on top of pure vapor would create a
  // both-WaterMatter-and-WaterVapor cell, which the per-tick water
  // invariant check forbids. Re-dispatch the same condensation event with
  // an incremented retry counter so the destination's vapor has a chance
  // to disperse on the next tick. After
  // `WATER_VAPOR_CONFLICT_RETRY_LIMIT` retries, abort with a warn log so
  // the dispatcher queue does not grow unbounded on a sealed configuration.
  //
  // Pure abort (skip without re-dispatch) is a valid fallback if retries
  // ever prove buggy, but it would silently lose the condensation amount;
  // the warn log is the only diagnostic we get for sealed-pocket
  // recurrences in the live game.
  bool destinationIsVaporOnly = !destinationIsEmpty &&
                                destMatter.WaterVapor > 0 &&
                                destMatter.WaterMatter <= 0;
  if (destinationIsVaporOnly) {
    if (retryCount < WATER_VAPOR_CONFLICT_RETRY_LIMIT) {
      Position retrySource{vaporX, vaporY, vaporZ, DirectionEnum::DOWN};
      dispatcher.enqueue<CondenseWaterEntityEvent>(CondenseWaterEntityEvent{
          retrySource, static_cast<int>(condensationAmount),
          static_cast<int>(TerrainIdTypeEnum::NONE), retryCount + 1});
      return;
    }
    spdlog::get("console")->warn(
        "[createWaterTerrainBelowVapor] Condensation gave up after {} "
        "retries at ({}, {}, {}); destination cell holds only vapor and no "
        "horizontal escape is available — vapor stack may be sealed. "
        "Source vapor at ({}, {}, {}) keeps its matter.",
        WATER_VAPOR_CONFLICT_RETRY_LIMIT, destX, destY, destZ, vaporX, vaporY,
        vaporZ);
    return;
  }

  if (destinationIsEmpty) {
    Position newPosition = {destX, destY, destZ, DirectionEnum::DOWN};

    EntityTypeComponent newType = {};
    newType.mainType = static_cast<int>(EntityEnum::TERRAIN);
    newType.subType0 = static_cast<int>(TerrainEnum::WATER);
    newType.subType1 = 0;

    PhysicsStats newPhysicsStats = {};
    newPhysicsStats.mass = 20;
    newPhysicsStats.maxSpeed = 10;
    newPhysicsStats.minSpeed = 0.0;

    StructuralIntegrityComponent newStructuralIntegrityComponent = {};
    newStructuralIntegrityComponent.canStackEntities = false;
    newStructuralIntegrityComponent.maxLoadCapacity = -1;
    newStructuralIntegrityComponent.matterState = MatterState::LIQUID;

    voxelGrid.terrainGridRepository->setPosition(destX, destY, destZ,
                                                 newPosition);
    voxelGrid.terrainGridRepository->setTerrainEntityType(destX, destY, destZ,
                                                          newType);
    voxelGrid.terrainGridRepository->setTerrainStructuralIntegrity(
        destX, destY, destZ, newStructuralIntegrityComponent);
    voxelGrid.terrainGridRepository->setPhysicsStats(destX, destY, destZ,
                                                     newPhysicsStats);

    voxelGrid.terrainGridRepository->setTerrainId(
        destX, destY, destZ,
        static_cast<int>(TerrainIdTypeEnum::ON_GRID_STORAGE));
  }

  destMatter.WaterMatter += condensationAmount;
  _logIfViolatingMatterWrite("createWaterTerrainBelowVapor:dest", destX, destY,
                             destZ, destMatter);
  voxelGrid.terrainGridRepository->setTerrainMatterContainer(destX, destY,
                                                             destZ, destMatter);

  // Seed an initial downward gravity velocity so the newly-condensed
  // water voxel enters the VDB velocity grid and keeps falling on
  // subsequent ticks — condensation can land water above an empty
  // column when the vapor was floating. See `createWaterTerrainFromFall`
  // for the same rationale spelled out in full.
  if (destZ > 0 && voxelGrid.getTerrain(destX, destY, destZ - 1) ==
                       static_cast<int64_t>(TerrainIdTypeEnum::NONE)) {
    float gravityKick = PhysicsManager::Instance()->getGravity();
    voxelGrid.terrainGridRepository->setVelocity(
        destX, destY, destZ, Velocity{0.0f, 0.0f, -gravityKick});
  }

  // Reduce vapor amount
  vaporMatter.WaterVapor -= condensationAmount;
  _logIfViolatingMatterWrite("createWaterTerrainBelowVapor:vaporSource", vaporX,
                             vaporY, vaporZ, vaporMatter);
  voxelGrid.terrainGridRepository->setTerrainMatterContainer(
      vaporX, vaporY, vaporZ, vaporMatter);

  // Cleanup vapor entity if depleted
  if (vaporMatter.WaterVapor <= 0) {
    int vaporTerrainId = voxelGrid.getTerrain(vaporX, vaporY, vaporZ);
    if (vaporTerrainId != static_cast<int>(TerrainIdTypeEnum::NONE)) {
      voxelGrid.setTerrain(vaporX, vaporY, vaporZ,
                           static_cast<int>(TerrainIdTypeEnum::NONE));
      // We're already holding TerrainGridLock for this operation, avoid
      // double-locking
      voxelGrid.terrainGridRepository->softDeactivateEntity(
          dispatcher, static_cast<entt::entity>(vaporTerrainId), false);
    }
  }

  (void)registry; // no longer creates an EnTT entity for the new water voxel

  // spdlog::get("console")->info(
  //     "[createWaterTerrainBelowVapor] Created water terrain below vapor at ("
  //     + std::to_string(vaporX) + ", " + std::to_string(vaporY) + ", " +
  //     std::to_string(vaporZ - 1) +
  //     ") with amount: " + std::to_string(condensationAmount));
}

// =========================================================================
// ================ 5. Event-based Mutators = ================
// =========================================================================

inline void _handleInvalidTerrainFound(entt::dispatcher &dispatcher,
                                       VoxelGrid &voxelGrid,
                                       const InvalidTerrainFoundEvent &event) {
  voxelGrid.deleteTerrain(dispatcher, event.x, event.y, event.z);
}

// Centralised stale-terrain-cell recovery.
//
// An event handler may receive an event that carries a now-invalid EnTT
// entity handle (typical when the event was queued before the entity was
// destroyed — e.g., via version-overflow recycling). When the cell at
// the event's coord is in a transitory "EMPTY water" state, we want to
// clean up the dead entity reference and leave the cell as ``NONE`` so
// downstream coord-based code paths can re-populate it cleanly. Cells
// that already hold real water/vapor matter are *not* touched — losing
// matter is worse than carrying a dead handle until the next pass.
//
// Returns ``true`` when the cell was recovered (caller can treat it as
// freshly NONE), ``false`` when the cell holds real matter and the
// caller should leave it alone.
inline bool _recoverStaleTerrainCellIfTransitory(VoxelGrid &voxelGrid,
                                                 entt::dispatcher &dispatcher,
                                                 int x, int y, int z) {
  EntityTypeComponent type =
      voxelGrid.terrainGridRepository->getTerrainEntityType(x, y, z);
  const bool isTransitoryEmpty =
      type.mainType == static_cast<int>(EntityEnum::TERRAIN) &&
      type.subType0 == static_cast<int>(TerrainEnum::EMPTY);
  if (!isTransitoryEmpty) {
    return false;
  }
  voxelGrid.deleteTerrain(dispatcher, x, y, z, true);
  return true;
}

// Gravity wake-up: when physics-layer code drains a cell (via `moveTerrain`
// or `deleteTerrain`), the cell directly above may be settled water with
// zero velocity in the VDB velocity grid. Without a nudge it stays stuck,
// because the velocity-driven physics pass only iterates voxels that
// already carry non-zero velocity. Kick the cell-above with a small
// downward velocity so the next tick picks it up and the column collapses
// cleanly.
//
// This is a free helper (not on a class) so the move-trigger mutator
// below can call it without pulling `PhysicsEngine` into the call site.
inline void _nudgeSettledWaterAfterDrain(VoxelGrid &voxelGrid, int drainedX,
                                         int drainedY, int drainedZ) {
  const int ax = drainedX;
  const int ay = drainedY;
  const int az = drainedZ + 1;

  const int aboveId = voxelGrid.getTerrain(ax, ay, az);
  if (aboveId == static_cast<int>(TerrainIdTypeEnum::NONE)) {
    return;
  }

  const MatterState aboveMs =
      voxelGrid.terrainGridRepository->getMatterState(ax, ay, az);
  if (aboveMs != MatterState::LIQUID) {
    return;
  }

  const MatterContainer aboveMatter =
      voxelGrid.terrainGridRepository->getTerrainMatterContainer(ax, ay, az);
  if (aboveMatter.WaterMatter <= 0) {
    return;
  }

  const Velocity aboveVel =
      voxelGrid.terrainGridRepository->getVelocity(ax, ay, az);
  if (aboveVel.vx != 0.0f || aboveVel.vy != 0.0f || aboveVel.vz != 0.0f) {
    return; // already moving — the velocity-driven pass will handle it
  }

  const float gravityKick = PhysicsManager::Instance()->getGravity();
  voxelGrid.terrainGridRepository->setVelocity(
      ax, ay, az, Velocity{0.0f, 0.0f, -gravityKick});
}

// Centralised mutator for the move-trigger sub-block of
// `PhysicsEngine::processVelocityForVoxel`. Wrapping the
// `setMovingComponent` + `moveTerrain` + nudge sequence in a single
// helper keeps every state-mutating physics-loop write inside
// `PhysicsMutators`, makes the locking explicit, and adds an invariant
// guard that `moveTerrain` itself does not enforce: a velocity-driven
// move must not relocate liquid water into a vapor cell, nor vapor into
// a liquid-water cell, because `moveTerrain` overwrites the destination
// with the source's `MatterContainer` as-is. If a phase mismatch is
// detected (or if the source itself is already in violation with both
// `WaterMatter > 0` and `WaterVapor > 0`), the move is skipped and the
// source's velocity is cleared so the source does not retry on the next
// tick.
//
// Returns ``true`` when the move executed, ``false`` when it was
// refused. Callers that observed `!hasCollision` from
// `ReadonlyQueries::hasCollision` should *not* assume the move always
// runs — this guard is more conservative than `hasCollision` (which
// today returns true whenever the destination has any terrain and is
// therefore expected to already block phase mismatches; this guard is
// the defence-in-depth in case a race makes the destination populated
// between the collision check and the actual move).
inline bool _attemptVelocityDrivenMove(VoxelGrid &voxelGrid,
                                       const Position &sourcePos,
                                       const Velocity &vel, int toX, int toY,
                                       int toZ, float completionTime,
                                       bool willStopX, bool willStopY,
                                       bool willStopZ) {
  TerrainGridLock lock(voxelGrid.terrainGridRepository.get());

  const MatterContainer sourceMatter =
      voxelGrid.terrainGridRepository->getTerrainMatterContainer(
          sourcePos.x, sourcePos.y, sourcePos.z);

  // Defence 1: source itself is in invariant violation. We refuse to
  // propagate it via `moveTerrain` (which would overwrite a clean dest
  // with the violated matter). Clear velocity so the source stops
  // re-attempting; it will be caught by the per-tick water-invariant
  // check on a later iteration when it is read for processing.
  const bool sourceIsLiquid = sourceMatter.WaterMatter > 0;
  const bool sourceIsVapor = sourceMatter.WaterVapor > 0;
  if (sourceIsLiquid && sourceIsVapor) {
    spdlog::get("console")->warn(
        "[_attemptVelocityDrivenMove] Source at ({}, {}, {}) already "
        "violates the WaterMatter/WaterVapor invariant — refusing move "
        "to ({}, {}, {}). Clearing source velocity.",
        sourcePos.x, sourcePos.y, sourcePos.z, toX, toY, toZ);
    voxelGrid.terrainGridRepository->setVelocity(
        sourcePos.x, sourcePos.y, sourcePos.z, Velocity{0.0f, 0.0f, 0.0f});
    return false;
  }

  // Defence 2: phase mismatch with destination. `moveTerrain` will only
  // proceed when the destination is NONE, but a race against another
  // worker thread could populate it between this thread's
  // `hasCollision` check and the move. Read the destination state under
  // our own lock and refuse if the move would overwrite a different
  // phase.
  const int destTerrainId = voxelGrid.getTerrain(toX, toY, toZ);
  const bool destinationIsEmpty =
      destTerrainId == static_cast<int>(TerrainIdTypeEnum::NONE);
  if (!destinationIsEmpty) {
    const MatterContainer destMatter =
        voxelGrid.terrainGridRepository->getTerrainMatterContainer(toX, toY,
                                                                   toZ);
    const bool destIsLiquid = destMatter.WaterMatter > 0;
    const bool destIsVapor = destMatter.WaterVapor > 0;
    const bool phaseMismatch =
        (sourceIsLiquid && destIsVapor) || (sourceIsVapor && destIsLiquid);

    if (phaseMismatch) {
      spdlog::get("console")->info(
          "[_attemptVelocityDrivenMove] Phase mismatch: source ({}, {}, {}) "
          "isLiquid={} isVapor={} → dest ({}, {}, {}) isLiquid={} isVapor={}. "
          "Skipping move and clearing source velocity.",
          sourcePos.x, sourcePos.y, sourcePos.z, sourceIsLiquid, sourceIsVapor,
          toX, toY, toZ, destIsLiquid, destIsVapor);
      voxelGrid.terrainGridRepository->setVelocity(
          sourcePos.x, sourcePos.y, sourcePos.z, Velocity{0.0f, 0.0f, 0.0f});
      return false;
    }

    // Same-phase or both-empty (no matter on either side): also skip,
    // because `moveTerrain` itself refuses to move into a populated cell
    // and clearing velocity here keeps the source from retrying every
    // tick on a permanently blocked path.
    spdlog::get("console")->debug(
        "[_attemptVelocityDrivenMove] Destination ({}, {}, {}) already "
        "populated (same-phase). Skipping move and clearing source velocity.",
        toX, toY, toZ);
    voxelGrid.terrainGridRepository->setVelocity(
        sourcePos.x, sourcePos.y, sourcePos.z, Velocity{0.0f, 0.0f, 0.0f});
    return false;
  }

  // Safe to move: empty destination, source is in a single phase.
  MovingComponent mc =
      initializeMovingComponent(sourcePos, vel, toX, toY, toZ, completionTime,
                                willStopX, willStopY, willStopZ);
  voxelGrid.terrainGridRepository->setMovingComponent(sourcePos.x, sourcePos.y,
                                                      sourcePos.z, mc);
  voxelGrid.terrainGridRepository->moveTerrain(mc);
  // (sourcePos.x, sourcePos.y, sourcePos.z) was just drained; wake any
  // settled water in the cell directly above so a column collapses on
  // the next tick.
  _nudgeSettledWaterAfterDrain(voxelGrid, sourcePos.x, sourcePos.y,
                               sourcePos.z);
  return true;
}

// Centralised mutator for `WaterCreationEvent`. Materialises liquid water
// at a coordinate from a source that does not drain another cell —
// `SpringWaterSystem`, scripted weather, future rain. Implements the four
// architectural goals stated in the Phase 2 audit:
//   - Rule 2 (mutator-only writes): the only direct repository writes for
//     this code path live here.
//   - Rule 3 (validate own preconditions): reads cell state under the
//     unique lock and branches on what it actually finds.
//   - Rule 4 (never accept inconsistency):
//       * NONE destination     -> write fresh water terrain scaffolding.
//       * Liquid water on a    -> additive merge (matches the
//         WATER- or GRASS-         evaporation branch in
//         typed cell                `processTileWater`, which treats
//                                   grass + WaterMatter as a valid
//                                   saturated-soil state).
//       * Vapor-only dest      -> re-enqueue with retryCount+1 up to
//                                  WATER_VAPOR_CONFLICT_RETRY_LIMIT, then
//                                  warn-log and drop.
//       * Other terrain types  -> refuse + warn (water cannot land on
//                                  stone, transitory-empty cells, plant
//                                  terrain, etc.).
inline void _handleWaterCreationEvent(entt::registry &registry,
                                      entt::dispatcher &dispatcher,
                                      VoxelGrid &voxelGrid,
                                      const WaterCreationEvent &event) {
  (void)registry; // No EnTT entity is created — water lives in VDB only.
  if (!voxelGrid.terrainGridRepository) {
    spdlog::get("console")->warn(
        "[_handleWaterCreationEvent] missing terrainGridRepository");
    return;
  }
  TerrainGridLock lock(voxelGrid.terrainGridRepository.get());

  const int x = event.position.x;
  const int y = event.position.y;
  const int z = event.position.z;

  const int64_t terrainId = voxelGrid.getTerrain(x, y, z);
  const bool destinationIsEmpty =
      terrainId == static_cast<int64_t>(TerrainIdTypeEnum::NONE);

  EntityTypeComponent destType{};
  MatterContainer destMatter{};
  if (!destinationIsEmpty) {
    destType = voxelGrid.terrainGridRepository->getTerrainEntityType(x, y, z);
    destMatter =
        voxelGrid.terrainGridRepository->getTerrainMatterContainer(x, y, z);
  }

  // Refusal path: cell exists but does not hold liquid water in any
  // currently-modelled form. Accept WATER-typed terrain (the obvious
  // case) and GRASS-typed terrain (water saturating soil — the engine's
  // evaporation branch in `processTileWater` already treats
  // grass + WaterMatter as a valid steady state, so additive merges into
  // grass are physically meaningful and consistent with the rest of the
  // simulation). Stone, transitory empty, plant terrain, etc. still
  // refuse: water cannot land there.
  const bool isWaterCarryingTerrain =
      destType.mainType == static_cast<int>(EntityEnum::TERRAIN) &&
      (destType.subType0 == static_cast<int>(TerrainEnum::WATER) ||
       destType.subType0 == static_cast<int>(TerrainEnum::GRASS));
  if (!destinationIsEmpty && !isWaterCarryingTerrain) {
    spdlog::get("console")->warn(
        "[_handleWaterCreationEvent] Cell at ({}, {}, {}) is neither WATER "
        "nor GRASS (mainType={}, subType0={}); skipping water creation "
        "amount={}.",
        x, y, z, destType.mainType, destType.subType0, event.amount);
    return;
  }

  // Refusal path with retry: cell holds pure vapor. Re-enqueue with
  // retryCount + 1 so the surrounding vapor has a tick to disperse, abort
  // with a warning once `WATER_VAPOR_CONFLICT_RETRY_LIMIT` is reached.
  const bool destinationIsVaporOnly = !destinationIsEmpty &&
                                      destMatter.WaterVapor > 0 &&
                                      destMatter.WaterMatter <= 0;
  if (destinationIsVaporOnly) {
    if (event.retryCount < WATER_VAPOR_CONFLICT_RETRY_LIMIT) {
      dispatcher.enqueue<WaterCreationEvent>(WaterCreationEvent{
          event.position, event.amount, event.retryCount + 1});
      return;
    }
    spdlog::get("console")->warn(
        "[_handleWaterCreationEvent] Vapor at ({}, {}, {}) did not clear "
        "after {} retries; aborting water creation amount={}.",
        x, y, z, WATER_VAPOR_CONFLICT_RETRY_LIMIT, event.amount);
    return;
  }

  // Path A: empty destination — write the full water-terrain scaffolding
  // and seed an initial gravity velocity if the cell below is empty so the
  // newly-spawned water enters the velocity-driven physics pass.
  if (destinationIsEmpty) {
    Position newPosition = {x, y, z, DirectionEnum::DOWN};

    EntityTypeComponent newType = {};
    newType.mainType = static_cast<int>(EntityEnum::TERRAIN);
    newType.subType0 = static_cast<int>(TerrainEnum::WATER);
    newType.subType1 = 0;

    PhysicsStats newPhysicsStats = {};
    newPhysicsStats.mass = 20;
    newPhysicsStats.maxSpeed = 10;
    newPhysicsStats.minSpeed = 0.0;

    StructuralIntegrityComponent newSIC = {};
    newSIC.canStackEntities = false;
    newSIC.maxLoadCapacity = -1;
    newSIC.matterState = MatterState::LIQUID;

    voxelGrid.terrainGridRepository->setPosition(x, y, z, newPosition);
    voxelGrid.terrainGridRepository->setTerrainEntityType(x, y, z, newType);
    voxelGrid.terrainGridRepository->setTerrainStructuralIntegrity(x, y, z,
                                                                   newSIC);
    voxelGrid.terrainGridRepository->setPhysicsStats(x, y, z, newPhysicsStats);
    voxelGrid.terrainGridRepository->setTerrainId(
        x, y, z, static_cast<int>(TerrainIdTypeEnum::ON_GRID_STORAGE));

    MatterContainer freshMatter{};
    freshMatter.WaterMatter = event.amount;
    _logIfViolatingMatterWrite("_handleWaterCreationEvent:freshCell", x, y, z,
                               freshMatter);
    voxelGrid.terrainGridRepository->setTerrainMatterContainer(x, y, z,
                                                               freshMatter);

    if (z > 0 && voxelGrid.getTerrain(x, y, z - 1) ==
                     static_cast<int64_t>(TerrainIdTypeEnum::NONE)) {
      const float gravityKick = PhysicsManager::Instance()->getGravity();
      voxelGrid.terrainGridRepository->setVelocity(
          x, y, z, Velocity{0.0f, 0.0f, -gravityKick});
    }
    return;
  }

  // Path B: existing liquid-water cell — additive matter merge.
  destMatter.WaterMatter += event.amount;
  _logIfViolatingMatterWrite("_handleWaterCreationEvent:additiveMerge", x, y, z,
                             destMatter);
  voxelGrid.terrainGridRepository->setTerrainMatterContainer(x, y, z,
                                                             destMatter);
}

// Definition: src/physics/mutators/WaterPhysicsMutators.cpp
void _handleWaterSpreadEvent(VoxelGrid &voxelGrid,
                             const WaterSpreadEvent &event);

inline void _handleWaterGravityFlowEvent(entt::registry &registry,
                                         entt::dispatcher &dispatcher,
                                         VoxelGrid &voxelGrid,
                                         const WaterGravityFlowEvent &event) {
  TerrainGridLock lock(voxelGrid.terrainGridRepository.get());

  const int sourceTerrainId =
      voxelGrid.getTerrain(event.source.x, event.source.y, event.source.z);
  const int targetTerrainId =
      voxelGrid.checkIfTerrainExists(event.target.x, event.target.y,
                                     event.target.z)
          ? voxelGrid.getTerrain(event.target.x, event.target.y, event.target.z)
          : static_cast<int>(TerrainIdTypeEnum::NONE);

  // Re-read current repository state to avoid TOCTOU races
  MatterContainer currentSource =
      voxelGrid.terrainGridRepository->getTerrainMatterContainer(
          event.source.x, event.source.y, event.source.z);
  MatterContainer currentTarget = {};
  if (targetTerrainId != static_cast<int>(TerrainIdTypeEnum::NONE)) {
    currentTarget = voxelGrid.terrainGridRepository->getTerrainMatterContainer(
        event.target.x, event.target.y, event.target.z);
  }

  // Validate that the downward flow is still possible.
  if (currentSource.WaterMatter < event.amount) {
    spdlog::get("console")->warn("[_handleWaterGravityFlowEvent] Source no "
                                 "longer has required amount of water.");
    return; // Source no longer has required amount
  }
  // Prevent flowing water into a tile that currently has vapor
  if (currentTarget.WaterVapor > 0) {
    spdlog::get("console")->warn("[_handleWaterGravityFlowEvent] Target "
                                 "currently has vapor; aborting flow.");
    return; // Target has vapor; skip flow
  }

  PhysicsStats sourcePhysicsStats =
      voxelGrid.terrainGridRepository->getPhysicsStats(
          event.source.x, event.source.y, event.source.z);

  EntityTypeComponent targetType = voxelGrid.getTerrainEntityTypeComponent(
      event.target.x, event.target.y, event.target.z);

  bool isTargetEmptyTerrain =
      (targetType.mainType == static_cast<int>(EntityEnum::TERRAIN) &&
       targetType.subType0 == static_cast<int>(TerrainEnum::EMPTY));

  if (targetTerrainId == static_cast<int>(TerrainIdTypeEnum::NONE) ||
      (isTargetEmptyTerrain &&
       targetTerrainId ==
           static_cast<int>(TerrainIdTypeEnum::ON_GRID_STORAGE))) {
    createWaterTerrainFromGravityFlow(voxelGrid, event.target, targetTerrainId,
                                      sourcePhysicsStats);
  }

  // Apply transfer using up-to-date state
  currentTarget.WaterMatter += event.amount;
  currentSource.WaterMatter -= event.amount;

  // Update both voxels atomically
  _logIfViolatingMatterWrite("_handleWaterGravityFlowEvent:target",
                             event.target.x, event.target.y, event.target.z,
                             currentTarget);
  voxelGrid.terrainGridRepository->setTerrainMatterContainer(
      event.target.x, event.target.y, event.target.z, currentTarget);
  _logIfViolatingMatterWrite("_handleWaterGravityFlowEvent:source",
                             event.source.x, event.source.y, event.source.z,
                             currentSource);
  voxelGrid.terrainGridRepository->setTerrainMatterContainer(
      event.source.x, event.source.y, event.source.z, currentSource);

  EntityTypeComponent sourceType = voxelGrid.getTerrainEntityTypeComponent(
      event.source.x, event.source.y, event.source.z);

  const bool isEmptyTerrain =
      (sourceType.mainType == static_cast<int>(EntityEnum::TERRAIN) &&
       sourceType.subType0 == static_cast<int>(TerrainEnum::EMPTY));

  const bool isWater =
      (sourceType.mainType == static_cast<int>(EntityEnum::TERRAIN) &&
       sourceType.subType0 == static_cast<int>(TerrainEnum::WATER));

  bool emptyWater = isWater && (currentSource.WaterMatter == 0 &&
                                currentSource.WaterVapor == 0);

  if (emptyWater) {
    // Convert to empty terrain if we just removed the last of the water.
    // Source always needs deletion now that the function no longer reuses an
    // ECS source entity at the destination.

    // spdlog::get("console")->warn("[_handleWaterGravityFlowEvent] Empty
    // terrain "
    //                              "at ({}, {}, {}), deleting it.",
    //                              event.source.x, event.source.y,
    //                              event.source.z);

    voxelGrid.deleteTerrain(dispatcher, event.source.x, event.source.y,
                            event.source.z, false);
  }
}

// Definition: src/physics/mutators/WaterPhysicsMutators.cpp
void _handleTerrainPhaseConversionEvent(
    VoxelGrid &voxelGrid, const TerrainPhaseConversionEvent &event);

inline void _handleVaporMergeSidewaysEvent(
    entt::registry &registry, entt::dispatcher &dispatcher,
    VoxelGrid &voxelGrid, const VaporMergeSidewaysEvent &event) {
  // Lock terrain grid for atomic state change (prevents race conditions with
  // other systems)
  TerrainGridLock lock(voxelGrid.terrainGridRepository.get());

  // Get target vapor and merge
  MatterContainer targetMatter =
      voxelGrid.terrainGridRepository->getTerrainMatterContainer(
          event.target.x, event.target.y, event.target.z);

  MatterContainer sourceMatter =
      voxelGrid.terrainGridRepository->getTerrainMatterContainer(
          event.source.x, event.source.y, event.source.z);
  int amountToMerge;
  if (event.amount == sourceMatter.WaterVapor) {
    amountToMerge = event.amount;
  } else {
    amountToMerge = sourceMatter.WaterVapor;
  }

  // Enforce that WaterMatter and WaterVapor cannot coexist
  // If target has liquid water, condense the vapor into it (quantum
  // condensation)
  if (targetMatter.WaterMatter > 0 && targetMatter.WaterVapor == 0) {
    targetMatter.WaterMatter += amountToMerge; // Vapor condenses into liquid
  } else if (targetMatter.WaterVapor > 0 && targetMatter.WaterMatter == 0) {
    targetMatter.WaterVapor += amountToMerge; // Add to vapor
  } else if (targetMatter.WaterMatter == 0 && targetMatter.WaterVapor == 0) {
    targetMatter.WaterVapor += amountToMerge; // Empty tile, add vapor
  } else {
    spdlog::get("console")->error(
        "[VaporMergeSidewaysEvent] State invariant violation at target ({}, "
        "{}, {}): "
        "Both WaterMatter ({}) and WaterVapor ({}) are non-zero.",
        event.target.x, event.target.y, event.target.z,
        targetMatter.WaterMatter, targetMatter.WaterVapor);
    throw aetherion::InvalidTerrainStateException(
        "VaporMergeSidewaysEvent: State invariant violation - both WaterMatter "
        "and WaterVapor "
        "non-zero.");
  }

  _logIfViolatingMatterWrite("_handleVaporMergeSidewaysEvent:target",
                             event.target.x, event.target.y, event.target.z,
                             targetMatter);
  voxelGrid.terrainGridRepository->setTerrainMatterContainer(
      event.target.x, event.target.y, event.target.z, targetMatter);

  // Clear source vapor
  sourceMatter.WaterVapor = 0;
  _logIfViolatingMatterWrite("_handleVaporMergeSidewaysEvent:source",
                             event.source.x, event.source.y, event.source.z,
                             sourceMatter);
  voxelGrid.terrainGridRepository->setTerrainMatterContainer(
      event.source.x, event.source.y, event.source.z, sourceMatter);

  // TODO: Clean this properly.
  // Delete or convert source entity if it's a valid entity (not ON_GRID_STORAGE
  // or NONE)
  if (event.sourceTerrainId !=
          static_cast<int>(TerrainIdTypeEnum::ON_GRID_STORAGE) &&
      event.sourceTerrainId != static_cast<int>(TerrainIdTypeEnum::NONE)) {
    entt::entity sourceEntity =
        static_cast<entt::entity>(event.sourceTerrainId);
    if (registry.valid(sourceEntity)) {
      std::ostringstream ossMessage;
      ossMessage << "[VaporMergeSidewaysEvent] Deleting source vapor entity ID="
                 << event.sourceTerrainId << " at (" << event.source.x << ", "
                 << event.source.y << ", " << event.source.z << ")";
      spdlog::get("console")->info(ossMessage.str());
      dispatcher.enqueue<KillEntityEvent>(sourceEntity);
    } else {
      // spdlog::get("console")->warn("[VaporMergeSidewaysEvent] Source terrain
      // "
      //                              "ID {} is not a valid entity; skipping "
      //                              "deletion.",
      //                              event.sourceTerrainId);
    }
  }
}

// Helper: Update position to destination
inline void
updatePositionToDestination(Position &position,
                            const MovingComponent &movingComponent) {
  position.x = movingComponent.movingToX;
  position.y = movingComponent.movingToY;
  position.z = movingComponent.movingToZ;
}

// Helper: Apply terrain movement in VoxelGrid
inline void applyTerrainMovement(VoxelGrid &voxelGrid, entt::entity entity,
                                 const MovingComponent &movingComponent) {
  // std::cout << "Setting movingTo positions for terrain moving."
  //           << "moving to: " << movingComponent.movingToX << ", " <<
  //           movingComponent.movingToY
  //           << ", " << movingComponent.movingToZ << "\n";

  validateTerrainEntityId(entity);
  voxelGrid.terrainGridRepository->moveTerrain(
      const_cast<MovingComponent &>(movingComponent));
}

// Helper: Apply regular entity movement in VoxelGrid
inline void applyEntityMovement(VoxelGrid &voxelGrid, entt::entity entity,
                                const MovingComponent &movingComponent) {
  Position movingToPosition;
  movingToPosition.x = movingComponent.movingToX;
  movingToPosition.y = movingComponent.movingToY;
  movingToPosition.z = movingComponent.movingToZ;
  voxelGrid.moveEntity(entity, movingToPosition);
}

// Main function: Create and apply movement component
inline void createMovingComponent(entt::registry &registry,
                                  entt::dispatcher &dispatcher,
                                  VoxelGrid &voxelGrid, entt::entity entity,
                                  Position &position, Velocity &velocity,
                                  int movingToX, int movingToY, int movingToZ,
                                  float completionTime, bool willStopX,
                                  bool willStopY, bool willStopZ,
                                  bool isTerrain) {
  MovingComponent movingComponent = initializeMovingComponent(
      position, velocity, movingToX, movingToY, movingToZ, completionTime,
      willStopX, willStopY, willStopZ);

  registry.emplace<MovingComponent>(entity, movingComponent);

  EntityTypeComponent entityType =
      getEntityType(registry, voxelGrid, entity, position, isTerrain);

  bool isTerrainType =
      (entityType.mainType == static_cast<int>(EntityEnum::TERRAIN)) ||
      isTerrain;

  if (isTerrainType) {
    applyTerrainMovement(voxelGrid, entity, movingComponent);
  } else {
    applyEntityMovement(voxelGrid, entity, movingComponent);
  }

  updatePositionToDestination(position, movingComponent);
}

#endif // PHYSICS_MUTATORS_HPP
