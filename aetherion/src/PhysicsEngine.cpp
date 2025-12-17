#include "PhysicsEngine.hpp"

#include <algorithm>
#include <iostream>
#include <random>
#include <sstream>

#include "EcosystemEngine.hpp"
#include "physics/Collision.hpp"
#include "physics/PhysicalMath.hpp"
#include "physics/PhysicsExceptions.hpp"
#include "physics/PhysicsMutators.hpp"
#include "physics/PhysicsUtils.hpp"
#include "physics/PhysicsValidators.hpp"
#include "physics/ReadonlyQueries.hpp"
#include "settings.hpp"
#include "terrain/TerrainGridLock.hpp" // For TerrainGridLock
#include "ecosystem/EcosystemEvents.hpp"
#include <memory>                      // For std::unique_ptr

// =========================================================================
// ================ PHYSICS ENGINE ORGANIZATION ================
// =========================================================================
//
// This file is organized into the following logical sections:
//
// 1. READ-ONLY QUERY FUNCTIONS
//    - Pure queries that validate state without modifications
//    - Collision detection, stability checks, terrain queries
//
// 2. PHYSICS CALCULATION FUNCTIONS
//    - Pure calculations that compute new values without state changes
//    - Velocity, friction, gravity calculations
//
// 3. COMPONENT INITIALIZATION FUNCTIONS
//    - Create and initialize component data structures
//    - Load entity physics data from ECS or terrain storage
//
// 4. ENTITY MOVEMENT STATE CHANGERS
//    - Modify position/velocity in VoxelGrid and ECS
//    - Apply movement, update positions
//
// 5. ENTITY CREATION/DELETION FUNCTIONS
//    - Create new entities or destroy existing ones
//    - Terrain type conversions
//
// 6. TERRAIN PHASE CONVERSION FUNCTIONS
//    - Transform terrain between different matter states
//    - Water/vapor conversions, soft-empty handling
//
// 7. PHYSICS ENGINE MAIN LOOP FUNCTIONS
//    - Orchestrate state changes across all entities
//    - Main physics processing loops
//
// 8. EVENT HANDLERS
//    - React to events and modify state atomically
//    - Movement, water phase, vapor, item events
//
// All functions that modify terrain state use atomic operations with
// lockTerrainGrid()/unlockTerrainGrid() for thread safety.
// =========================================================================

bool PhysicsEngine::checkIfCanJump(const MoveSolidEntityEvent& event) {
    // Implement the logic to determine if the entity can jump
    // Placeholder implementation:
    return true;
}

// Helper: Apply gravity and get new Z velocity
inline std::pair<float, bool> resolveVerticalMotion(entt::registry& registry, VoxelGrid& voxelGrid,
                                                    const Position& position, float velocityZ,
                                                    MatterState matterState,
                                                    entt::entity entityBeingDebugged,
                                                    entt::entity entity) {
    if (matterState == MatterState::SOLID || matterState == MatterState::LIQUID) {
        if (entity == entityBeingDebugged) {
            std::cout << "handleMovement -> applying Gravity" << std::endl;
        }
        auto resultZ = calculateVelocityAfterGravityStep(registry, voxelGrid, position.x,
                                                         position.y, position.z, velocityZ, 1);
        float newVelocityZ = resultZ.first;
        resultZ = calculateVelocityAfterGravityStep(registry, voxelGrid, position.x, position.y,
                                                    position.z, velocityZ, 2);
        bool willStopZ = resultZ.second;
        return {newVelocityZ, willStopZ};
    }
    return {velocityZ, false};
}


// New event handlers for water physics (all state changes)
void PhysicsEngine::onWaterSpreadEvent(const WaterSpreadEvent& event) {
    _handleWaterSpreadEvent(*voxelGrid, event);
}

void PhysicsEngine::onWaterGravityFlowEvent(const WaterGravityFlowEvent& event) {
    _handleWaterGravityFlowEvent(*voxelGrid, event);
}

void PhysicsEngine::onTerrainPhaseConversionEvent(const TerrainPhaseConversionEvent& event) {
    _handleTerrainPhaseConversionEvent(*voxelGrid, event);
}

void PhysicsEngine::onVaporCreationEvent(const VaporCreationEvent& event) {
    // Reuse existing helper function which already has proper locking
    TerrainGridLock lock(voxelGrid->terrainGridRepository.get());

    addOrCreateVaporAbove(registry, *voxelGrid, event.position.x, event.position.y,
                          event.position.z, event.amount);
}

void PhysicsEngine::onCreateVaporEntityEvent(const CreateVaporEntityEvent& event) {
    _handleCreateVaporEntityEvent(registry, dispatcher, *voxelGrid, event);
}

void PhysicsEngine::onDeleteOrConvertTerrainEvent(const DeleteOrConvertTerrainEvent& event) {
    // Delegate to existing helper which handles effects and soft-empty conversion
    TerrainGridLock lock(voxelGrid->terrainGridRepository.get());

    entt::entity terrain = event.terrain;
    deleteEntityOrConvertInEmpty(registry, dispatcher, const_cast<entt::entity&>(terrain));
}

void PhysicsEngine::onVaporMergeUpEvent(const VaporMergeUpEvent& event) {
    // Lock terrain grid for atomic state change
    TerrainGridLock lock(voxelGrid->terrainGridRepository.get());

    // Get target vapor and merge
    MatterContainer targetMatter = voxelGrid->terrainGridRepository->getTerrainMatterContainer(
        event.target.x, event.target.y, event.target.z);
    targetMatter.WaterVapor += event.amount;
    voxelGrid->terrainGridRepository->setTerrainMatterContainer(event.target.x, event.target.y,
                                                                event.target.z, targetMatter);

    // Clear source vapor
    MatterContainer sourceMatter = voxelGrid->terrainGridRepository->getTerrainMatterContainer(
        event.source.x, event.source.y, event.source.z);
    sourceMatter.WaterVapor = 0;
    voxelGrid->terrainGridRepository->setTerrainMatterContainer(event.source.x, event.source.y,
                                                                event.source.z, sourceMatter);

    // Delete or convert source entity while still holding lock
    // This prevents race condition where entity is deleted from tracking maps
    // while physics systems are still processing it
    if (registry.valid(event.sourceEntity)) {
        std::cout << "[VaporMergeUpEvent] Deleting source vapor entity ID="
                  << static_cast<int>(event.sourceEntity) << "\n";
        dispatcher.enqueue<KillEntityEvent>(event.sourceEntity);
    } else {
        std::ostringstream ossMessage;
        ossMessage << "[VaporMergeUpEvent] Source entity invalid for vapor merge at ("
                   << event.source.x << ", " << event.source.y << ", " << event.source.z << ")";
        spdlog::get("console")->warn(ossMessage.str());
    }
}

void PhysicsEngine::onVaporMergeSidewaysEvent(const VaporMergeSidewaysEvent& event) {
    _handleVaporMergeSidewaysEvent(registry, dispatcher, *voxelGrid, event);
}

