#include "PhysicsEngine.hpp"

#include <algorithm>
#include <chrono>
#include <iostream>
#include <memory>  // For std::unique_ptr
#include <random>
#include <sstream>

#include "EcosystemEngine.hpp"
#include "ecosystem/EcosystemEvents.hpp"
#include "physics/Collision.hpp"
#include "physics/PhysicalMath.hpp"
#include "physics/PhysicsExceptions.hpp"
#include "physics/PhysicsMutators.hpp"
#include "physics/PhysicsUtils.hpp"
#include "physics/PhysicsValidators.hpp"
#include "physics/ReadonlyQueries.hpp"
#include "settings.hpp"
#include "terrain/TerrainGridLock.hpp"  // For TerrainGridLock

// Physics event time series
inline const std::string PHYSICS_MOVE_GAS_ENTITY = "physics_move_gas_entity";
inline const std::string PHYSICS_MOVE_SOLID_ENTITY = "physics_move_solid_entity";
inline const std::string PHYSICS_EVAPORATE_WATER_ENTITY = "physics_evaporate_water_entity";
inline const std::string PHYSICS_CONDENSE_WATER_ENTITY = "physics_condense_water_entity";
inline const std::string PHYSICS_WATER_FALL_ENTITY = "physics_water_fall_entity";
inline const std::string PHYSICS_WATER_SPREAD = "physics_water_spread";
inline const std::string PHYSICS_WATER_GRAVITY_FLOW = "physics_water_gravity_flow";
inline const std::string PHYSICS_TERRAIN_PHASE_CONVERSION = "physics_terrain_phase_conversion";
inline const std::string PHYSICS_VAPOR_CREATION = "physics_vapor_creation";
inline const std::string PHYSICS_VAPOR_MERGE_UP = "physics_vapor_merge_up";
inline const std::string PHYSICS_VAPOR_MERGE_SIDEWAYS = "physics_vapor_merge_sideways";
inline const std::string PHYSICS_ADD_VAPOR_TO_TILE_ABOVE = "physics_add_vapor_to_tile_above";
inline const std::string PHYSICS_CREATE_VAPOR_ENTITY = "physics_create_vapor_entity";
inline const std::string PHYSICS_DELETE_OR_CONVERT_TERRAIN = "physics_delete_or_convert_terrain";
inline const std::string PHYSICS_INVALID_TERRAIN_FOUND = "physics_invalid_terrain_found";

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
    incPhysicsMetric(PHYSICS_WATER_SPREAD);
    _handleWaterSpreadEvent(*voxelGrid, event);
}

void PhysicsEngine::onWaterGravityFlowEvent(const WaterGravityFlowEvent& event) {
    incPhysicsMetric(PHYSICS_WATER_GRAVITY_FLOW);
    _handleWaterGravityFlowEvent(*voxelGrid, event);
}

void PhysicsEngine::onTerrainPhaseConversionEvent(const TerrainPhaseConversionEvent& event) {
    incPhysicsMetric(PHYSICS_TERRAIN_PHASE_CONVERSION);
    _handleTerrainPhaseConversionEvent(*voxelGrid, event);
}

void PhysicsEngine::onVaporCreationEvent(const VaporCreationEvent& event) {
    incPhysicsMetric(PHYSICS_VAPOR_CREATION);
    // Reuse existing helper function which already has proper locking
    TerrainGridLock lock(voxelGrid->terrainGridRepository.get());

    // Defensive check: ensure the tile on position is valid for vapor addition
    int tx = event.position.x;
    int ty = event.position.y;
    int tz = event.position.z;
    int terrainAboveId = voxelGrid->getTerrain(tx, ty, tz);
    if (terrainAboveId != static_cast<int>(TerrainIdTypeEnum::NONE)) {
        EntityTypeComponent typeAbove =
            voxelGrid->terrainGridRepository->getTerrainEntityType(tx, ty, tz);
        MatterContainer matterAbove =
            voxelGrid->terrainGridRepository->getTerrainMatterContainer(tx, ty, tz);

        if (!(typeAbove.mainType == static_cast<int>(EntityEnum::TERRAIN) &&
              typeAbove.subType0 == static_cast<int>(TerrainEnum::WATER) &&
              matterAbove.WaterMatter == 0)) {
            std::ostringstream oss;
            oss << "[onVaporCreationEvent] Cannot add vapor above at (" << tx << ", " << ty << ", "
                << tz << ") - target invalid. type=" << typeAbove.mainType
                << ", subtype=" << typeAbove.subType0 << ", WaterMatter=" << matterAbove.WaterMatter
                << ", WaterVapor=" << matterAbove.WaterVapor;
            spdlog::get("console")->warn(oss.str());
            return;
        }
    }

    createVaporTerrainEntity(registry, *voxelGrid, event.position.x, event.position.y,
                             event.position.z, event.amount);
}

void PhysicsEngine::onCreateVaporEntityEvent(const CreateVaporEntityEvent& event) {
    incPhysicsMetric(PHYSICS_CREATE_VAPOR_ENTITY);
    _handleCreateVaporEntityEvent(registry, dispatcher, *voxelGrid, event);
}

void PhysicsEngine::onDeleteOrConvertTerrainEvent(const DeleteOrConvertTerrainEvent& event) {
    incPhysicsMetric(PHYSICS_DELETE_OR_CONVERT_TERRAIN);
    // Delegate to existing helper which handles effects and soft-empty conversion
    TerrainGridLock lock(voxelGrid->terrainGridRepository.get());

    entt::entity terrain = event.terrain;
    deleteEntityOrConvertInEmpty(registry, dispatcher, const_cast<entt::entity&>(terrain));
}

void PhysicsEngine::onVaporMergeUpEvent(const VaporMergeUpEvent& event) {
    incPhysicsMetric(PHYSICS_VAPOR_MERGE_UP);
    // Lock terrain grid for atomic state change
    TerrainGridLock lock(voxelGrid->terrainGridRepository.get());

    // Validate merge target before adding vapor
    EntityTypeComponent targetType = voxelGrid->terrainGridRepository->getTerrainEntityType(
        event.target.x, event.target.y, event.target.z);
    MatterContainer targetMatter = voxelGrid->terrainGridRepository->getTerrainMatterContainer(
        event.target.x, event.target.y, event.target.z);

    // Only merge into a vapor/transitory tile: must be terrain WATER with no liquid water
    if (targetType.mainType == static_cast<int>(EntityEnum::TERRAIN) &&
        targetType.subType0 == static_cast<int>(TerrainEnum::WATER) &&
        targetMatter.WaterMatter == 0) {
        targetMatter.WaterVapor += event.amount;
        voxelGrid->terrainGridRepository->setTerrainMatterContainer(event.target.x, event.target.y,
                                                                    event.target.z, targetMatter);
    } else {
        std::ostringstream ossMessage;
        ossMessage << "[VaporMergeUpEvent] Merge target invalid at (" << event.target.x << ", "
                   << event.target.y << ", " << event.target.z << ") - skipping merge."
                   << " type=" << targetType.mainType << ", subtype=" << targetType.subType0
                   << ", WaterMatter=" << targetMatter.WaterMatter
                   << ", WaterVapor=" << targetMatter.WaterVapor;
        spdlog::get("console")->warn(ossMessage.str());
        return;
    }

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
        // std::cout << "[VaporMergeUpEvent] Deleting source vapor entity ID="
        //           << static_cast<int>(event.sourceEntity) << "\n";
        std::ostringstream ossMessage;
        ossMessage << "[VaporMergeUpEvent] Source entity valid for vapor merge at ("
                   << event.source.x << ", " << event.source.y << ", " << event.source.z
                   << ") EntityId=" << static_cast<int>(event.sourceEntity);
        spdlog::get("console")->debug(ossMessage.str());
        dispatcher.enqueue<KillEntityEvent>(event.sourceEntity);
    } else {
        std::ostringstream ossMessage;
        ossMessage << "[VaporMergeUpEvent] Source entity invalid for vapor merge at ("
                   << event.source.x << ", " << event.source.y << ", " << event.source.z << ")";
        spdlog::get("console")->debug(ossMessage.str());
    }
}