void PhysicsEngine::onAddVaporToTileAboveEvent(const AddVaporToTileAboveEvent& event) {
    // Lock terrain grid for atomic operation
    TerrainGridLock lock(voxelGrid->terrainGridRepository.get());

    const int x = event.sourcePos.x;
    const int y = event.sourcePos.y;
    const int z = event.sourcePos.z + 1;  // Tile above
    const int terrainAboveId = event.terrainAboveId;

    // Convert soft-empty to vapor if needed
    if (getTypeAndCheckSoftEmpty(registry, *voxelGrid, terrainAboveId, x, y, z)) {
        convertSoftEmptyIntoVapor(registry, *voxelGrid, terrainAboveId, x, y, z);
    }

    // Read terrain state after potential conversion
    EntityTypeComponent typeAbove = voxelGrid->terrainGridRepository->getTerrainEntityType(x, y, z);
    MatterContainer matterContainerAbove =
        voxelGrid->terrainGridRepository->getTerrainMatterContainer(x, y, z);

    // Check if it's vapor terrain and safe to add
    if (typeAbove.mainType == static_cast<int>(EntityEnum::TERRAIN) &&
        typeAbove.subType0 == static_cast<int>(TerrainEnum::WATER) &&
        matterContainerAbove.WaterVapor >= 0 && matterContainerAbove.WaterMatter == 0) {
        // Add vapor amount
        matterContainerAbove.WaterVapor += event.amount;
        voxelGrid->terrainGridRepository->setTerrainMatterContainer(x, y, z, matterContainerAbove);

        std::ostringstream ossMessage;
        ossMessage << "[AddVaporToTileAboveEvent] Added " << event.amount << " vapor at (" << x
                   << ", " << y << ", " << z << ")";
        spdlog::get("console")->info(ossMessage.str());
    } else {
        std::ostringstream ossMessage;
        ossMessage << "[AddVaporToTileAboveEvent] Cannot add vapor above; obstruction at (" << x
                   << ", " << y << ", " << z << ")";
        spdlog::get("console")->info(ossMessage.str());
    }
}

// Register event handlers
void PhysicsEngine::registerEventHandlers(entt::dispatcher& dispatcher) {
    dispatcher.sink<MoveGasEntityEvent>().connect<&PhysicsEngine::onMoveGasEntityEvent>(*this);
    dispatcher.sink<MoveSolidEntityEvent>().connect<&PhysicsEngine::onMoveSolidEntityEvent>(*this);
    dispatcher.sink<TakeItemEvent>().connect<&PhysicsEngine::onTakeItemEvent>(*this);
    dispatcher.sink<UseItemEvent>().connect<&PhysicsEngine::onUseItemEvent>(*this);
    dispatcher.sink<SetPhysicsEntityToDebug>().connect<&PhysicsEngine::onSetPhysicsEntityToDebug>(
        *this);

    // Register water phase change event handlers
    dispatcher.sink<EvaporateWaterEntityEvent>()
        .connect<&PhysicsEngine::onEvaporateWaterEntityEvent>(*this);
    dispatcher.sink<CondenseWaterEntityEvent>().connect<&PhysicsEngine::onCondenseWaterEntityEvent>(
        *this);
    dispatcher.sink<WaterFallEntityEvent>().connect<&PhysicsEngine::onWaterFallEntityEvent>(*this);

    // Register water flow event handlers (new architecture)
    dispatcher.sink<WaterSpreadEvent>().connect<&PhysicsEngine::onWaterSpreadEvent>(*this);
    dispatcher.sink<WaterGravityFlowEvent>().connect<&PhysicsEngine::onWaterGravityFlowEvent>(
        *this);
    dispatcher.sink<TerrainPhaseConversionEvent>()
        .connect<&PhysicsEngine::onTerrainPhaseConversionEvent>(*this);

    // Register vapor event handlers
    dispatcher.sink<VaporCreationEvent>().connect<&PhysicsEngine::onVaporCreationEvent>(*this);
    dispatcher.sink<VaporMergeUpEvent>().connect<&PhysicsEngine::onVaporMergeUpEvent>(*this);
    dispatcher.sink<VaporMergeSidewaysEvent>()
        .connect<&PhysicsEngine::onVaporMergeSidewaysEvent>(*this);
    dispatcher.sink<AddVaporToTileAboveEvent>().connect<&PhysicsEngine::onAddVaporToTileAboveEvent>(
        *this);
    dispatcher.sink<CreateVaporEntityEvent>().connect<&PhysicsEngine::onCreateVaporEntityEvent>(
        *this);
    dispatcher.sink<DeleteOrConvertTerrainEvent>()
        .connect<&PhysicsEngine::onDeleteOrConvertTerrainEvent>(*this);
    dispatcher.sink<InvalidTerrainFoundEvent>()
        .connect<&PhysicsEngine::onInvalidTerrainFound>(*this);
}

void PhysicsEngine::onInvalidTerrainFound(const InvalidTerrainFoundEvent& event) {
    _handleInvalidTerrainFound(dispatcher, *voxelGrid, event);
}

// ================ END OF REFACTORING ================

// Helper: Load entity data (Position, Velocity, PhysicsStats) from either ECS or terrain storage
// ATOMIC: For terrain, uses getPhysicsSnapshot() to read all data under a single lock
inline std::tuple<Position&, Velocity&, PhysicsStats&> loadEntityPhysicsData(
    entt::registry& registry, VoxelGrid& voxelGrid, entt::entity entity, bool isTerrain,
    Position& terrainPos, Velocity& terrainVel, PhysicsStats& terrainPS) {
    if (isTerrain) {
        // std::cout << "[loadEntityPhysicsData] Loading terrain physics data for entity ID=" <<
        // int(entity) << "\n";
        if (!registry.valid(entity)) {
            throw aetherion::InvalidEntityException(
                "Invalid terrain entity in loadEntityPhysicsData");
        }

        terrainPos = voxelGrid.terrainGridRepository->getPositionOfEntt(entity);

        if (!voxelGrid.checkIfTerrainExists(terrainPos.x, terrainPos.y, terrainPos.z)) {
            // Terrain not found in repository - perform cleanup and check grid
            // std::cout << "[loadEntityPhysicsData] Terrain not found in repository at position ("
            //           << terrainPos.x << ", " << terrainPos.y << ", " << terrainPos.z << ")\n";

            // Check if terrain exists in the underlying grid (OpenVDB storage)
            // It's possible the entity was deleted from entt but still exists in the grid
            // Terrain doesn't exist in grid either - cleanup entity and throw error
            // std::cout << "[loadEntityPhysicsData] Terrain not found in grid either. Cleaning up
            // entity.\n";
            if (registry.valid(entity)) {
                _destroyEntity(registry, voxelGrid, entity, false);
            } else {
                // std::cout << "[loadEntityPhysicsData] Entity already invalid during cleanup.\n";
                throw std::runtime_error(
                    "Terrain does not exist at the given position in repository or grid");
            }

            // Terrain exists in grid but not in repository - return empty velocity objects
            // std::cout << "[loadEntityPhysicsData] Terrain found in grid but not in repository.
            // Returning empty velocity.\n";
            terrainVel = Velocity{0.f, 0.f, 0.f};
            terrainPS = voxelGrid.terrainGridRepository->getPhysicsStats(terrainPos.x, terrainPos.y,
                                                                         terrainPos.z);
            return std::tie(terrainPos, terrainVel, terrainPS);
        } else {
            // std::cout << "[loadEntityPhysicsData] Terrain found in repository at position ("
            //           << terrainPos.x << ", " << terrainPos.y << ", " << terrainPos.z << ")\n";
        }

        terrainVel =
            voxelGrid.terrainGridRepository->getVelocity(terrainPos.x, terrainPos.y, terrainPos.z);
        terrainPS = voxelGrid.terrainGridRepository->getPhysicsStats(terrainPos.x, terrainPos.y,
                                                                     terrainPos.z);
        return std::tie(terrainPos, terrainVel, terrainPS);
    } else {
        if (!registry.valid(entity)) {
            throw std::runtime_error("Entity no longer valid in loadEntityPhysicsData");
        }

        return std::tie(registry.get<Position>(entity), registry.get<Velocity>(entity),
                        registry.get<PhysicsStats>(entity));
    }
}

// Helper: Handle lateral collision and Z-axis movement
inline bool handleLateralCollision(entt::registry& registry, entt::dispatcher& dispatcher,
                                   VoxelGrid& voxelGrid, entt::entity entity, Position& position,
                                   Velocity& velocity, float newVelocityX, float newVelocityY,
                                   float newVelocityZ, float completionTime, bool willStopX,
                                   bool willStopY, bool willStopZ, bool haveMovement,
                                   bool isTerrain) {
    bool lateralCollision = false;
    if (getDirectionFromVelocity(newVelocityX) != 0) {
        lateralCollision = true;
        velocity.vx = 0;
    }
    if (getDirectionFromVelocity(newVelocityY) != 0) {
        lateralCollision = true;
        velocity.vy = 0;
    }

    if (!lateralCollision) {
        return false;
    }

    // Check Z-axis collision
    bool collisionZ = false;
    int movingToX = position.x;
    int movingToY = position.y;
    int movingToZ = position.z + getDirectionFromVelocity(newVelocityZ);

    if ((0 <= movingToX && movingToX < voxelGrid.width) &&
        (0 <= movingToY && movingToY < voxelGrid.height) &&
        (0 <= movingToZ && movingToZ < voxelGrid.depth)) {
        int movingToEntityId = voxelGrid.getEntity(movingToX, movingToY, movingToZ);
        bool movingToTerrainExists =
            voxelGrid.checkIfTerrainExists(movingToX, movingToY, movingToZ);

        if (movingToEntityId != -1 || movingToTerrainExists) {
            collisionZ = true;
        }
    } else {
        collisionZ = true;
    }

    if (!collisionZ && !haveMovement) {
        velocity.vz = newVelocityZ;
        createMovingComponent(registry, dispatcher, voxelGrid, entity, position, velocity,
                              movingToX, movingToY, movingToZ, completionTime, willStopX, willStopY,
                              willStopZ, isTerrain);
        return true;
    } else {
        velocity.vz = 0;
        return false;
    }
}

// Main function: Handle entity movement with physics
void handleMovement(entt::registry& registry, entt::dispatcher& dispatcher, VoxelGrid& voxelGrid,
                    entt::entity entity, entt::entity entityBeingDebugged, bool isTerrain) {
    // if (isTerrain) {
    //     std::cout << "[handleMovement] Handling terrain entity ID=" << int(entity) << "\n";
    // }

    // SAFETY CHECK 1: Validate entity is still valid
    if (!registry.valid(entity)) {
        try {
            entity = handleInvalidEntityForMovement(registry, voxelGrid, dispatcher, entity);
        } catch (const aetherion::InvalidEntityException& e) {
            // Exception indicates we should stop processing this entity.
            // The mutator function already logged the details.
            return;
        }
    }

    // SAFETY CHECK 2: For terrain entities, verify they have Position component
    // This ensures vapor entities are fully initialized before physics processes them
    ensurePositionComponentForTerrain(registry, voxelGrid, entity, isTerrain);

    bool haveMovement = registry.all_of<MovingComponent>(entity);

    // ⚠️ CRITICAL FIX: Acquire terrain grid lock BEFORE reading any terrain data
    // to prevent TOCTOU race conditions where terrain moves between position lookup
    // and velocity/physics reads (see loadEntityPhysicsData lines 507-515)
    // IMPORTANT: Use RAII pattern to ensure lock is ALWAYS released
    std::unique_ptr<TerrainGridLock> terrainLockGuard;
    if (isTerrain) {
        terrainLockGuard = std::make_unique<TerrainGridLock>(voxelGrid.terrainGridRepository.get());
    }

    // Exception-safe lock release using try-catch with specific exception handlers
    try {
        // Load entity physics data from ECS or terrain storage
        Position terrainPos{};
        Velocity terrainVel{};
        PhysicsStats terrainPS{};

        // SAFETY CHECK 3: Load entity data (exceptions will propagate if issues occur)
        // NOTE: For terrain entities, this now executes with terrainGridMutex held
        auto tuple = loadEntityPhysicsData(registry, voxelGrid, entity, isTerrain, terrainPos,
                                           terrainVel, terrainPS);
        Position& position = std::get<0>(tuple);
        Velocity& velocity = std::get<1>(tuple);
        PhysicsStats& physicsStats = std::get<2>(tuple);

        // Get matter state and apply physics forces
        MatterState matterState = getMatterState(registry, voxelGrid, entity, position, isTerrain);

        auto [newVelocityZ, willStopZ] = resolveVerticalMotion(
            registry, voxelGrid, position, velocity.vz, matterState, entityBeingDebugged, entity);

        // Check stability below entity and apply friction
        bool bellowIsStable = checkBelowStability(registry, voxelGrid, position);

        auto [newVelocityX, newVelocityY, willStopX, willStopY] = applyKineticFrictionDamping(
            velocity.vx, velocity.vy, matterState, bellowIsStable, newVelocityZ);

        if (matterState != MatterState::GAS) {
            // Update velocities
            updateEntityVelocity(velocity, newVelocityX, newVelocityY, newVelocityZ);
        }

        // Calculate movement destination with special collision handling
        auto [movingToX, movingToY, movingToZ, completionTime] =
            calculateMovementDestination(registry, voxelGrid, position, velocity, physicsStats,
                                         velocity.vx, velocity.vy, velocity.vz);

        // NOTE: Terrain grid lock already acquired above (line ~889) for terrain entities

        // Check collision and handle movement
        bool collision = hasCollision(registry, voxelGrid, entity, position.x, position.y,
                                      position.z, movingToX, movingToY, movingToZ, isTerrain);

        if (!collision && completionTime < calculateTimeToMove(physicsStats.minSpeed)) {
            if (!haveMovement) {
                createMovingComponent(registry, dispatcher, voxelGrid, entity, position, velocity,
                                      movingToX, movingToY, movingToZ, completionTime, willStopX,
                                      willStopY, willStopZ, isTerrain);
            }
        } else {
            // Handle lateral collision and try Z-axis movement
            bool handled =
                handleLateralCollision(registry, dispatcher, voxelGrid, entity, position, velocity,
                                       newVelocityX, newVelocityY, newVelocityZ, completionTime,
                                       willStopX, willStopY, willStopZ, haveMovement, isTerrain);

            if (!handled) {
                velocity.vz = 0;
            }

            // Clean up zero velocity
            cleanupZeroVelocity(registry, voxelGrid, entity, position, velocity, isTerrain);
        }
    } catch (const aetherion::InvalidEntityException& e) {
        // Handle entity-specific errors with custom cleanup
        if (isTerrain) {
            cleanupInvalidTerrainEntity(registry, voxelGrid, entity, e);
            return;
        }
        std::cout << "[handleMovement] InvalidEntityException: " << e.what()
                  << " - entity ID=" << static_cast<int>(entity) << std::endl;
        throw;  // Re-throw after logging
    } catch (const aetherion::TerrainLockException& e) {
        // Handle terrain locking errors
        std::cout << "[handleMovement] TerrainLockException: " << e.what() << std::endl;
        throw;  // Re-throw after logging
    } catch (const aetherion::PhysicsException& e) {
        // Handle any other physics-related exceptions
        std::cout << "[handleMovement] PhysicsException: " << e.what()
                  << " - entity ID=" << static_cast<int>(entity) << std::endl;
        throw;  // Re-throw after logging
    } catch (...) {
        // Handle any other unexpected exceptions
        std::cout << "[handleMovement] Unexpected exception occurred"
                  << " - entity ID=" << static_cast<int>(entity) << std::endl;
        throw;  // Re-throw the exception
    }
}