void PhysicsEngine::onVaporMergeSidewaysEvent(const VaporMergeSidewaysEvent& event) {
    incPhysicsMetric(PHYSICS_VAPOR_MERGE_SIDEWAYS);
    _handleVaporMergeSidewaysEvent(registry, dispatcher, *voxelGrid, event);
}

void PhysicsEngine::onAddVaporToTileAboveEvent(const AddVaporToTileAboveEvent& event) {
    incPhysicsMetric(PHYSICS_ADD_VAPOR_TO_TILE_ABOVE);
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
        spdlog::get("console")->debug(ossMessage.str());
    } else {
        std::ostringstream ossMessage;
        ossMessage << "[AddVaporToTileAboveEvent] Cannot add vapor above; obstruction at (" << x
                   << ", " << y << ", " << z << ")";
        spdlog::get("console")->debug(ossMessage.str());
    }
}

// Register event handlers
void PhysicsEngine::registerEventHandlers(entt::dispatcher& dispatcher) {
    dispatcher.sink<MoveGasEntityEvent>().connect<&PhysicsEngine::onMoveGasEntityEvent>(*this);
    dispatcher.sink<MoveSolidEntityEvent>().connect<&PhysicsEngine::onMoveSolidEntityEvent>(*this);
    dispatcher.sink<MoveSolidLiquidTerrainEvent>().connect<&PhysicsEngine::onMoveSolidLiquidTerrainEvent>(*this);
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
    dispatcher.sink<VaporMergeSidewaysEvent>().connect<&PhysicsEngine::onVaporMergeSidewaysEvent>(
        *this);
    dispatcher.sink<AddVaporToTileAboveEvent>().connect<&PhysicsEngine::onAddVaporToTileAboveEvent>(
        *this);
    dispatcher.sink<CreateVaporEntityEvent>().connect<&PhysicsEngine::onCreateVaporEntityEvent>(
        *this);
    dispatcher.sink<DeleteOrConvertTerrainEvent>()
        .connect<&PhysicsEngine::onDeleteOrConvertTerrainEvent>(*this);
    dispatcher.sink<InvalidTerrainFoundEvent>().connect<&PhysicsEngine::onInvalidTerrainFound>(
        *this);
}

void PhysicsEngine::onInvalidTerrainFound(const InvalidTerrainFoundEvent& event) {
    incPhysicsMetric(PHYSICS_INVALID_TERRAIN_FOUND);
    _handleInvalidTerrainFound(dispatcher, *voxelGrid, event);
}

// Increment metric counter (thread-safe)
void PhysicsEngine::incPhysicsMetric(const std::string& metricName) {
    std::lock_guard<std::mutex> lock(metricsMutex_);
    physicsMetrics_[metricName]++;
}

// Flush current metrics to GameDB via provided handler and reset counters
void PhysicsEngine::flushPhysicsMetrics(GameDBHandler* dbHandler) {
    if (dbHandler == nullptr) return;

    std::lock_guard<std::mutex> lock(metricsMutex_);
    auto now = std::chrono::system_clock::now();
    long long ts = std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count();

    for (const auto& kv : physicsMetrics_) {
        const std::string& name = kv.first;
        uint64_t value = kv.second;
        dbHandler->putTimeSeries(name, ts, static_cast<double>(value));
    }

    // reset counters
    for (auto& kv : physicsMetrics_) {
        kv.second = 0;
    }
}

// ================ END OF REFACTORING ================