// =========================================================================

void handleMovingTo(entt::registry& registry, VoxelGrid& voxelGrid, entt::entity entity, bool isTerrain) {
    // SAFETY CHECK: Validate entity is still valid
    if (!registry.valid(entity)) {
        std::cout << "[handleMovingTo] WARNING: Invalid entity " << static_cast<int>(entity)
                  << " - skipping" << std::endl;
        return;
    }

    // SAFETY CHECK: Ensure entity has required components
    if (!registry.all_of<MovingComponent, Position>(entity)) {
        std::cout << "[handleMovingTo] WARNING: Entity " << static_cast<int>(entity)
                  << " missing MovingComponent or Position - skipping" << std::endl;
        return;
    }

    // ⚠️ CRITICAL FIX: Acquire terrain grid lock BEFORE reading any terrain data
    // to prevent TOCTOU race conditions where terrain moves between position lookup
    // and velocity/physics reads (see loadEntityPhysicsData lines 507-515)
    // IMPORTANT: Use RAII pattern to ensure lock is ALWAYS released
    std::unique_ptr<TerrainGridLock> terrainLockGuard;
    if (isTerrain) {
        terrainLockGuard = std::make_unique<TerrainGridLock>(voxelGrid.terrainGridRepository.get());
    }

    auto&& [movingComponent, position] = registry.get<MovingComponent, Position>(entity);

    if (movingComponent.timeRemaining <= 0) {
        // voxelGrid.setEntity(position.x, position.y, position.z, -1);
        // voxelGrid.setEntity(movingComponent.movingToX, movingComponent.movingToY,
        //                     movingComponent.movingToZ, static_cast<int>(entity));
        // position.x = movingComponent.movingToX;
        // position.y = movingComponent.movingToY;
        // position.z = movingComponent.movingToZ;

        bool hasVelocity = registry.all_of<Velocity>(entity);
        Velocity velocity;
        if (!hasVelocity) {
            velocity = {0.0f, 0.0f, 0.0f};
        } else {
            velocity = registry.get<Velocity>(entity);
        }

        MatterState matterState = MatterState::SOLID;
        // TODO: Terrains does not get this component like this anymore - fix later
        StructuralIntegrityComponent* sic = registry.try_get<StructuralIntegrityComponent>(entity);
        if (sic) {
            matterState = sic->matterState;
        }

        if (matterState == MatterState::SOLID) {
            float newVelocityZ;
            bool willStopZ{false};
            std::pair<float, bool> resultZ;
            resultZ = calculateVelocityAfterGravityStep(registry, voxelGrid, position.x, position.y,
                                                        position.z, velocity.vz, 1);
            newVelocityZ = resultZ.first;
            resultZ = calculateVelocityAfterGravityStep(registry, voxelGrid, position.x, position.y,
                                                        position.z, velocity.vz, 2);
            willStopZ = resultZ.second;

            velocity.vz = newVelocityZ;
        }

        if (!hasVelocity) {
            registry.emplace<Velocity>(entity, velocity);
        }

        // CRITICAL: Remove MovingComponent to allow new movement events to be processed
        // std::cout << "[handleMovingTo] Removing MovingComponent from entity "
        //           << static_cast<int>(entity) << std::endl;
        registry.remove<MovingComponent>(entity);
    } else {
        movingComponent.timeRemaining--;
    }
}

void PhysicsEngine::processPhysics(entt::registry& registry, VoxelGrid& voxelGrid,
                                   entt::dispatcher& dispatcher, GameClock& clock) {
    // spdlog::get("console")->debug("Processing physics");

    auto velocityView = registry.view<Velocity>();
    for (auto entity : velocityView) {
        // SAFETY CHECK: Validate entity before processing
        if (!registry.valid(entity)) {
            // Entity is invalid but still in Velocity component storage
            // This happens during the timing window between registry.destroy() and hook execution
            // The onDestroyVelocity hook will clean up tracking maps - just skip for now
            std::cout
                << "[processPhysics:Velocity] WARNING: Invalid entity in velocityView - Starting sanity checks; entity ID="
                << static_cast<int>(entity) << " (Starting cleanup routine)" << std::endl;

            Position pos;
            try {
                pos = voxelGrid.terrainGridRepository->getPositionOfEntt(entity);
            } catch (const aetherion::InvalidEntityException& e) {
                // Exception indicates we should stop processing this entity.
                // The mutator function already logged the details.
                Position *_pos = registry.try_get<Position>(entity);
                pos = _pos ? *_pos : Position{-1, -1, -1, DirectionEnum::UP};
                if (pos.x == -1 && pos.y == -1 && pos.z == -1) {
                    std::cout << "[processPhysics:Velocity] Could not find position of entity "
                            << static_cast<int>(entity)
                            << " in TerrainGridRepository or registry - just delete it." << std::endl;
                    printTerrainDiagnostics(registry, voxelGrid, entity, pos,
                                            EntityTypeComponent{}, 0);
                    // throw std::runtime_error("Could not find entity position for Velocity processing");
                    continue;
                }
            }

            EntityTypeComponent *entityType = registry.try_get<EntityTypeComponent>(entity);

            bool isTerrain = (entityType &&
                (entityType->mainType != static_cast<int>(EntityEnum::BEAST) && entityType->mainType != static_cast<int>(EntityEnum::PLANT))
            ) || (entityType == nullptr);
            int entityId = static_cast<int>(entity);
            if (pos.x == -1 && pos.y == -1 && pos.z == -1 && isTerrain) {
                if (entityId != static_cast<int>(TerrainIdTypeEnum::ON_GRID_STORAGE) &&
                    entityId != static_cast<int>(TerrainIdTypeEnum::NONE) ) {
                        std::cout << "[processPhysics:Velocity] Could not find position of entity " << entityId
                                << " in TerrainGridRepository - just delete it." << std::endl;
                        registry.remove<Velocity>(entity);
                        printTerrainDiagnostics(registry, voxelGrid, entity, pos,
                                                entityType ? *entityType : EntityTypeComponent{},
                                                0);
                        _destroyEntity(registry, voxelGrid, entity);

                        printTerrainDiagnostics(registry, voxelGrid, entity, pos,
                                                entityType ? *entityType : EntityTypeComponent{},
                                                0);
                        throw std::runtime_error("Entity invalid during Velocity processing");
                }
                continue;
            } else if (isTerrain) {
                try {
                    entity =
                        reviveColdTerrainEntities(registry, voxelGrid, dispatcher, pos, entity);
                } catch (const aetherion::InvalidEntityException& e) {
                    // Entity cannot be revived (e.g., zero vapor matter converted to empty)
                    std::cout << "[processPhysics:Velocity] Revival failed: " << e.what()
                              << " - entity ID=" << entityId << " - early return" << std::endl;
                    continue;
                }
            } else {
                std::cout << "[processPhysics] Entity " << static_cast<int>(entity)
                          << " has Velocity and Position - proceeding" << std::endl;
                throw std::runtime_error(
                    "Entity invalid during Velocity processing but revival succeeded");
            }
        }

        // SAFETY CHECK: Ensure entity has Position component
        Position pos;
        int entityId = static_cast<int>(entity);
        if (!registry.all_of<Position>(entity)) {
            std::cout << "[processPhysics:Velocity] WARNING: Entity " << static_cast<int>(entity)
                      << " has Velocity but no Position - skipping" << std::endl;

            // delete from terrain repository mapping.
            Position pos;
            try {
                pos = voxelGrid.terrainGridRepository->getPositionOfEntt(entity);
            } catch (const aetherion::InvalidEntityException& e) {
                // Exception indicates we should stop processing this entity.
                // The mutator function already logged the details.
                Position *_pos = registry.try_get<Position>(entity);
                pos = _pos ? *_pos : Position{-1, -1, -1, DirectionEnum::UP};
                if (pos.x == -1 && pos.y == -1 && pos.z == -1) {
                    std::cout << "[processPhysics:Velocity] Could not find position of entity "
                            << static_cast<int>(entity)
                            << " in TerrainGridRepository or registry - just delete it." << std::endl;
                    throw std::runtime_error("Could not find entity position for Velocity processing");
                    // continue;
                }
            }

            if (pos.x == -1 && pos.y == -1 && pos.z == -1) {
                std::cout << "[processPhysics:Velocity] Could not find position of entity " << entityId
                          << " in TerrainGridRepository, skipping entity." << std::endl;
                continue;
            }

            std::cout << "[processPhysics:Velocity] Found position of entity " << entityId
                      << " in TerrainGridRepository at (" << pos.x << ", " << pos.y << ", " << pos.z
                      << ")" << " - checking if vapor terrain needs revival" << std::endl;

            // Check if this is vapor terrain that needs to be revived
            EntityTypeComponent terrainType =
                voxelGrid.terrainGridRepository->getTerrainEntityType(pos.x, pos.y, pos.z);
            int vaporMatter = voxelGrid.terrainGridRepository->getVaporMatter(pos.x, pos.y, pos.z);

            if (terrainType.mainType == static_cast<int>(EntityEnum::TERRAIN) && vaporMatter > 0) {
                std::cout << "[processPhysics:Velocity] Reviving cold vapor terrain at (" << pos.x << ", "
                          << pos.y << ", " << pos.z << ") with vapor matter: " << vaporMatter
                          << std::endl;

                // Revive the terrain by ensuring it's active in ECS
                entity = _ensureEntityActive(voxelGrid, pos.x, pos.y, pos.z);

                std::cout << "[processPhysics:Velocity] Revived vapor terrain as entity "
                          << static_cast<int>(entity) << " - will continue processing" << std::endl;

                // Get the position from the newly revived entity
                pos = registry.get<Position>(entity);
            } else {
                std::cout << "[processPhysics:Velocity] Not vapor terrain (mainType=" << terrainType.mainType
                          << ", vapor=" << vaporMatter << ") - skipping" << std::endl;
                continue;
            }
        } else {
            // std::cout << "[processPhysics] Entity " << static_cast<int>(entity)
            //           << " has Velocity and Position - proceeding" << std::endl;
            pos = registry.get<Position>(entity);
        }

        int entityVoxelGridId = voxelGrid.getEntity(pos.x, pos.y, pos.z);
        if (entityId == entityVoxelGridId) {
            handleMovement(registry, dispatcher, voxelGrid, entity, entityBeingDebugged, false);
            continue;
        }
        int terrainVoxelGridId = voxelGrid.getTerrain(pos.x, pos.y, pos.z);
        if (terrainVoxelGridId != static_cast<int>(TerrainIdTypeEnum::NONE) &&
            terrainVoxelGridId != static_cast<int>(TerrainIdTypeEnum::ON_GRID_STORAGE)) {
            // This entity is actually terrain, remove its velocity
            handleMovement(registry, dispatcher, voxelGrid, entity, entityBeingDebugged, true);
        }
    }

    auto movingComponentView = registry.view<MovingComponent>();
    for (auto entity : movingComponentView) {
        // SAFETY CHECK: Validate entity before processing
        // Note: handleMovingTo also validates, but checking here prevents unnecessary calls
        // SAFETY CHECK: Validate entity before processing
        if (!registry.valid(entity)) {
            // Entity is invalid but still in Velocity component storage
            // This happens during the timing window between registry.destroy() and hook execution
            // The onDestroyVelocity hook will clean up tracking maps - just skip for now
            std::cout
                << "[processPhysics:MovingComponent] WARNING: Invalid entity in velocityView - skipping; entity ID="
                << static_cast<int>(entity) << " (cleanup will be handled by hooks)" << std::endl;

            Position pos;
            try {
                pos = voxelGrid.terrainGridRepository->getPositionOfEntt(entity);
            } catch (const aetherion::InvalidEntityException& e) {
                // Exception indicates we should stop processing this entity.
                // The mutator function already logged the details.
                Position *_pos = registry.try_get<Position>(entity);
                pos = _pos ? *_pos : Position{-1, -1, -1, DirectionEnum::UP};
                if (pos.x == -1 && pos.y == -1 && pos.z == -1) {
                    std::cout << "[processPhysics:MovingComponent] Could not find position of entity "
                            << static_cast<int>(entity)
                            << " in TerrainGridRepository or registry - just delete it." << std::endl;
                    // throw std::runtime_error("Could not find entity position for MovingComponent processing");
                    continue;
                }
            }

            int entityId = static_cast<int>(entity);
            if (pos.x == -1 && pos.y == -1 && pos.z == -1) {
                if (entityId != static_cast<int>(TerrainIdTypeEnum::ON_GRID_STORAGE) &&
                    entityId != static_cast<int>(TerrainIdTypeEnum::NONE) ) {
                        std::cout << "[processPhysics:MovingComponent] Could not find position of entity " << entityId
                                << " in TerrainGridRepository - just delete it." << std::endl;
                        registry.remove<MovingComponent>(entity);
                        _destroyEntity(registry, voxelGrid, entity);
                        throw std::runtime_error("Entity invalid during MovingComponent processing");
                }
                continue;
            } else {
                try {
                    entity =
                        reviveColdTerrainEntities(registry, voxelGrid, dispatcher, pos, entity);
                } catch (const aetherion::InvalidEntityException& e) {
                    // Entity cannot be revived (e.g., zero vapor matter converted to empty)
                    std::cout << "[processPhysics:MovingComponent] Revival failed: " << e.what()
                              << " - entity ID=" << entityId << " - early return" << std::endl;
                    continue;
                }
            }
        }

        // SAFETY CHECK: Ensure entity has Position component
        Position pos;
        bool isTerrain = false;
        int entityId = static_cast<int>(entity);
        if (!registry.all_of<Position>(entity)) {
            std::cout << "[processPhysics:MovingComponent] WARNING: Entity " << static_cast<int>(entity)
                      << " has Velocity but no Position - skipping" << std::endl;

            // delete from terrain repository mapping.
            pos = voxelGrid.terrainGridRepository->getPositionOfEntt(entity);
            if (pos.x == -1 && pos.y == -1 && pos.z == -1) {
                std::cout << "[processPhysics:MovingComponent] Could not find position of entity " << entityId
                          << " in TerrainGridRepository, skipping entity." << std::endl;
                continue;
            }

            std::cout << "[processPhysics:MovingComponent] Found position of entity " << entityId
                      << " in TerrainGridRepository at (" << pos.x << ", " << pos.y << ", " << pos.z
                      << ")" << " - checking if vapor terrain needs revival" << std::endl;

            // Check if this is vapor terrain that needs to be revived
            EntityTypeComponent terrainType =
                voxelGrid.terrainGridRepository->getTerrainEntityType(pos.x, pos.y, pos.z);
            int vaporMatter = voxelGrid.terrainGridRepository->getVaporMatter(pos.x, pos.y, pos.z);

            bool isTerrain = terrainType.mainType == static_cast<int>(EntityEnum::TERRAIN);

            if (terrainType.mainType == static_cast<int>(EntityEnum::TERRAIN) && vaporMatter > 0) {
                std::cout << "[processPhysics:MovingComponent] Reviving cold vapor terrain at (" << pos.x << ", "
                          << pos.y << ", " << pos.z << ") with vapor matter: " << vaporMatter
                          << std::endl;

                // Revive the terrain by ensuring it's active in ECS
                entity = _ensureEntityActive(voxelGrid, pos.x, pos.y, pos.z);

                std::cout << "[processPhysics:MovingComponent] Revived vapor terrain as entity "
                          << static_cast<int>(entity) << " - will continue processing" << std::endl;

                // Get the position from the newly revived entity
                pos = registry.get<Position>(entity);
            } else {
                std::cout << "[processPhysics:MovingComponent] Not vapor terrain (mainType=" << terrainType.mainType
                          << ", vapor=" << vaporMatter << ") - skipping" << std::endl;
                continue;
            }
        } else {
            // std::cout << "[processPhysics] Entity " << static_cast<int>(entity)
            //           << " has Velocity and Position - proceeding" << std::endl;
            pos = registry.get<Position>(entity);
        }

        if (!registry.valid(entity)) {
            // std::cout << "[processPhysics] WARNING: Invalid entity in movingComponentView - "
            //              "skipping; Entity ID="
            //           << static_cast<int>(entity) << std::endl;
            _destroyEntity(registry, voxelGrid, entity);
            continue;
        }

        handleMovingTo(registry, voxelGrid, entity, isTerrain);
    }
}