// Helper: Load entity data (Position, Velocity, PhysicsStats) from either ECS or terrain storage
// ATOMIC: For terrain, uses getPhysicsSnapshot() to read all data under a single lock
inline std::tuple<Position&, Velocity&, PhysicsStats&> loadEntityPhysicsData(
    entt::registry& registry, entt::dispatcher& dispatcher, VoxelGrid& voxelGrid,
    entt::entity entity, bool isTerrain, Position& terrainPos, Velocity& terrainVel,
    PhysicsStats& terrainPS) {
    if (isTerrain) {
        // std::cout << "[loadEntityPhysicsData] Loading terrain physics data for entity ID=" <<
        // int(entity) << "\n";
        if (!registry.valid(entity)) {
            throw aetherion::InvalidEntityException(
                "Invalid terrain entity in loadEntityPhysicsData");
        }

        // First try to get Position from the ECS registry (source of truth for current frame),
        // then fall back to TerrainGridRepository's byEntity_ map if not present in ECS.
        // This avoids position mismatch where the repo map has already been updated to a
        // new position but the SIC data at the old position is what we actually need.
        auto* ecsPos = registry.try_get<Position>(entity);
        if (ecsPos) {
            terrainPos = *ecsPos;
        } else {
            try {
                terrainPos = voxelGrid.terrainGridRepository->getPositionOfEntt(entity);
            } catch (const aetherion::InvalidEntityException& e) {
                // Entity not found in terrain repository - cleanup and re-throw with more context
                std::ostringstream error;
                error << "Terrain entity " << static_cast<int>(entity)
                      << " not found in TerrainGridRepository: " << e.what();
                throw aetherion::InvalidEntityException(error.str());
            }
        }

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
                _destroyEntity(registry, dispatcher, voxelGrid, entity, false);
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
    auto logger = spdlog::get("console");

    if (isTerrain) {
        logger->debug("[handleMovement] Handling terrain entity ID={}", int(entity));
    }

    // SAFETY CHECK 1: Validate entity is still valid
    if (!registry.valid(entity)) {
        if (isTerrain) {
            logger->warn("[handleMovement][TERRAIN id={}] Entity INVALID in registry, attempting recovery", int(entity));
        }
        try {
            entity = handleInvalidEntityForMovement(registry, voxelGrid, dispatcher, entity);
            if (isTerrain) {
                logger->debug("[handleMovement][TERRAIN id={}] Entity recovered after invalid check", int(entity));
            }
        } catch (const aetherion::InvalidEntityException& e) {
            // Exception indicates we should stop processing this entity.
            // The mutator function already logged the details.
            if (isTerrain) {
                logger->warn("[handleMovement][TERRAIN id={}] Entity recovery FAILED: {} — SKIPPING", int(entity), e.what());
            }
            return;
        }
    }

    // SAFETY CHECK 2: For terrain entities, verify they have Position component
    // This ensures vapor entities are fully initialized before physics processes them
    ensurePositionComponentForTerrain(registry, voxelGrid, entity, isTerrain);

    // It seems that here explodes. [Checked - Confirmed. Here it explodes]
    if (isTerrain) {
        // throw std::runtime_error("handleMovingTo: isTerrain - Just checking...");
        auto pos = registry.get<Position>(entity);
        StructuralIntegrityComponent sic = voxelGrid.terrainGridRepository->getTerrainStructuralIntegrity(
                pos.x, pos.y, pos.z);
        if (sic.matterState == MatterState::LIQUID) {
            spdlog::get("console")->debug("[handleMovement][TERRAIN id={}] StructuralIntegrityComponent.matterState=LIQUID - pos=({},{},{}), matterState={}",
                                      int(entity), pos.x, pos.y, pos.z, static_cast<int>(sic.matterState));
            // throw std::runtime_error("handleMovement: isTerrain and LIQUID - Just checking...");
        }
    }

    bool haveMovement = registry.all_of<MovingComponent>(entity);
    if (isTerrain) {
        logger->debug("[handleMovement][TERRAIN id={}] haveMovement(MovingComponent)={}", int(entity), haveMovement);
    }

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
        auto tuple = loadEntityPhysicsData(registry, dispatcher, voxelGrid, entity, isTerrain,
                                           terrainPos, terrainVel, terrainPS);
        Position& position = std::get<0>(tuple);
        Velocity& velocity = std::get<1>(tuple);
        PhysicsStats& physicsStats = std::get<2>(tuple);

        // Get matter state and apply physics forces
        MatterState matterState = getMatterState(registry, voxelGrid, entity, position, isTerrain);

        auto [newVelocityZ, willStopZ] = resolveVerticalMotion(
            registry, voxelGrid, position, velocity.vz, matterState, entityBeingDebugged, entity);

        if (isTerrain && matterState == MatterState::LIQUID) {
            logger->debug("[handleMovement][TERRAIN id={}] resolveVerticalMotion: newVelocityZ={:.2f} willStopZ={}",
                         int(entity), newVelocityZ, willStopZ);
        }

        // Check stability below entity and apply friction
        bool bellowIsStable = checkBelowStability(registry, voxelGrid, position);

        if (isTerrain && matterState == MatterState::LIQUID) {
            logger->debug("[handleMovement][TERRAIN id={}] bellowIsStable={}", int(entity), bellowIsStable);
        }

        auto [newVelocityX, newVelocityY, willStopX, willStopY] = applyKineticFrictionDamping(
            velocity.vx, velocity.vy, matterState, bellowIsStable, newVelocityZ);

        if (isTerrain && matterState == MatterState::LIQUID) {
            logger->debug("[handleMovement][TERRAIN id={}] afterFriction: newVel=({:.2f},{:.2f}) willStop=({},{})",
                         int(entity), newVelocityX, newVelocityY, willStopX, willStopY);
        }

        if (matterState != MatterState::GAS) {
            // Update velocities
            updateEntityVelocity(velocity, newVelocityX, newVelocityY, newVelocityZ);
            if (isTerrain && matterState == MatterState::LIQUID) {
                logger->debug("[handleMovement][TERRAIN id={}] velocityUpdated: vel=({:.2f},{:.2f},{:.2f})",
                             int(entity), velocity.vx, velocity.vy, velocity.vz);
            }
        } else {
            if (isTerrain) {
                logger->debug("[handleMovement][TERRAIN id={}] SKIPPED velocity update (GAS state)", int(entity));
            }
        }

        // Calculate movement destination with special collision handling
        auto [movingToX, movingToY, movingToZ, completionTime] =
            calculateMovementDestination(registry, voxelGrid, position, velocity, physicsStats,
                                         velocity.vx, velocity.vy, velocity.vz);

        if (isTerrain) {
            int timeThreshold = calculateTimeToMove(physicsStats.minSpeed);
            logger->debug("[handleMovement][TERRAIN id={}] moveDest=({},{},{}) completionTime={:.2f} timeThreshold={}",
                         int(entity), movingToX, movingToY, movingToZ, completionTime, timeThreshold);
        }

        // NOTE: Terrain grid lock already acquired above for terrain entities

        // Check collision and handle movement
        bool collision = hasCollision(registry, voxelGrid, entity, position.x, position.y,
                                      position.z, movingToX, movingToY, movingToZ, isTerrain);

        if (isTerrain) {
            logger->debug("[handleMovement][TERRAIN id={}] collision={}", int(entity), collision);
        }

        if (!collision && completionTime < calculateTimeToMove(physicsStats.minSpeed)) {
            if (!haveMovement) {
                if (isTerrain) {
                    logger->debug("[handleMovement][TERRAIN id={}] CREATING MovingComponent: ({},{},{}) -> ({},{},{}) time={:.2f}",
                                 int(entity), position.x, position.y, position.z,
                                 movingToX, movingToY, movingToZ, completionTime);
                }
                createMovingComponent(registry, dispatcher, voxelGrid, entity, position, velocity,
                                      movingToX, movingToY, movingToZ, completionTime, willStopX,
                                      willStopY, willStopZ, isTerrain);
            } else {
                if (isTerrain) {
                    logger->debug("[handleMovement][TERRAIN id={}] NO-OP: already has MovingComponent", int(entity));
                }
            }
        } else {
            if (isTerrain) {
                bool timeExceeded = completionTime >= calculateTimeToMove(physicsStats.minSpeed);
                logger->debug("[handleMovement][TERRAIN id={}] NOT MOVING: collision={} timeExceeded={} — trying lateral collision handler",
                             int(entity), collision, timeExceeded);
            }

            // Handle lateral collision and try Z-axis movement
            bool handled =
                handleLateralCollision(registry, dispatcher, voxelGrid, entity, position, velocity,
                                       newVelocityX, newVelocityY, newVelocityZ, completionTime,
                                       willStopX, willStopY, willStopZ, haveMovement, isTerrain);

            if (isTerrain) {
                logger->debug("[handleMovement][TERRAIN id={}] lateralCollision handled={}", int(entity), handled);
            }

            if (!handled) {
                velocity.vz = 0;
                if (isTerrain) {
                    logger->debug("[handleMovement][TERRAIN id={}] lateral NOT handled, vz zeroed", int(entity));
                }
            }

            // Clean up zero velocity
            cleanupZeroVelocity(registry, voxelGrid, entity, position, velocity, isTerrain);

            if (isTerrain) {
                logger->debug("[handleMovement][TERRAIN id={}] FINAL: vel=({:.2f},{:.2f},{:.2f}) — entity at rest or cleaned up",
                             int(entity), velocity.vx, velocity.vy, velocity.vz);
            }
        }

    } catch (const aetherion::InvalidEntityException& e) {
        // Handle entity-specific errors with custom cleanup
        if (isTerrain) {
            logger->error("[handleMovement][TERRAIN id={}] InvalidEntityException: {}", int(entity), e.what());
            cleanupInvalidTerrainEntity(registry, dispatcher, voxelGrid, entity, e);
            return;
        }
        std::cout << "[handleMovement] InvalidEntityException: " << e.what()
                  << " - entity ID=" << static_cast<int>(entity) << std::endl;
        throw;  // Re-throw after logging
    } catch (const aetherion::TerrainLockException& e) {
        // Handle terrain locking errors
        if (isTerrain) {
            logger->error("[handleMovement][TERRAIN id={}] TerrainLockException: {}", int(entity), e.what());
        }
        std::cout << "[handleMovement] TerrainLockException: " << e.what() << std::endl;
        throw;  // Re-throw after logging
    } catch (const aetherion::InvalidTerrainMovementException& e) {
        // Handle invalid terrain movement exceptions
        if (isTerrain) {
            logger->warn("[handleMovement][TERRAIN id={}] InvalidTerrainMovementException: {}", int(entity), e.what());
        }
        std::cout << "[handleMovement] InvalidTerrainMovementException: " << e.what()
                  << " - entity ID=" << static_cast<int>(entity) << std::endl;
        return;
    } catch (const aetherion::PhysicsException& e) {
        // Handle any other physics-related exceptions
        if (isTerrain) {
            logger->error("[handleMovement][TERRAIN id={}] PhysicsException: {}", int(entity), e.what());
        }
        std::cout << "[handleMovement] PhysicsException: " << e.what()
                  << " - entity ID=" << static_cast<int>(entity) << std::endl;
        throw;  // Re-throw after logging
    } catch (...) {
        // Handle any other unexpected exceptions
        if (isTerrain) {
            logger->error("[handleMovement][TERRAIN id={}] Unexpected exception occurred", int(entity));
        }
        std::cout << "[handleMovement] Unexpected exception occurred"
                  << " - entity ID=" << static_cast<int>(entity) << std::endl;
        throw;  // Re-throw the exception
    }
}

// =========================================================================