void PhysicsEngine::processPhysicsAsync(entt::registry& registry, VoxelGrid& voxelGrid,
                                        entt::dispatcher& dispatcher, GameClock& clock) {
    std::scoped_lock lock(physicsMutex);  // Ensure exclusive access

    processingComplete = false;

    auto position_view = registry.view<Position>();

    // spdlog::get("console")->debug("Processing physics Async");

    for (auto entity : position_view) {
        if (!registry.valid(entity)) {
            continue;
        }

        MatterState matterState = MatterState::SOLID;
        StructuralIntegrityComponent* sic = registry.try_get<StructuralIntegrityComponent>(entity);
        if (sic) {
            matterState = sic->matterState;
        }

        if (matterState == MatterState::SOLID || matterState == MatterState::LIQUID) {
            // Your physics processing code here
            if (!registry.all_of<Velocity, Position, EntityTypeComponent>(entity)) {
                auto&& [pos, type] = registry.get<Position, EntityTypeComponent>(entity);
                if (checkIfCanFall(registry, voxelGrid, pos.x, pos.y, pos.z)) {
                    float gravity = PhysicsManager::Instance()->getGravity();
                    dispatcher.enqueue<MoveSolidEntityEvent>(entity, 0, 0, -gravity);
                }
            }
        }
    }

    processingComplete = true;
}

bool PhysicsEngine::isProcessingComplete() const { return processingComplete; }

void PhysicsEngine::onMoveGasEntityEvent(const MoveGasEntityEvent& event) {
    // Validate voxelGrid before acquiring lock
    if (voxelGrid == nullptr) {
        throw std::runtime_error("onMoveGasEntityEvent: voxelGrid is null");
    }

    
    TerrainGridLock lock(voxelGrid->terrainGridRepository.get());

    int terrainId = voxelGrid->getTerrain(event.position.x, event.position.y, event.position.z);

    if (terrainId == static_cast<int>(TerrainIdTypeEnum::NONE)) {
        return;
    }

    // Validate entity early
    bool hasEntity =
        event.entity != entt::null &&
        static_cast<int>(event.entity) != static_cast<int>(TerrainIdTypeEnum::NONE) &&
        static_cast<int>(event.entity) != static_cast<int>(TerrainIdTypeEnum::ON_GRID_STORAGE);

    if (!hasEntity) {
        throw std::runtime_error(
            "onMoveGasEntityEvent: event.entity is null or invalid (Either none or on grid)");
    }

    // Read all terrain data atomically right after lock acquisition
    Position pos = voxelGrid->terrainGridRepository->getPosition(
        event.position.x, event.position.y, event.position.z);
    PhysicsStats physicsStats =
        voxelGrid->terrainGridRepository->getPhysicsStats(pos.x, pos.y, pos.z);
    Velocity velocity = voxelGrid->terrainGridRepository->getVelocity(pos.x, pos.y, pos.z);

    // Ensure Position component exists in ECS for consistency
    bool hasEnTTPosition = registry.all_of<Position>(event.entity);
    if (!hasEnTTPosition) {
        registry.emplace<Position>(event.entity, pos);
    }

    // Check if entity is currently moving
    bool haveMovement = registry.all_of<MovingComponent>(event.entity);

    // Calculate acceleration from forces
    float gravity = PhysicsManager::Instance()->getGravity();
    float accelerationX = static_cast<float>(event.forceX) / physicsStats.mass;
    float accelerationY = static_cast<float>(event.forceY) / physicsStats.mass;
    float accelerationZ = 0.0f;
    if (event.rhoEnv > 0.0f && event.rhoGas > 0.0f) {
        accelerationZ = ((event.rhoEnv - event.rhoGas) * gravity) / event.rhoGas;
    }

    // Translate physics to grid movement
    float newVelocityX, newVelocityY, newVelocityZ;
    std::tie(newVelocityX, newVelocityY, newVelocityZ) =
        translatePhysicsToGridMovement(velocity.vx, velocity.vy, velocity.vz, accelerationX,
                                        accelerationY, accelerationZ, physicsStats.maxSpeed);

    // Determine direction from new velocities (mirrors solid entity pattern)
    DirectionEnum direction =
        getDirectionFromVelocities(newVelocityX, newVelocityY, newVelocityZ);
    bool canApplyForce = true;

    // Only block force application if trying to change direction mid-movement
    if (haveMovement) {
        auto& movingComponent = registry.get<MovingComponent>(event.entity);
        // Allow same-direction acceleration, block direction changes
        canApplyForce = (direction == movingComponent.direction);

        if (!canApplyForce && event.forceApplyNewVelocity) {
            // forceApplyNewVelocity overrides direction check (e.g., evaporation)
            canApplyForce = true;
        }
    }

    if (canApplyForce) {
        // Update velocity in terrainGridRepository (source of truth for terrain entities)
        velocity.vx = newVelocityX;
        velocity.vy = newVelocityY;
        velocity.vz = newVelocityZ;
        voxelGrid->terrainGridRepository->setVelocity(pos.x, pos.y, pos.z, velocity);

        // Synchronize MovingComponent to prevent stale velocity
        if (haveMovement) {
            auto& movingComp = registry.get<MovingComponent>(event.entity);
            movingComp.vx = newVelocityX;
            movingComp.vy = newVelocityY;
            movingComp.vz = newVelocityZ;
        }
    }

}

// Subscribe to the MoveSolidEntityEvent and handle the movement
void PhysicsEngine::onMoveSolidEntityEvent(const MoveSolidEntityEvent& event) {
    spdlog::get("console")->debug("onMoveSolidEntityEvent -> entered");

    if (registry.valid(event.entity) &&
        registry.all_of<Position, EntityTypeComponent, PhysicsStats>(event.entity)) {
        auto&& [pos, type, physicsStats] =
            registry.get<Position, EntityTypeComponent, PhysicsStats>(event.entity);

        // Attempt to retrieve the optional Velocity component
        bool haveMovement = registry.all_of<MovingComponent>(event.entity);
        bool hasVelocity = registry.all_of<Velocity>(event.entity);
        if (!hasVelocity) {
            Velocity velocity = {0.0f, 0.0f, 0.0f};  // Replace as needed
            registry.emplace<Velocity>(event.entity, velocity);
        }
        auto& velocity = registry.get<Velocity>(event.entity);

        // Calculate the acceleration using F = m * a, or a = F / m
        float accelerationX = static_cast<float>(event.forceX) / physicsStats.mass;
        float accelerationY = static_cast<float>(event.forceY) / physicsStats.mass;
        float accelerationZ = 0.0f;

        // Determine if the entity can jump
        if (checkIfCanJump(event)) {
            accelerationZ = static_cast<float>(event.forceZ) / physicsStats.mass;
        }

        // Translate physics to grid movement
        float newVelocityX, newVelocityY, newVelocityZ;
        std::tie(newVelocityX, newVelocityY, newVelocityZ) =
            translatePhysicsToGridMovement(velocity.vx, velocity.vy, velocity.vz, accelerationX,
                                           accelerationY, accelerationZ, physicsStats.maxSpeed);

        // Update direction based on new velocities
        DirectionEnum direction =
            getDirectionFromVelocities(newVelocityX, newVelocityY, newVelocityZ);
        bool canApplyForce = true;

        if (haveMovement) {
            auto& movingComponent = registry.get<MovingComponent>(event.entity);
            (direction == movingComponent.direction) ? canApplyForce = true : canApplyForce = false;
        }
        // If velocities are zero, retain the current direction

        if (canApplyForce) {
            if (registry.all_of<MetabolismComponent>(event.entity)) {
                float metabolismApplyForce =
                    PhysicsManager::Instance()->getMetabolismCostToApplyForce();
                // float metabolismApplyForce = 0.000005f;
                auto& metabolism = registry.get<MetabolismComponent>(event.entity);

                float metabolismCost =
                    physicsStats.mass *
                    (std::abs(event.forceX) + std::abs(event.forceY) + std::abs(event.forceZ)) *
                    metabolismApplyForce;
                metabolism.energyReserve -= metabolismCost;
            }
            velocity.vx = newVelocityX;
            velocity.vy = newVelocityY;
            velocity.vz = newVelocityZ;
            if (direction != DirectionEnum::UPWARD and direction != DirectionEnum::DOWNWARD) {
                pos.direction = direction;
            }
        }

    } else {
        // std::ostringstream ossMessage;
        // ossMessage << "Entity " << static_cast<int>(event.entity) << " lacks required
        // components."; spdlog::get("console")->debug(ossMessage.str());
    }
}

void PhysicsEngine::onTakeItemEvent(const TakeItemEvent& event) {
    /*
    -- This will be maintained before the full refactoring of the onTakeItemEvent

    auto all_view = registry.view<Position, EntityTypeComponent, Inventory>();

    if (registry.valid(event.entity) &&
        registry.all_of<Position, EntityTypeComponent, Inventory>(event.entity)) {
        auto&& [pos, type, inventory] =
            registry.get<Position, EntityTypeComponent, Inventory>(event.entity);

        int takingFromX = pos.x;
        int takingFromY = pos.y;
        int takingFromZ = pos.z;
        if (pos.direction == DirectionEnum::RIGHT) {
            takingFromX++;
        } else if (pos.direction == DirectionEnum::LEFT) {
            takingFromX--;
        } else if (pos.direction == DirectionEnum::UP) {
            takingFromY--;
        } else if (pos.direction == DirectionEnum::DOWN) {
            takingFromY++;
        } else if (pos.direction == DirectionEnum::UPWARD) {
            takingFromZ++;
        } else if (pos.direction == DirectionEnum::DOWNWARD) {
            takingFromZ--;
        }

        int entityId = event.voxelGrid.getEntity(takingFromX, takingFromY, takingFromZ);

        // Check if entityId is a plant or an item entity
        if (entityId != -1) {
            entt::entity takenEntity = static_cast<entt::entity>(entityId);
            if (registry.all_of<EntityTypeComponent, Inventory>(takenEntity)) {
                auto&& [takingFromType, takingFromInventory] =
                    registry.get<EntityTypeComponent, Inventory>(takenEntity);
                if (takingFromType.mainType == 1 && takingFromInventory.itemIDs.size() > 0) {
                    if (!takingFromInventory.itemIDs.empty()) {
                        if (!inventory.isFull()) {
                            int itemId =
                                takingFromInventory.itemIDs.back();  // Retrieve the last item
                            takingFromInventory.itemIDs.pop_back();  // Remove the last item
                            inventory.addItem(itemId);

                            if (registry.all_of<ConsoleLogsComponent>(event.entity)) {
                                ConsoleLogsComponent& consoleLogs =
                                    registry.get<ConsoleLogsComponent>(event.entity);
                                consoleLogs.add_log("Raspberry collected.");
                            }
                        } else {
                            if (registry.all_of<ConsoleLogsComponent>(event.entity)) {
                                ConsoleLogsComponent& consoleLogs =
                                    registry.get<ConsoleLogsComponent>(event.entity);
                                consoleLogs.add_log("Your inventory is full!");
                            }
                        }
                    }
                }
            }
        }
    }
    */

    auto all_view = registry.view<Position, EntityTypeComponent, Inventory, OnTakeItemBehavior>();

    if (registry.valid(event.entity) &&
        registry.all_of<Position, EntityTypeComponent, Inventory, OnTakeItemBehavior>(
            event.entity)) {
        auto&& [pos, type, inventory, onTakeItemBehavior] =
            registry.get<Position, EntityTypeComponent, Inventory, OnTakeItemBehavior>(
                event.entity);

        nb::gil_scoped_acquire acquire;
        int entityId = static_cast<int>(event.entity);

        onTakeItemBehavior.behavior(entityId, event.pyRegistryObj, event.voxelGrid,
                                    event.hoveredEntityId, event.selectedEntityId);
    }
}