void handleMovingTo(entt::registry& registry, VoxelGrid& voxelGrid, entt::entity entity,
                    bool isTerrain) {
    // SAFETY CHECK: Validate entity is still valid
    if (!registry.valid(entity)) {
        std::ostringstream ossMessage;
        ossMessage << "[handleMovingTo] WARNING: Invalid entity " << static_cast<int>(entity)
                   << " - skipping";
        spdlog::get("console")->debug(ossMessage.str());
        return;
    }

    // SAFETY CHECK: Ensure entity has required components
    if (!registry.all_of<MovingComponent, Position>(entity)) {
        std::ostringstream ossMessage;
        ossMessage << "[handleMovingTo] WARNING: Entity " << static_cast<int>(entity)
                   << " missing MovingComponent or Position - skipping";
        spdlog::get("console")->debug(ossMessage.str());
        return;
    }

    if (isTerrain) {
        throw std::runtime_error("handleMovingTo: isTerrain - Just checking...");
        spdlog::get("console")->debug("[handleMovingTo] isTerrain Fetching matter state for terrain entity ID={}", int(entity));
        auto pos = registry.get<Position>(entity);
        StructuralIntegrityComponent sic = voxelGrid.terrainGridRepository->getTerrainStructuralIntegrity(
                pos.x, pos.y, pos.z);
        if (sic.matterState == MatterState::LIQUID) {
            throw std::runtime_error("handleMovingTo: isTerrain - Just checking...");
        }
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
        StructuralIntegrityComponent* p_sic =
            registry.try_get<StructuralIntegrityComponent>(entity);
        if (p_sic && !isTerrain) {
            matterState = p_sic->matterState;
        } else if (isTerrain) {
            // TODO: Remove me
            throw std::runtime_error(
                "handleMovingTo: isTerrain - Just checking...");
            spdlog::get("console")->debug("[handleMovingTo] isTerrain Fetching matter state for terrain entity ID={}", int(entity));
            StructuralIntegrityComponent sic =
                voxelGrid.terrainGridRepository->getTerrainStructuralIntegrity(
                    position.x, position.y, position.z);
            matterState = sic.matterState;
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
        } else if (isTerrain && matterState == MatterState::LIQUID) {
            // TODO: Remove me
            throw std::runtime_error(
                "handleMovingTo: Entity is liquid, bingo - Just checking...");
        }

        if (!hasVelocity) {
            // if (isTerrain) {
            //     std::ostringstream ossMessage;
            //     ossMessage
            //         << "[handleMovingTo] WARNING: Creating Velocity component for terrain entity
            //         "
            //         << static_cast<int>(entity) << " at position (" << position.x << ", "
            //         << position.y << ", " << position.z << ")";
            //     spdlog::get("console")->info(ossMessage.str());
            // }
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
            // Entity is invalid but still in MovingComponent component storage
            // This happens during the timing window between registry.destroy() and hook execution
            // The onDestroyVelocity hook will clean up tracking maps - just skip for now
            std::ostringstream ossMessage;
            ossMessage << "[processPhysics:MovingComponent] WARNING: Invalid entity in "
                          "velocityView - skipping; entity ID="
                       << static_cast<int>(entity) << " (cleanup will be handled by hooks)";
            spdlog::get("console")->debug(ossMessage.str());

            continue;
        }

        // SAFETY CHECK: Ensure entity has Position component
        Position pos;
        int entityId = static_cast<int>(entity);
        if (!registry.all_of<Position>(entity)) {
            {
                std::ostringstream ossMessage;
                ossMessage << "[processPhysics:Velocity] WARNING: Entity "
                           << static_cast<int>(entity)
                           << " has Velocity but no Position - skipping";
                spdlog::get("console")->debug(ossMessage.str());
            }

            // delete from terrain repository mapping.
            Position pos;
            try {
                pos = voxelGrid.terrainGridRepository->getPositionOfEntt(entity);
            } catch (const aetherion::InvalidEntityException& e) {
                // Exception indicates we should stop processing this entity.
                // The mutator function already logged the details.
                Position* _pos = registry.try_get<Position>(entity);
                pos = _pos ? *_pos : Position{-1, -1, -1, DirectionEnum::UP};
                if (pos.x == -1 && pos.y == -1 && pos.z == -1) {
                    {
                        std::ostringstream ossMessage;
                        ossMessage << "[processPhysics:Velocity] Could not find position of entity "
                                   << static_cast<int>(entity)
                                   << " in TerrainGridRepository or registry - just delete it.";
                        spdlog::get("console")->debug(ossMessage.str());
                    }
                    softDeactivateTerrainEntity(dispatcher, voxelGrid, entity, true);
                    // throw std::runtime_error("Could not find entity position for Velocity
                    // processing");
                    continue;
                }
            }

            if (pos.x == -1 && pos.y == -1 && pos.z == -1) {
                {
                    std::ostringstream ossMessage;
                    ossMessage << "[processPhysics:Velocity] Could not find position of entity "
                               << entityId << " in TerrainGridRepository, skipping entity.";
                    spdlog::get("console")->debug(ossMessage.str());
                }
                continue;
            }

            {
                // std::ostringstream ossMessage;
                // ossMessage << "[processPhysics:Velocity] Found position of entity " << entityId
                //            << " in TerrainGridRepository at (" << pos.x << ", " << pos.y << ", "
                //            << pos.z
                //            << ") - checking if vapor terrain needs revival";
                // spdlog::get("console")->debug(ossMessage.str());
            }

            // Check if this is vapor terrain that needs to be revived
            EntityTypeComponent terrainType =
                voxelGrid.terrainGridRepository->getTerrainEntityType(pos.x, pos.y, pos.z);
            int vaporMatter = voxelGrid.terrainGridRepository->getVaporMatter(pos.x, pos.y, pos.z);

            if (terrainType.mainType == static_cast<int>(EntityEnum::TERRAIN) && vaporMatter > 0) {
                {
                    std::ostringstream ossMessage;
                    ossMessage << "[processPhysics:Velocity] Reviving cold vapor terrain at ("
                               << pos.x << ", " << pos.y << ", " << pos.z
                               << ") with vapor matter: " << vaporMatter;
                    spdlog::get("console")->debug(ossMessage.str());
                }

                // Revive the terrain by ensuring it's active in ECS
                entity = _ensureEntityActive(voxelGrid, pos.x, pos.y, pos.z);

                {
                    std::ostringstream ossMessage;
                    ossMessage << "[processPhysics:Velocity] Revived vapor terrain as entity "
                               << static_cast<int>(entity) << " - will continue processing";
                    spdlog::get("console")->debug(ossMessage.str());
                }

                // Get the position from the newly revived entity
                pos = registry.get<Position>(entity);
            } else {
                {
                    std::ostringstream ossMessage;
                    ossMessage << "[processPhysics:Velocity] Not vapor terrain (mainType="
                               << terrainType.mainType << ", vapor=" << vaporMatter
                               << ") - skipping";
                    spdlog::get("console")->debug(ossMessage.str());
                }
                continue;
            }
        } else {
            // std::cout << "[processPhysics] Entity " << static_cast<int>(entity)
            //           << " has Velocity and Position - proceeding" << std::endl;
            pos = registry.get<Position>(entity);
        }

        int entityVoxelGridId = voxelGrid.getEntity(pos.x, pos.y, pos.z);
        if (entityId == entityVoxelGridId) {
            try {
                handleMovement(registry, dispatcher, voxelGrid, entity, entityBeingDebugged, false);
            } catch (const aetherion::InvalidEntityException& e) {
                std::ostringstream ossMessage;
                ossMessage << "[processPhysics] InvalidEntityException for entity " << entityId
                           << ": " << e.what() << " - skipping";
                spdlog::get("console")->warn(ossMessage.str());
            } catch (const aetherion::TerrainLockException& e) {
                std::ostringstream ossMessage;
                ossMessage << "[processPhysics] TerrainLockException for entity " << entityId
                           << ": " << e.what() << " - skipping";
                spdlog::get("console")->warn(ossMessage.str());
            } catch (const aetherion::PhysicsException& e) {
                std::ostringstream ossMessage;
                ossMessage << "[processPhysics] PhysicsException for entity " << entityId << ": "
                           << e.what() << " - skipping";
                spdlog::get("console")->warn(ossMessage.str());
            }
            continue;
        }
        int terrainVoxelGridId = voxelGrid.getTerrain(pos.x, pos.y, pos.z);
        if (terrainVoxelGridId != static_cast<int>(TerrainIdTypeEnum::NONE) &&
            terrainVoxelGridId != static_cast<int>(TerrainIdTypeEnum::ON_GRID_STORAGE)) {
            // This entity is actually terrain, remove its velocity
            try {
                handleMovement(registry, dispatcher, voxelGrid, entity, entityBeingDebugged, true);
            } catch (const aetherion::InvalidEntityException& e) {
                std::ostringstream ossMessage;
                ossMessage << "[processPhysics] InvalidEntityException for terrain entity "
                           << entityId << ": " << e.what() << " - skipping";
                spdlog::get("console")->warn(ossMessage.str());
            } catch (const aetherion::TerrainLockException& e) {
                std::ostringstream ossMessage;
                ossMessage << "[processPhysics] TerrainLockException for terrain entity "
                           << entityId << ": " << e.what() << " - skipping";
                spdlog::get("console")->warn(ossMessage.str());
            } catch (const aetherion::PhysicsException& e) {
                std::ostringstream ossMessage;
                ossMessage << "[processPhysics] PhysicsException for terrain entity " << entityId
                           << ": " << e.what() << " - skipping";
                spdlog::get("console")->warn(ossMessage.str());
            }
        }
    }

    auto movingComponentView = registry.view<MovingComponent>();
    for (auto entity : movingComponentView) {
        // SAFETY CHECK: Validate entity before processing
        // Note: handleMovingTo also validates, but checking here prevents unnecessary calls
        // SAFETY CHECK: Validate entity before processing
        if (!registry.valid(entity)) {
            // Entity is invalid but still in MovingComponent component storage
            // This happens during the timing window between registry.destroy() and hook execution
            // The onDestroyVelocity hook will clean up tracking maps - just skip for now
            std::ostringstream ossMessage;
            ossMessage << "[processPhysics:MovingComponent] WARNING: Invalid entity in "
                          "movingComponentView - skipping; entity ID="
                       << static_cast<int>(entity) << " (cleanup will be handled by hooks)";
            spdlog::get("console")->debug(ossMessage.str());

            continue;
        }

        // SAFETY CHECK: Ensure entity has Position component
        Position pos;
        bool isTerrain = false;
        int entityId = static_cast<int>(entity);
        if (!registry.all_of<Position>(entity)) {
            {
                std::ostringstream ossMessage;
                ossMessage << "[processPhysics:MovingComponent] WARNING: Entity "
                           << static_cast<int>(entity)
                           << " has Velocity but no Position - skipping";
                spdlog::get("console")->debug(ossMessage.str());
            }

            // delete from terrain repository mapping.
            try {
                pos = voxelGrid.terrainGridRepository->getPositionOfEntt(entity);
            } catch (const aetherion::InvalidEntityException& e) {
                std::ostringstream ossMessage;
                ossMessage << "[processPhysics:MovingComponent] Entity " << entityId
                           << " not found in TerrainGridRepository: " << e.what() << " - skipping";
                spdlog::get("console")->debug(ossMessage.str());
                dispatcher.enqueue<KillEntityEvent>(entity);
                continue;
            }
            if (pos.x == -1 && pos.y == -1 && pos.z == -1) {
                {
                    std::ostringstream ossMessage;
                    ossMessage
                        << "[processPhysics:MovingComponent] Could not find position of entity "
                        << entityId << " in TerrainGridRepository, skipping entity.";
                    spdlog::get("console")->debug(ossMessage.str());
                }
                continue;
            }

            {
                // std::ostringstream ossMessage;
                // ossMessage << "[processPhysics:MovingComponent] Found position of entity " <<
                // entityId
                //           << " in TerrainGridRepository at (" << pos.x << ", " << pos.y << ", "
                //           << pos.z
                //           << ") - checking if vapor terrain needs revival";
                // spdlog::get("console")->debug(ossMessage.str());
            }

            // Check if this is vapor terrain that needs to be revived
            EntityTypeComponent terrainType =
                voxelGrid.terrainGridRepository->getTerrainEntityType(pos.x, pos.y, pos.z);
            int vaporMatter = voxelGrid.terrainGridRepository->getVaporMatter(pos.x, pos.y, pos.z);

            bool isTerrain = terrainType.mainType == static_cast<int>(EntityEnum::TERRAIN);

            if (terrainType.mainType == static_cast<int>(EntityEnum::TERRAIN) && vaporMatter > 0) {
                {
                    std::ostringstream ossMessage;
                    ossMessage
                        << "[processPhysics:MovingComponent] Reviving cold vapor terrain at ("
                        << pos.x << ", " << pos.y << ", " << pos.z
                        << ") with vapor matter: " << vaporMatter;
                    spdlog::get("console")->debug(ossMessage.str());
                }

                // Revive the terrain by ensuring it's active in ECS
                entity = _ensureEntityActive(voxelGrid, pos.x, pos.y, pos.z);

                {
                    std::ostringstream ossMessage;
                    ossMessage
                        << "[processPhysics:MovingComponent] Revived vapor terrain as entity "
                        << static_cast<int>(entity) << " - will continue processing";
                    spdlog::get("console")->debug(ossMessage.str());
                }

                // Get the position from the newly revived entity
                pos = registry.get<Position>(entity);
            } else {
                {
                    std::ostringstream ossMessage;
                    ossMessage << "[processPhysics:MovingComponent] Not vapor terrain (mainType="
                               << terrainType.mainType << ", vapor=" << vaporMatter
                               << ") - skipping";
                    spdlog::get("console")->debug(ossMessage.str());
                }
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
            _destroyEntity(registry, dispatcher, voxelGrid, entity);
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

        Position pos = registry.get<Position>(entity);

        MatterState matterState = MatterState::SOLID;
        bool isTerrain = false;
        StructuralIntegrityComponent* sic = registry.try_get<StructuralIntegrityComponent>(entity);
        if (sic) {
            matterState = sic->matterState;
        } else {
            int terrainId = voxelGrid.getTerrain(pos.x, pos.y, pos.z);
            if (terrainId == static_cast<int>(entity)) {
                isTerrain = true;
                StructuralIntegrityComponent sic =
                    voxelGrid.terrainGridRepository->getTerrainStructuralIntegrity(pos.x, pos.y,
                                                                                   pos.z);
                matterState = sic.matterState;
            }
        }
        if (matterState == MatterState::SOLID || matterState == MatterState::LIQUID) {
            // Physics processing for solid/liquid entities
            if (!isTerrain && registry.all_of<EntityTypeComponent>(entity)) {
                EntityTypeComponent type = registry.get<EntityTypeComponent>(entity);
                // Guard: Don't enqueue movement event if entity is already moving
                // This prevents feedback loops where MoveSolidEntityEvent cascades
                bool isAlreadyMoving = registry.all_of<MovingComponent>(entity);
                if (!isAlreadyMoving && checkIfCanFall(registry, voxelGrid, pos.x, pos.y, pos.z)) {
                    float gravity = PhysicsManager::Instance()->getGravity();
                    dispatcher.enqueue<MoveSolidEntityEvent>(entity, 0, 0, -gravity);
                }
            } else if (isTerrain) {
                EntityTypeComponent type = voxelGrid.terrainGridRepository->getTerrainEntityType(pos.x, pos.y, pos.z);
                bool isAlreadyMoving = registry.all_of<MovingComponent>(entity);

                MatterContainer matterContainer = voxelGrid.terrainGridRepository->getTerrainMatterContainer(
                    pos.x, pos.y, pos.z);
                PhysicsStats physicsStats = voxelGrid.terrainGridRepository->getPhysicsStats(
                    pos.x, pos.y, pos.z);

                spdlog::get("console")->debug(
                    "Processing terrain entity {} at position ({}, {}, {}), entity type.mainType: {}, type.subType0: {}, type.subType1: {}, isTerrain: {}, isAlreadyMoving: {}, physicsStats.mass: {:.2f}",
                    static_cast<int>(entity), pos.x, pos.y, pos.z, type.mainType, type.subType0, type.subType1,
                    isTerrain, isAlreadyMoving, physicsStats.mass);

                if (!isAlreadyMoving && checkIfTerrainCanFall(registry, voxelGrid, pos.x, pos.y, pos.z, matterState)) {
                    if (physicsStats.mass > 0.0f) {
                        float gravity = PhysicsManager::Instance()->getGravity();
                        dispatcher.enqueue<MoveSolidLiquidTerrainEvent>(entity, 0, 0, -gravity);
                    }
                } else {
                    spdlog::get("console")->debug(
                        "Not enqueuing MoveSolidEntityEvent for terrain entity {} at position ({}, {}, {}), entity type.mainType: {} , type.subType0: {}, isTerrain: {}, isAlreadyMoving: {}",
                        static_cast<int>(entity), pos.x, pos.y, pos.z, type.mainType, type.subType0, isTerrain, isAlreadyMoving);

                }
            } else {
                spdlog::get("console")->debug( "Entity {} at position ({}, {}, {}) is not terrain and does not have EntityTypeComponent, skipping physics processing. isTerrain: {}", static_cast<int>(entity), pos.x, pos.y, pos.z, isTerrain);
            }

        } else if (matterState == MatterState::GAS) {
            // Physics processing for gas entities (vapor)
            // Gas entities should be processed via EcosystemEngine for buoyancy-driven movement
            // This prevents duplicate event generation that would cause cascading events
            // Note: Vapor movement is driven by density differences and is handled by
            // the ecosystem engine during its vapor movement phase, not by async gravity events
        }
    }

    processingComplete = true;
}

bool PhysicsEngine::isProcessingComplete() const { return processingComplete; }

// Handle movement of gas entities based on applied forces and environmental conditions
// Algorithm:
// 1. Validate voxelGrid exists
// 2. Acquire terrain grid lock for thread-safe access
// 3. Get terrain at event position (exit early if NONE)
// 4. Validate entity is valid and not null/on-grid
// 5. Read terrain data atomically: position, physics stats, and velocity
// 6. Ensure Position component exists in ECS for consistency
// 7. Check if entity already has MovingComponent
// 8. Calculate acceleration from forces:
//    - X and Y acceleration from applied forces (F = ma)
//    - Z acceleration from buoyancy (density difference with environment)
// 9. Translate physics acceleration to grid velocities with max speed limits
// 10. Determine movement direction from new velocities
// 11. Decide if force can be applied:
//     - Allow if no movement yet
//     - Allow if same direction as current movement
//     - Allow if forceApplyNewVelocity override is set (e.g., evaporation)
//     - Block if changing direction mid-movement
// 12. If force can be applied:
//     - Update velocity in terrain grid (source of truth)
//     - Sync velocity to MovingComponent if it exists
void PhysicsEngine::onMoveGasEntityEvent(const MoveGasEntityEvent& event) {
    incPhysicsMetric(PHYSICS_MOVE_GAS_ENTITY);
    // Step 1: Validate voxelGrid before acquiring lock
    if (voxelGrid == nullptr) {
        throw std::runtime_error("onMoveGasEntityEvent: voxelGrid is null");
    }

    // Step 2: Acquire terrain grid lock for thread-safe access
    TerrainGridLock lock(voxelGrid->terrainGridRepository.get());

    // Step 3: Get terrain at event position (exit early if NONE)
    int terrainId = voxelGrid->getTerrain(event.position.x, event.position.y, event.position.z);

    if (terrainId == static_cast<int>(TerrainIdTypeEnum::NONE)) {
        return;
    }

    if (terrainId != static_cast<int>(event.entity)) {
        // This can happen if the gas entity is in the process of being removed but still has a
        // pending movement event Just skip processing since the entity will be removed shortly
        return;
        // throw std::runtime_error(
        //     "onMoveGasEntityEvent: Terrain ID at position does not match event entity (entity may
        //     be pending removal) - skipping");
    }

    // Step 4: Validate entity is valid and not null/on-grid
    bool hasEntity =
        event.entity != entt::null &&
        static_cast<int>(event.entity) != static_cast<int>(TerrainIdTypeEnum::NONE) &&
        static_cast<int>(event.entity) != static_cast<int>(TerrainIdTypeEnum::ON_GRID_STORAGE);

    if (!hasEntity) {
        throw std::runtime_error(
            "onMoveGasEntityEvent: event.entity is null or invalid (Either none or on grid)");
    }

    if (!registry.valid(event.entity)) {
        // throw std::runtime_error(
        //     "onMoveGasEntityEvent: event.entity is not a valid entity in the registry");
        return;
    }

    // Step 5: Read terrain data atomically: position, physics stats, and velocity
    Position pos = voxelGrid->terrainGridRepository->getPosition(event.position.x, event.position.y,
                                                                 event.position.z);
    PhysicsStats physicsStats =
        voxelGrid->terrainGridRepository->getPhysicsStats(pos.x, pos.y, pos.z);
    Velocity velocity = voxelGrid->terrainGridRepository->getVelocity(pos.x, pos.y, pos.z);

    // Step 6: Ensure Position component exists in ECS for consistency
    bool hasEnTTPosition = registry.all_of<Position>(event.entity);
    if (!hasEnTTPosition) {
        registry.emplace<Position>(event.entity, pos);
    }

    // Step 7: Check if entity already has MovingComponent
    bool haveMovement = registry.all_of<MovingComponent>(event.entity);

    // Step 8: Calculate acceleration from forces (X and Y from applied forces, Z from buoyancy)
    float gravity = PhysicsManager::Instance()->getGravity();
    float accelerationX = static_cast<float>(event.forceX) / physicsStats.mass;
    float accelerationY = static_cast<float>(event.forceY) / physicsStats.mass;
    float accelerationZ = 0.0f;
    if (event.rhoEnv > 0.0f && event.rhoGas > 0.0f) {
        accelerationZ = ((event.rhoEnv - event.rhoGas) * gravity) / event.rhoGas;
    }

    // Step 9: Translate physics acceleration to grid velocities with max speed limits
    float newVelocityX, newVelocityY, newVelocityZ;
    std::tie(newVelocityX, newVelocityY, newVelocityZ) =
        translatePhysicsToGridMovement(velocity.vx, velocity.vy, velocity.vz, accelerationX,
                                       accelerationY, accelerationZ, physicsStats.maxSpeed);

    // Step 10: Determine movement direction from new velocities
    DirectionEnum direction = getDirectionFromVelocities(newVelocityX, newVelocityY, newVelocityZ);
    bool canApplyForce = true;

    // Step 11: Decide if force can be applied (check direction constraints)
    if (haveMovement) {
        auto& movingComponent = registry.get<MovingComponent>(event.entity);
        // Allow same-direction acceleration, block direction changes
        canApplyForce = (direction == movingComponent.direction);

        if (!canApplyForce && event.forceApplyNewVelocity) {
            // forceApplyNewVelocity overrides direction check (e.g., evaporation)
            canApplyForce = true;
        }
    }

    // Step 12: If force can be applied, update velocity in terrain grid and sync to ECS
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
    incPhysicsMetric(PHYSICS_MOVE_SOLID_ENTITY);
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

void PhysicsEngine::onMoveSolidLiquidTerrainEvent(const MoveSolidLiquidTerrainEvent& event) {
    incPhysicsMetric(PHYSICS_MOVE_SOLID_ENTITY);
    spdlog::get("console")->debug(
        "onMoveSolidLiquidTerrainEvent -> entered | entity={} | event.force=({}, {}, {})",
        static_cast<int>(event.entity), event.forceX, event.forceY, event.forceZ);

    if (registry.valid(event.entity) &&
        registry.all_of<Position>(event.entity)) {
        auto pos = registry.get<Position>(event.entity);

        std::optional<int> terrainId = voxelGrid->terrainGridRepository->getTerrainIdIfExists(pos.x, pos.y, pos.z); // Validate terrain exists at this position before proceeding
        if (!terrainId.has_value()) {
            spdlog::get("console")->warn( "onMoveSolidLiquidTerrainEvent -> No terrain found at position ({}, {}, {}) for entity {} - skipping event", pos.x, pos.y, pos.z, static_cast<int>(event.entity));
            return;
        } else if (terrainId.value() != static_cast<int>(event.entity)) {
            spdlog::get("console")->warn( "onMoveSolidLiquidTerrainEvent -> Terrain ID {} at position ({}, {}, {}) does not match event entity {} - skipping event", terrainId.value(), pos.x, pos.y, pos.z, static_cast<int>(event.entity));
            return;
        } else {
            spdlog::get("console")->debug( "onMoveSolidLiquidTerrainEvent -> Found terrain with ID {} at position ({}, {}, {}) for entity {}", terrainId.value(), pos.x, pos.y, pos.z, static_cast<int>(event.entity));
        }

        EntityTypeComponent type = voxelGrid->terrainGridRepository->getTerrainEntityType(
            pos.x, pos.y, pos.z);
        MatterContainer matterContainer = voxelGrid->terrainGridRepository->getTerrainMatterContainer(
            pos.x, pos.y, pos.z);
        PhysicsStats physicsStats = voxelGrid->terrainGridRepository->getPhysicsStats(
            pos.x, pos.y, pos.z);
        StructuralIntegrityComponent structuralIntegrity =
            voxelGrid->terrainGridRepository->getTerrainStructuralIntegrity(pos.x, pos.y, pos.z);

        if (physicsStats.mass == 0.0f &&
            type.mainType == static_cast<int>(EntityEnum::TERRAIN) &&
            type.subType0 == static_cast<int>(TerrainEnum::WATER)) {
            spdlog::get("console")->debug(
                "onMoveSolidLiquidTerrainEvent -> entity={} at pos=({}, {}, {}) has zero mass but is water terrain - assigning mass based on water matter (WaterMatter={})",
                static_cast<int>(event.entity), pos.x, pos.y, pos.z, matterContainer.WaterMatter);
            if (matterContainer.WaterMatter == 0) {
                throw std::runtime_error(
                    "onMoveSolidLiquidTerrainEvent: Water terrain entity has zero mass and zero water matter, cannot assign mass");
            }
            physicsStats.mass = static_cast<float>(matterContainer.WaterMatter);
        } else if (physicsStats.mass == 0.0f) {
            spdlog::get("console")->warn( "onMoveSolidLiquidTerrainEvent -> entity={} at pos=({}, {}, {}), type=({}, {}, {})", static_cast<int>(event.entity), pos.x, pos.y, pos.z, type.mainType, type.subType0, type.subType1);
            spdlog::get("console")->warn( "onMoveSolidLiquidTerrainEvent -> entity={} at pos=({}, {}, {}) has zero mass and is not water terrain - this may lead to unexpected behavior when applying forces", static_cast<int>(event.entity), pos.x, pos.y, pos.z);
                throw std::runtime_error("onMoveSolidLiquidTerrainEvent: Entity has zero mass and is not water terrain, cannot apply forces");
        }


        spdlog::get("console")->debug(
            "onMoveSolidLiquidTerrainEvent -> entity={} | pos=({}, {}, {}) dir={} | "
            "physicsStats: mass={}, maxSpeed={} | entityType: mainType={}, subType0={}",
            static_cast<int>(event.entity), pos.x, pos.y, pos.z, static_cast<int>(pos.direction),
            physicsStats.mass, physicsStats.maxSpeed, type.mainType, type.subType0);

        // Attempt to retrieve the optional Velocity component
        bool haveMovement = registry.all_of<MovingComponent>(event.entity);
        bool hasVelocity = registry.all_of<Velocity>(event.entity);
        if (!hasVelocity) {
            Velocity velocity = {0.0f, 0.0f, 0.0f};
            registry.emplace<Velocity>(event.entity, velocity);
        }
        auto& velocity = registry.get<Velocity>(event.entity);

        spdlog::get("console")->debug(
            "onMoveSolidLiquidTerrainEvent -> entity={} | pre-existing velocity=({}, {}, {}) | "
            "hasVelocity={} | haveMovement={}",
            static_cast<int>(event.entity), velocity.vx, velocity.vy, velocity.vz,
            hasVelocity, haveMovement);

        // Calculate the acceleration using F = m * a, or a = F / m
        float accelerationX = static_cast<float>(event.forceX) / physicsStats.mass;
        float accelerationY = static_cast<float>(event.forceY) / physicsStats.mass;
        float accelerationZ = static_cast<float>(event.forceZ) / physicsStats.mass;

        spdlog::get("console")->debug(
            "onMoveSolidLiquidTerrainEvent -> entity={} | "
            "acceleration=({}, {}, {}) (NOTE: accZ forced to 0, event.forceZ={} is IGNORED) | "
            "allowMultiDirection={}",
            static_cast<int>(event.entity), accelerationX, accelerationY, accelerationZ,
            event.forceZ, PhysicsManager::Instance()->getAllowMultiDirection());

        // Translate physics to grid movement
        float newVelocityX, newVelocityY, newVelocityZ;
        std::tie(newVelocityX, newVelocityY, newVelocityZ) =
            translatePhysicsToGridMovement(velocity.vx, velocity.vy, velocity.vz, accelerationX,
                                           accelerationY, accelerationZ, physicsStats.maxSpeed);

        spdlog::get("console")->debug(
            "onMoveSolidLiquidTerrainEvent -> entity={} | "
            "translatePhysicsToGridMovement result: newVelocity=({}, {}, {}) | "
            "inputs: oldVelocity=({}, {}, {}), accel=({}, {}, {}), maxSpeed={}",
            static_cast<int>(event.entity),
            newVelocityX, newVelocityY, newVelocityZ,
            velocity.vx, velocity.vy, velocity.vz,
            accelerationX, accelerationY, accelerationZ, physicsStats.maxSpeed);

        // Update direction based on new velocities
        DirectionEnum direction =
            getDirectionFromVelocities(newVelocityX, newVelocityY, newVelocityZ);
        bool canApplyForce = true;

        if (haveMovement) {
            auto& movingComponent = registry.get<MovingComponent>(event.entity);
            (direction == movingComponent.direction) ? canApplyForce = true : canApplyForce = false;
            spdlog::get("console")->debug(
                "onMoveSolidLiquidTerrainEvent -> entity={} | "
                "direction check: newDirection={} vs movingComponent.direction={} -> canApplyForce={}",
                static_cast<int>(event.entity),
                static_cast<int>(direction), static_cast<int>(movingComponent.direction), canApplyForce);
        }
        // If velocities are zero, retain the current direction

        if (canApplyForce) {
            spdlog::get("console")->debug(
                "onMoveSolidLiquidTerrainEvent -> APPLYING force | entity={} | "
                "velocity: ({}, {}, {}) -> ({}, {}, {}) | direction: {} (int={})",
                static_cast<int>(event.entity),
                velocity.vx, velocity.vy, velocity.vz,
                newVelocityX, newVelocityY, newVelocityZ,
                static_cast<int>(direction), static_cast<int>(direction));

            velocity.vx = newVelocityX;
            velocity.vy = newVelocityY;
            velocity.vz = newVelocityZ;
            if (direction != DirectionEnum::UPWARD and direction != DirectionEnum::DOWNWARD) {
                pos.direction = direction;
            }
        } else {
            spdlog::get("console")->debug(
                "onMoveSolidLiquidTerrainEvent -> BLOCKED force | entity={} | "
                "cannot apply force due to direction constraints | newVelocity=({}, {}, {})",
                static_cast<int>(event.entity), newVelocityX, newVelocityY, newVelocityZ);
        }
    } else {
        spdlog::get("console")->error(
            "onMoveSolidLiquidTerrainEvent -> entity={} is invalid or missing Position component | "
            "valid={} hasPosition={}",
            static_cast<int>(event.entity),
            registry.valid(event.entity),
            registry.valid(event.entity) ? registry.all_of<Position>(event.entity) : false);
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
    incPhysicsMetric(PHYSICS_EVAPORATE_WATER_ENTITY);
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
    incPhysicsMetric(PHYSICS_CONDENSE_WATER_ENTITY);
    const int x = event.vaporPos.x;
    const int y = event.vaporPos.y;
    const int z = event.vaporPos.z;

    // Acquire RAII terrain grid lock for atomic condensation operation
    TerrainGridLock lock(voxelGrid->terrainGridRepository.get());

    // Get current vapor state
    MatterContainer vaporMatter =
        voxelGrid->terrainGridRepository->getTerrainMatterContainer(x, y, z);

    if (vaporMatter.WaterVapor < event.condensationAmount) {
        // Not enough vapor to condense
        std::ostringstream ossMessage;
        ossMessage << "[onCondenseWaterEntityEvent] Not enough vapor to condense at (" << x << ", "
                   << y << ", " << z << ") - available: " << vaporMatter.WaterVapor
                   << ", requested: " << event.condensationAmount;
        spdlog::get("console")->warn(ossMessage.str());
        return;
    }

    if (vaporMatter.WaterMatter > 0 || vaporMatter.WaterVapor == 0) {
        std::ostringstream ossMessage;
        ossMessage << "[onCondenseWaterEntityEvent] Invalid vapor state for condensation at (" << x
                   << ", " << y << ", " << z << ") - WaterMatter: " << vaporMatter.WaterMatter
                   << ", WaterVapor: " << vaporMatter.WaterVapor;
        spdlog::get("console")->warn(ossMessage.str());
        return;
    }

    spdlog::get("console")->debug(
        "[onCondenseWaterEntityEvent] Attempting to condense vapor at (" + std::to_string(x) +
        ", " + std::to_string(y) + ", " + std::to_string(z) +
        ") with vapor matter: " + std::to_string(vaporMatter.WaterVapor) +
        " and condensation amount: " + std::to_string(event.condensationAmount));

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

            std::ostringstream ossMessage;
            ossMessage << "[onCondenseWaterEntityEvent] Condensed " << event.condensationAmount
                       << " vapor at (" << x << ", " << y << ", " << z << ")\n"
                       << "  ------------------------------------------------\n"
                       << "    matterBelow now has WaterMatter: " << matterBelow.WaterMatter << "\n"
                       << "    matterBelow now has WaterVapor: " << matterBelow.WaterVapor << "\n"
                       << "  ------------------------------------------------\n"
                       << "    vaporMatter now has WaterMatter: " << vaporMatter.WaterMatter << "\n"
                       << "    vaporMatter now has WaterVapor: " << vaporMatter.WaterVapor << "\n"
                       << "  ------------------------------------------------\n"
                       << "    into water terrain below at";

            spdlog::get("console")->debug(ossMessage.str());

            voxelGrid->terrainGridRepository->setTerrainMatterContainer(x, y, z - 1, matterBelow);
            voxelGrid->terrainGridRepository->setTerrainMatterContainer(x, y, z, vaporMatter);

            // Cleanup vapor entity if depleted
            if (vaporMatter.WaterVapor <= 0) {
                int vaporTerrainId = voxelGrid->getTerrain(x, y, z);
                if (vaporTerrainId != static_cast<int>(TerrainIdTypeEnum::NONE)) {
                    // voxelGrid->terrainGridRepository->unlockTerrainGrid();

                    // spdlog::get("console")->warn(
                    //     "[onCondenseWaterEntityEvent] Vapor depleted after condensation -
                    //     removing vapor terrain entity");
                    return;

                    // entt::entity vaporEntity = static_cast<entt::entity>(vaporTerrainId);
                    // deleteEntityOrConvertInEmpty(registry, dispatcher, vaporEntity);
                    // voxelGrid->terrainGridRepository->lockTerrainGrid();
                }
            }
        }
    } else {
        std::ostringstream ossMessage;
        ossMessage << "[onCondenseWaterEntityEvent] No terrain below vapor at (" << x << ", " << y
                   << ", " << z << ") - creating new water terrain below with condensed water";
        spdlog::get("console")->debug(ossMessage.str());
        // Path 2: Create new water tile below (no terrain exists)
        createWaterTerrainBelowVapor(registry, dispatcher, *voxelGrid, x, y, z,
                                     event.condensationAmount, vaporMatter);
    }

    // RAII `lock` will release the terrain grid lock when it goes out of scope
}

// Water fall event handler - moved from EcosystemEngine
void PhysicsEngine::onWaterFallEntityEvent(const WaterFallEntityEvent& event) {
    incPhysicsMetric(PHYSICS_WATER_FALL_ENTITY);
    // On EnTT only need to check for Position, the others are on voxelGrid
    if (static_cast<int>(event.entity) == static_cast<int>(TerrainIdTypeEnum::NONE)) {
        spdlog::get("console")->info(
            "onWaterFallEntityEvent -> entity is ON_GRID_STORAGE, skipping Position check");
        return;
    } else if (static_cast<int>(event.entity) != static_cast<int>(TerrainIdTypeEnum::ON_GRID_STORAGE) &&
        (!registry.valid(event.entity) || !registry.all_of<Position>(event.entity))) {
        spdlog::get("console")->info(
            "onWaterFallEntityEvent -> entity {} is not valid or missing Position component - skipping event",
            static_cast<int>(event.entity));
        return;
    }

    Position pos;
    if (static_cast<int>(event.entity) != static_cast<int>(TerrainIdTypeEnum::ON_GRID_STORAGE)) {
        auto* posPtr = registry.try_get<Position>(event.entity);
        if (!posPtr) {
            spdlog::get("console")->warn(
                "onWaterFallEntityEvent -> entity {} missing Position component after validation - skipping event",
                static_cast<int>(event.entity));
            return;
        }
        pos = *posPtr;
    } else {
        pos = event.position;
    }
    EntityTypeComponent type = voxelGrid->terrainGridRepository->getTerrainEntityType(pos.x, pos.y, pos.z);
    MatterContainer matterContainer = voxelGrid->terrainGridRepository->getTerrainMatterContainer(pos.x, pos.y, pos.z);

    int terrainToCreateWaterId =
        voxelGrid->getTerrain(event.position.x, event.position.y, event.position.z);
    if (terrainToCreateWaterId == static_cast<int>(TerrainIdTypeEnum::NONE)) {
        // TODO [feature#181-water-not-running]: Here seems dangerous and causing segfaults when trying to create water terrain on fall - need to investigate and ensure all necessary state is properly initialized before creating terrain. For now, just logging and skipping the event to prevent crashes.
        // createWaterTerrainFromFall(registry, dispatcher, *voxelGrid, event.position.x,
        //                            event.position.y, event.position.z, event.fallingAmount,
        //                            event.entity, event.sourcePos);
        spdlog::get("console")->info(
            "onWaterFallEntityEvent -> No terrain at position ({}, {}, {}) to create water from fall - skipping event",
            event.position.x, event.position.y, event.position.z);
    }
}