void PhysicsEngine::onUseItemEvent(const UseItemEvent& event) {
    auto all_view = registry.view<Position, EntityTypeComponent, Inventory, OnUseItemBehavior>();

    if (registry.valid(event.entity) &&
        registry.all_of<Position, EntityTypeComponent, Inventory, OnUseItemBehavior>(
            event.entity)) {
        auto&& [pos, type, inventory, onUseItemBehavior] =
            registry.get<Position, EntityTypeComponent, Inventory, OnUseItemBehavior>(event.entity);

        nb::gil_scoped_acquire acquire;
        int entityId = static_cast<int>(event.entity);

        onUseItemBehavior.behavior(entityId, event.pyRegistryObj, event.voxelGrid, event.itemSlot,
                                   event.hoveredEntityId, event.selectedEntityId);
    }
}

void PhysicsEngine::onSetPhysicsEntityToDebug(const SetPhysicsEntityToDebug& event) {
    entityBeingDebugged = event.entity;
}

// Water evaporation event handler - moved from EcosystemEngine
void PhysicsEngine::onEvaporateWaterEntityEvent(const EvaporateWaterEntityEvent& event) {
    int terrainId = voxelGrid->getTerrain(event.position.x, event.position.y, event.position.z);
    if (terrainId == static_cast<int>(TerrainIdTypeEnum::NONE)) {
        return;  // No terrain to evaporate from
    }

    // Lock terrain grid for atomic state change (includes PhysicsStats + evaporation)
    voxelGrid->terrainGridRepository->lockTerrainGrid();

    Position pos = voxelGrid->terrainGridRepository->getPosition(event.position.x, event.position.y,
                                                                 event.position.z);
    EntityTypeComponent type = voxelGrid->terrainGridRepository->getTerrainEntityType(
        event.position.x, event.position.y, event.position.z);
    MatterContainer matterContainer = voxelGrid->terrainGridRepository->getTerrainMatterContainer(
        event.position.x, event.position.y, event.position.z);
    PhysicsStats physicsStats = voxelGrid->terrainGridRepository->getPhysicsStats(
        event.position.x, event.position.y, event.position.z);

    // Validate physics constraints
    bool canEvaporate =
        (event.sunIntensity > 0.0f && type.mainType == static_cast<int>(EntityEnum::TERRAIN) &&
         (type.subType0 == static_cast<int>(TerrainEnum::WATER) ||
          type.subType0 == static_cast<int>(TerrainEnum::GRASS)) &&
         matterContainer.WaterMatter > 0);

    if (canEvaporate) {
        // Calculate heat accumulation
        float EVAPORATION_COEFFICIENT = PhysicsManager::Instance()->getEvaporationCoefficient();
        const float HEAT_TO_WATER_EVAPORATION =
            PhysicsManager::Instance()->getHeatToWaterEvaporation();
        float heat = EVAPORATION_COEFFICIENT * event.sunIntensity;

        physicsStats.heat += heat;

        // Check if enough heat to evaporate
        if (physicsStats.heat > HEAT_TO_WATER_EVAPORATION) {
            int waterEvaporated = 1;
            matterContainer.WaterMatter -= waterEvaporated;
            physicsStats.heat = 0.0f;  // Reset heat after evaporation

            voxelGrid->terrainGridRepository->setTerrainMatterContainer(pos.x, pos.y, pos.z,
                                                                        matterContainer);
            voxelGrid->terrainGridRepository->setPhysicsStats(pos.x, pos.y, pos.z, physicsStats);

            // Create or add vapor on z+1
            addOrCreateVaporAbove(registry, *voxelGrid, pos.x, pos.y, pos.z, waterEvaporated);
        } else {
            // Just update heat, no evaporation yet
            voxelGrid->terrainGridRepository->setPhysicsStats(pos.x, pos.y, pos.z, physicsStats);
        }
    }

    voxelGrid->terrainGridRepository->unlockTerrainGrid();
}

// Water condensation event handler - moved from EcosystemEngine
void PhysicsEngine::onCondenseWaterEntityEvent(const CondenseWaterEntityEvent& event) {
    const int x = event.vaporPos.x;
    const int y = event.vaporPos.y;
    const int z = event.vaporPos.z;

    // Lock terrain grid for atomic condensation operation
    voxelGrid->terrainGridRepository->lockTerrainGrid();

    // Get current vapor state
    MatterContainer vaporMatter =
        voxelGrid->terrainGridRepository->getTerrainMatterContainer(x, y, z);

    // Check if there's terrain below
    if (event.terrainBelowId != static_cast<int>(TerrainIdTypeEnum::NONE)) {
        // Path 1: Add condensed water to existing terrain below
        EntityTypeComponent typeBelow =
            voxelGrid->terrainGridRepository->getTerrainEntityType(x, y, z - 1);
        MatterContainer matterBelow =
            voxelGrid->terrainGridRepository->getTerrainMatterContainer(x, y, z - 1);

        // Validate target is water terrain and can accept condensation
        if (typeBelow.mainType == static_cast<int>(EntityEnum::TERRAIN) &&
            typeBelow.subType0 == static_cast<int>(TerrainEnum::WATER) &&
            matterBelow.WaterMatter >= 0 && matterBelow.WaterVapor == 0) {
            // Transfer vapor to water below
            matterBelow.WaterMatter += event.condensationAmount;
            vaporMatter.WaterVapor -= event.condensationAmount;

            voxelGrid->terrainGridRepository->setTerrainMatterContainer(x, y, z - 1, matterBelow);
            voxelGrid->terrainGridRepository->setTerrainMatterContainer(x, y, z, vaporMatter);

            // Cleanup vapor entity if depleted
            if (vaporMatter.WaterVapor <= 0) {
                int vaporTerrainId = voxelGrid->getTerrain(x, y, z);
                if (vaporTerrainId != static_cast<int>(TerrainIdTypeEnum::NONE)) {
                    voxelGrid->terrainGridRepository->unlockTerrainGrid();
                    entt::entity vaporEntity = static_cast<entt::entity>(vaporTerrainId);
                    deleteEntityOrConvertInEmpty(registry, dispatcher, vaporEntity);
                    voxelGrid->terrainGridRepository->lockTerrainGrid();
                }
            }
        }
    } else {
        // Path 2: Create new water tile below (no terrain exists)
        createWaterTerrainBelowVapor(registry, *voxelGrid, x, y, z, event.condensationAmount,
                                     vaporMatter);
    }

    voxelGrid->terrainGridRepository->unlockTerrainGrid();
}

// Water fall event handler - moved from EcosystemEngine
void PhysicsEngine::onWaterFallEntityEvent(const WaterFallEntityEvent& event) {
    if (!registry.valid(event.entity) ||
        !registry.all_of<Position, EntityTypeComponent, MatterContainer>(event.entity)) {
        return;
    }

    auto&& [pos, type, matterContainer] =
        registry.get<Position, EntityTypeComponent, MatterContainer>(event.entity);

    int terrainToCreateWaterId =
        voxelGrid->getTerrain(event.position.x, event.position.y, event.position.z);
    if (terrainToCreateWaterId == -1) {
        createWaterTerrainFromFall(registry, *voxelGrid, event.position.x, event.position.y,
                                   event.position.z, event.fallingAmount, event.entity);
    }
}
