#include "PhysicsEngine.hpp"

#include <algorithm>
#include <iostream>
#include <random>
#include <sstream>

#include "EcosystemEngine.hpp"
#include "physics/PhysicalMath.hpp"
#include "physics/PhysicsMutators.hpp"
#include "physics/PhysicsValidators.hpp"
#include "physics/ReadonlyQueries.hpp"
#include "settings.hpp"

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

// // =========================================================================
// // ================ 1. READ-ONLY QUERY FUNCTIONS ================
// // =========================================================================
// // These functions perform read-only queries on game state without making
// // any modifications. They are used for validation, collision detection,
// // and state inspection.
// // =========================================================================

// Helper: Check if terrain is soft empty
// static bool isTerrainSoftEmpty(EntityTypeComponent& terrainType) {
//     return (terrainType.mainType == static_cast<int>(EntityEnum::TERRAIN) &&
//             terrainType.subType0 == static_cast<int>(TerrainEnum::EMPTY));
// }

// // Helper: Validate terrain entity ID
// inline void validateTerrainEntityId(entt::entity entity) {
//     int entityId = static_cast<int>(entity);
//     if (entityId == -1 || entityId == -2) {
//         throw std::runtime_error("Invalid terrain entity ID in createMovingComponent");
//     }
// }

bool PhysicsEngine::checkIfCanJump(const MoveSolidEntityEvent& event) {
    // Implement the logic to determine if the entity can jump
    // Placeholder implementation:
    return true;
}

inline bool hasCollision(entt::registry& registry, VoxelGrid& voxelGrid, entt::entity entity,
                         int movingFromX, int movingFromY, int movingFromZ, int movingToX,
                         int movingToY, int movingToZ, bool isTerrain) {
    bool collision = false;
    // Check if the movement is within bounds for x, y, z
    if ((0 <= movingToX && movingToX < voxelGrid.width) &&
        (0 <= movingToY && movingToY < voxelGrid.height) &&
        (0 <= movingToZ && movingToZ < voxelGrid.depth)) {
        int movingToEntityId = voxelGrid.getEntity(movingToX, movingToY, movingToZ);
        bool terrainExists = voxelGrid.checkIfTerrainExists(movingToX, movingToY, movingToZ);

        bool entityCollision = false;
        if (movingToEntityId != -1) {
            entityCollision = true;
        }

        bool terrainCollision = false;
        if (terrainExists) {
            EntityTypeComponent etc = getEntityTypeComponent(
                registry, voxelGrid, entity, movingFromX, movingFromY, movingFromZ, isTerrain);
            EntityTypeComponent terrainEtc = voxelGrid.terrainGridRepository->getTerrainEntityType(
                movingToX, movingToY, movingToZ);
            // Any terrain that is different than water
            if (etc.mainType == static_cast<int>(EntityEnum::TERRAIN)) {
                terrainCollision = true;
            } else if (terrainEtc.subType0 != static_cast<int>(TerrainEnum::EMPTY) &&
                       terrainEtc.subType0 != static_cast<int>(TerrainEnum::WATER)) {
                terrainCollision = true;
            }
        }

        // Check if there is an entity or terrain blocking the destination
        if (entityCollision || terrainCollision) {
            collision = true;
        }
    } else {
        // Out of bounds collision with the world boundary
        collision = true;
    }

    return collision;
}

std::tuple<bool, int, int, int> hasSpecialCollision(entt::registry& registry, VoxelGrid& voxelGrid,
                                                    Position position, int movingToX, int movingToY,
                                                    int movingToZ) {
    bool collision = false;
    int newMovingToX, newMovingToY, newMovingToZ;
    // Check if the movement is within bounds for x, y, z
    if ((0 <= movingToX && movingToX < voxelGrid.width) &&
        (0 <= movingToY && movingToY < voxelGrid.height) &&
        (0 <= movingToZ && movingToZ < voxelGrid.depth)) {
        // int movingToEntityId = voxelGrid.getEntity(movingToX, movingToY, movingToZ);
        bool movingToSameZTerrainExists =
            voxelGrid.checkIfTerrainExists(movingToX, movingToY, movingToZ);
        bool movingToBellowTerrainExists =
            voxelGrid.checkIfTerrainExists(movingToX, movingToY, movingToZ - 1);

        // Check if there is an entity or terrain blocking the destination
        if (movingToSameZTerrainExists) {
            EntityTypeComponent etc = voxelGrid.terrainGridRepository->getTerrainEntityType(
                movingToX, movingToY, movingToZ);
            // subType1 == 1 means ramp_east
            if (etc.subType1 == 1) {
                collision = true;
                newMovingToX = movingToX - 1;
                newMovingToY = movingToY;
                newMovingToZ = movingToZ + 1;
            } else if (etc.subType1 == 2) {
                collision = true;
                newMovingToX = movingToX + 1;
                newMovingToY = movingToY;
                newMovingToZ = movingToZ + 1;
            } else if (etc.subType1 == 7) {
                collision = true;
                newMovingToX = movingToX;
                newMovingToY = movingToY - 1;
                newMovingToZ = movingToZ + 1;
            } else if (etc.subType1 == 8) {
                collision = true;
                newMovingToX = movingToX;
                newMovingToY = movingToY + 1;
                newMovingToZ = movingToZ + 1;
            }
        } else if (movingToBellowTerrainExists) {
            EntityTypeComponent etc = voxelGrid.terrainGridRepository->getTerrainEntityType(
                movingToX, movingToY, movingToZ - 1);
            // subType1 == 1 means ramp_east
            if (etc.subType1 == 1) {
                collision = true;
                newMovingToX = movingToX + 1;
                newMovingToY = movingToY;
                newMovingToZ = movingToZ - 1;
            } else if (etc.subType1 == 2) {
                collision = true;
                newMovingToX = movingToX - 1;
                newMovingToY = movingToY;
                newMovingToZ = movingToZ - 1;
            } else if (etc.subType1 == 7) {
                collision = true;
                newMovingToX = movingToX;
                newMovingToY = movingToY + 1;
                newMovingToZ = movingToZ - 1;
            } else if (etc.subType1 == 8) {
                collision = true;
                newMovingToX = movingToX;
                newMovingToY = movingToY - 1;
                newMovingToZ = movingToZ - 1;
            }
        }
    }

    return std::make_tuple(collision, newMovingToX, newMovingToY, newMovingToZ);
}

std::tuple<float, float, float> PhysicsEngine::translatePhysicsToGridMovement(
    float velocityX, float velocityY, float velocityZ, float accelerationX, float accelerationY,
    float accelerationZ, int16_t maxSpeed) {
    float newVx;
    float newVy;
    float newVz;

    if (PhysicsManager::Instance()->getAllowMultiDirection()) {
        newVx = velocityX + accelerationX;
        newVy = velocityY + accelerationY;
        newVz = velocityZ + accelerationZ;
    } else {
        // Calculate absolute velocities for comparison
        float absVx = std::abs(velocityX);
        float absVy = std::abs(velocityY);
        float absVz = std::abs(velocityZ);

        float absAx = std::abs(accelerationX);
        float absAy = std::abs(accelerationY);
        float absAz = std::abs(accelerationZ);

        // Determine which axis has the greatest absolute velocity and acceleration
        if (absVx >= absVy && absVx >= absVz &&  // X has the greatest velocity
            absAx >= absAy && absAx >= absAz     // X has the greatest acceleration
        ) {
            // Apply acceleration only to X-axis
            newVx = velocityX + accelerationX;
            newVy = 0.0f;
            newVz = 0.0f;
        } else if (absVy >= absVx && absVy >= absVz &&  // Y has the greatest velocity
                   absAy >= absAx && absAy >= absAz     // Y has the greatest acceleration
        ) {
            // Apply acceleration only to Y-axis
            newVx = 0.0f;
            newVy = velocityY + accelerationY;
            newVz = 0.0f;
        } else if (absVz >= absVx && absVz >= absVy &&  // Z has the greatest velocity
                   absAz >= absAx && absAz >= absAy     // Z has the greatest acceleration
        ) {
            // Apply acceleration only to Z-axis
            newVx = 0.0f;
            newVy = 0.0f;
            newVz = velocityZ + accelerationZ;
        } else {
            // No single axis has both the greatest velocity and acceleration
            // You can choose to handle this case as needed. For example:
            // - Apply acceleration to the axis with the highest acceleration
            // - Apply no acceleration
            // - Apply a default behavior

            // Example: Apply acceleration to the axis with the highest acceleration
            if (absAx >= absAy && absAx >= absAz) {
                newVx = velocityX + accelerationX;
                newVy = 0.0f;
                newVz = 0.0f;
            } else if (absAy >= absAx && absAy >= absAz) {
                newVx = 0.0f;
                newVy = velocityY + accelerationY;
                newVz = 0.0f;
            } else {
                newVx = 0.0f;
                newVy = 0.0f;
                newVz = velocityZ + accelerationZ;
            }
        }
    }

    float absNewVx = std::abs(newVx);
    float absNewVy = std::abs(newVy);
    float absNewVz = std::abs(newVz);
    float floatMaxSpeed;
    if ((absNewVx > 0 && absNewVy > 0) || (absNewVx > 0 && absNewVz > 0) ||
        (absNewVy > 0 && absNewVz > 0)) {
        floatMaxSpeed = static_cast<float>(maxSpeed) / 2;
    } else {
        floatMaxSpeed = static_cast<float>(maxSpeed);
    }

    // Clamp velocities to maxSpeed
    newVx = std::clamp(newVx, -floatMaxSpeed, floatMaxSpeed);
    newVy = std::clamp(newVy, -floatMaxSpeed, floatMaxSpeed);
    newVz = std::clamp(newVz, -floatMaxSpeed, floatMaxSpeed);

    return std::make_tuple(newVx, newVy, newVz);
}

std::pair<float, bool> calculateVelocityAfterGravityStep(entt::registry& registry,
                                                         VoxelGrid& voxelGrid, int i, int j, int k,
                                                         float velocityZ, int dt) {
    float gravity = PhysicsManager::Instance()->getGravity();
    float newVelocityZ;

    if (velocityZ > 0.0f or checkIfCanFall(registry, voxelGrid, i, j, k)) {
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

// Helper: Initialize MovingComponent with movement parameters
inline MovingComponent initializeMovingComponent(const Position& position, const Velocity& velocity,
                                                 int movingToX, int movingToY, int movingToZ,
                                                 float completionTime, bool willStopX,
                                                 bool willStopY, bool willStopZ) {
    MovingComponent movingComponent;
    movingComponent.isMoving = true;

    movingComponent.movingFromX = position.x;
    movingComponent.movingFromY = position.y;
    movingComponent.movingFromZ = position.z;

    movingComponent.movingToX = movingToX;
    movingComponent.movingToY = movingToY;
    movingComponent.movingToZ = movingToZ;

    movingComponent.vx = velocity.vx;
    movingComponent.vy = velocity.vy;
    movingComponent.vz = velocity.vz;

    movingComponent.willStopX = willStopX;
    movingComponent.willStopY = willStopY;
    movingComponent.willStopZ = willStopZ;

    movingComponent.completionTime = completionTime;
    movingComponent.timeRemaining = completionTime;

    movingComponent.direction = getDirectionFromVelocities(velocity.vx, velocity.vy, velocity.vz);

    return movingComponent;
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
void createMovingComponent(entt::registry& registry, entt::dispatcher& dispatcher,
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

// Helper function for creating or adding vapor (moved from EcosystemEngine)
void createOrAddVaporPhysics(entt::registry& registry, VoxelGrid& voxelGrid, int x, int y, int z,
                             int amount) {
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
        // No entity above; create vapor entity
        auto newVaporEntity = registry.create();
        Position newPosition = {x, y, z + 1, DirectionEnum::DOWN};

        EntityTypeComponent newType = {};
        newType.mainType = 0;  // Terrain type
        newType.subType0 = 1;  // Water terrain (vapor)
        newType.subType1 = 0;

        MatterContainer newMatterContainer = {};
        newMatterContainer.WaterVapor = amount;
        newMatterContainer.WaterMatter = 0;

        PhysicsStats newPhysicsStats = {};
        newPhysicsStats.mass = 0.1;
        newPhysicsStats.maxSpeed = 10;
        newPhysicsStats.minSpeed = 0.0;

        StructuralIntegrityComponent newStructuralIntegrityComponent = {};
        newStructuralIntegrityComponent.canStackEntities = false;
        newStructuralIntegrityComponent.maxLoadCapacity = -1;
        newStructuralIntegrityComponent.matterState = MatterState::GAS;

        voxelGrid.terrainGridRepository->setPosition(x, y, z + 1, newPosition);
        voxelGrid.terrainGridRepository->setTerrainEntityType(x, y, z + 1, newType);
        voxelGrid.terrainGridRepository->setTerrainMatterContainer(x, y, z + 1, newMatterContainer);
        voxelGrid.terrainGridRepository->setTerrainStructuralIntegrity(
            x, y, z + 1, newStructuralIntegrityComponent);
        voxelGrid.terrainGridRepository->setPhysicsStats(x, y, z + 1, newPhysicsStats);
        int newTerrainId = static_cast<int>(newVaporEntity);
        voxelGrid.terrainGridRepository->setTerrainId(x, y, z + 1, newTerrainId);
    }
}

// New event handlers for water physics (all state changes)
void PhysicsEngine::onWaterSpreadEvent(const WaterSpreadEvent& event) {
    // Lock terrain grid for atomic state change
    voxelGrid->terrainGridRepository->lockTerrainGrid();

    // Transfer water from source to target
    MatterContainer sourceMatter = event.sourceMatter;
    MatterContainer targetMatter = event.targetMatter;

    targetMatter.WaterMatter += event.amount;
    sourceMatter.WaterMatter -= event.amount;

    // Update both voxels atomically
    voxelGrid->terrainGridRepository->setTerrainMatterContainer(event.target.x, event.target.y,
                                                                event.target.z, targetMatter);
    voxelGrid->terrainGridRepository->setTerrainMatterContainer(event.source.x, event.source.y,
                                                                event.source.z, sourceMatter);

    voxelGrid->terrainGridRepository->unlockTerrainGrid();
}

void PhysicsEngine::onWaterGravityFlowEvent(const WaterGravityFlowEvent& event) {
    // Lock terrain grid for atomic state change
    voxelGrid->terrainGridRepository->lockTerrainGrid();

    // Transfer water downward
    MatterContainer sourceMatter = event.sourceMatter;
    MatterContainer targetMatter = event.targetMatter;

    targetMatter.WaterMatter += event.amount;
    sourceMatter.WaterMatter -= event.amount;

    // Update both voxels atomically
    voxelGrid->terrainGridRepository->setTerrainMatterContainer(event.target.x, event.target.y,
                                                                event.target.z, targetMatter);
    voxelGrid->terrainGridRepository->setTerrainMatterContainer(event.source.x, event.source.y,
                                                                event.source.z, sourceMatter);

    voxelGrid->terrainGridRepository->unlockTerrainGrid();
}

void PhysicsEngine::onTerrainPhaseConversionEvent(const TerrainPhaseConversionEvent& event) {
    // Lock terrain grid for atomic state change
    voxelGrid->terrainGridRepository->lockTerrainGrid();

    // Apply terrain phase conversion (e.g., soft-empty -> water/vapor)
    voxelGrid->terrainGridRepository->setTerrainEntityType(event.position.x, event.position.y,
                                                           event.position.z, event.newType);
    voxelGrid->terrainGridRepository->setTerrainMatterContainer(event.position.x, event.position.y,
                                                                event.position.z, event.newMatter);
    voxelGrid->terrainGridRepository->setTerrainStructuralIntegrity(
        event.position.x, event.position.y, event.position.z, event.newStructuralIntegrity);

    voxelGrid->terrainGridRepository->unlockTerrainGrid();
}

void PhysicsEngine::onVaporCreationEvent(const VaporCreationEvent& event) {
    // Reuse existing helper function which already has proper locking
    createOrAddVaporPhysics(registry, *voxelGrid, event.position.x, event.position.y,
                            event.position.z, event.amount);
}

void PhysicsEngine::onCreateVaporEntityEvent(const CreateVaporEntityEvent& event) {
    // Atomic operation: Create entity and update terrain grid
    voxelGrid->terrainGridRepository->lockTerrainGrid();

    // Create new entity for the vapor
    entt::entity newEntity = registry.create();
    int terrainId = static_cast<int>(newEntity);

    // Set terrain ID atomically
    voxelGrid->terrainGridRepository->setTerrainId(event.position.x, event.position.y,
                                                   event.position.z, terrainId);

    voxelGrid->terrainGridRepository->unlockTerrainGrid();

    // Now dispatch the move event with the newly created entity
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

void PhysicsEngine::onVaporMergeUpEvent(const VaporMergeUpEvent& event) {
    // Lock terrain grid for atomic state change
    voxelGrid->terrainGridRepository->lockTerrainGrid();

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

    voxelGrid->terrainGridRepository->unlockTerrainGrid();

    // Delete or convert source entity after unlocking
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

void PhysicsEngine::onAddVaporToTileAboveEvent(const AddVaporToTileAboveEvent& event) {
    // Lock terrain grid for atomic operation
    voxelGrid->terrainGridRepository->lockTerrainGrid();

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

    voxelGrid->terrainGridRepository->unlockTerrainGrid();
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
    dispatcher.sink<AddVaporToTileAboveEvent>().connect<&PhysicsEngine::onAddVaporToTileAboveEvent>(
        *this);
    dispatcher.sink<CreateVaporEntityEvent>().connect<&PhysicsEngine::onCreateVaporEntityEvent>(
        *this);
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
            throw std::runtime_error("Invalid terrain entity");
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
                registry.destroy(entity);
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

// Helper: Apply friction to horizontal velocities
inline std::tuple<float, float, bool, bool> applyFrictionForces(float velocityX, float velocityY,
                                                                MatterState matterState,
                                                                bool bellowIsStable,
                                                                float newVelocityZ) {
    if (matterState == MatterState::SOLID && bellowIsStable && newVelocityZ <= 0) {
        auto resultX = calculateVelocityAfterFrictionStep(velocityX, 1);
        float newVelocityX = resultX.first;

        auto resultY = calculateVelocityAfterFrictionStep(velocityY, 1);
        float newVelocityY = resultY.first;

        resultX = calculateVelocityAfterFrictionStep(velocityX, 2);
        bool willStopX = resultX.second;

        resultY = calculateVelocityAfterFrictionStep(velocityY, 2);
        bool willStopY = resultY.second;

        return {newVelocityX, newVelocityY, willStopX, willStopY};
    }
    return {velocityX, velocityY, false, false};
}

// Helper: Calculate movement destination with special collision handling
inline std::tuple<int, int, int, float> calculateMovementDestination(
    entt::registry& registry, VoxelGrid& voxelGrid, const Position& position, Velocity& velocity,
    const PhysicsStats& physicsStats, float newVelocityX, float newVelocityY, float newVelocityZ) {
    float completionTime = calculateTimeToMove(newVelocityX, newVelocityY, newVelocityZ);
    int movingToX = position.x + getDirectionFromVelocity(newVelocityX);
    int movingToY = position.y + getDirectionFromVelocity(newVelocityY);
    int movingToZ = position.z + getDirectionFromVelocity(newVelocityZ);

    bool specialCollision;
    int newMovingToX, newMovingToY, newMovingToZ;
    std::tie(specialCollision, newMovingToX, newMovingToY, newMovingToZ) =
        hasSpecialCollision(registry, voxelGrid, position, movingToX, movingToY, movingToZ);

    if (specialCollision) {
        movingToX = newMovingToX;
        movingToY = newMovingToY;
        movingToZ = newMovingToZ;

        float newDirectionX = newMovingToX - position.x;
        float newDirectionY = newMovingToY - position.y;
        float newDirectionZ = newMovingToZ - position.z;

        velocity.vx = newDirectionX * (physicsStats.minSpeed / 2);
        velocity.vy = newDirectionY * (physicsStats.minSpeed / 2);
        velocity.vz = newDirectionZ * (physicsStats.minSpeed / 2);

        completionTime = calculateTimeToMove(velocity.vx, velocity.vy, velocity.vz);
    }

    return {movingToX, movingToY, movingToZ, completionTime};
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
        std::ostringstream error;
        error << "[handleMovement] Invalid entity " << static_cast<int>(entity);
        throw std::runtime_error(error.str());
    }

    // SAFETY CHECK 2: For terrain entities, verify they have Position component
    // This ensures vapor entities are fully initialized before physics processes them
    if (isTerrain && !registry.all_of<Position>(entity)) {
        std::ostringstream error;
        error << "[handleMovement] Terrain entity " << static_cast<int>(entity)
              << " missing Position component (not fully initialized yet)";
        throw std::runtime_error(error.str());
    }

    bool haveMovement = registry.all_of<MovingComponent>(entity);

    // ⚠️ CRITICAL FIX: Acquire terrain grid lock BEFORE reading any terrain data
    // to prevent TOCTOU race conditions where terrain moves between position lookup
    // and velocity/physics reads (see loadEntityPhysicsData lines 507-515)
    // IMPORTANT: Use try-finally pattern to ensure lock is ALWAYS released
    if (isTerrain) {
        voxelGrid.terrainGridRepository->lockTerrainGrid();
    }

    // Exception-safe lock release using try-catch
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

        auto [newVelocityX, newVelocityY, willStopX, willStopY] = applyFrictionForces(
            velocity.vx, velocity.vy, matterState, bellowIsStable, newVelocityZ);

        if (matterState != MatterState::GAS) {
            // Update velocities
            velocity.vx = newVelocityX;
            velocity.vy = newVelocityY;
            velocity.vz = newVelocityZ;
        }

        // Calculate movement destination with special collision handling
        auto [movingToX, movingToY, movingToZ, completionTime] =
            calculateMovementDestination(registry, voxelGrid, position, velocity, physicsStats,
                                         velocity.vx, velocity.vy, velocity.vz);

        // NOTE: Terrain grid lock already acquired above (line ~741) for terrain entities

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
    } catch (...) {
        // CRITICAL: Release lock before re-throwing exception
        if (isTerrain) {
            std::cout << "[handleMovement] Exception occurred, unlocking terrain grid.\n";
            voxelGrid.terrainGridRepository->unlockTerrainGrid();
        }
        throw;  // Re-throw the exception
    }

    // Unlock terrain grid if we locked it (normal exit path)
    if (isTerrain) {
        voxelGrid.terrainGridRepository->unlockTerrainGrid();
    }
}

// =========================================================================

void handleMovingTo(entt::registry& registry, VoxelGrid& voxelGrid, entt::entity entity) {
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
            std::cout << "[processPhysics] WARNING: Invalid entity in velocityView - skipping; entity ID="
                      << static_cast<int>(entity)
                      << std::endl;
            continue;
        }

        // SAFETY CHECK: Ensure entity has Position component
        if (!registry.all_of<Position>(entity)) {
            std::cout << "[processPhysics] WARNING: Entity " << static_cast<int>(entity)
                      << " has Velocity but no Position - skipping" << std::endl;
            continue;
        }

        Position pos = registry.get<Position>(entity);
        int entityId = static_cast<int>(entity);
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
        if (!registry.valid(entity)) {
            std::cout
                << "[processPhysics] WARNING: Invalid entity in movingComponentView - skipping"
                << std::endl;
            continue;
        }

        handleMovingTo(registry, voxelGrid, entity);
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

    voxelGrid->terrainGridRepository->lockTerrainGrid();

    try {
        int terrainId = voxelGrid->getTerrain(event.position.x, event.position.y, event.position.z);

        if (terrainId == static_cast<int>(TerrainIdTypeEnum::NONE)) {
            voxelGrid->terrainGridRepository->unlockTerrainGrid();
            return;
        }

        // Validate entity early
        bool hasEntity =
            event.entity != entt::null &&
            static_cast<int>(event.entity) != static_cast<int>(TerrainIdTypeEnum::NONE) &&
            static_cast<int>(event.entity) != static_cast<int>(TerrainIdTypeEnum::ON_GRID_STORAGE);

        if (!hasEntity) {
            voxelGrid->terrainGridRepository->unlockTerrainGrid();
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

        voxelGrid->terrainGridRepository->unlockTerrainGrid();

    } catch (...) {
        // Exception safety: always release lock
        voxelGrid->terrainGridRepository->unlockTerrainGrid();
        throw;
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
            createOrAddVaporPhysics(registry, *voxelGrid, pos.x, pos.y, pos.z, waterEvaporated);
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
        entt::entity newWaterEntity = registry.create();
        Position newPosition = {x, y, z - 1, DirectionEnum::DOWN};

        EntityTypeComponent newType = {};
        newType.mainType = static_cast<int>(EntityEnum::TERRAIN);
        newType.subType0 = static_cast<int>(TerrainEnum::WATER);
        newType.subType1 = 0;

        MatterContainer newMatterContainer = {};
        newMatterContainer.WaterMatter = event.condensationAmount;
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
        registry.emplace<StructuralIntegrityComponent>(newWaterEntity,
                                                       newStructuralIntegrityComponent);
        registry.emplace<PhysicsStats>(newWaterEntity, newPhysicsStats);

        voxelGrid->setTerrain(x, y, z - 1, static_cast<int>(newWaterEntity));

        // Reduce vapor amount
        vaporMatter.WaterVapor -= event.condensationAmount;
        voxelGrid->terrainGridRepository->setTerrainMatterContainer(x, y, z, vaporMatter);

        // Cleanup vapor entity if depleted
        if (vaporMatter.WaterVapor <= 0) {
            int vaporTerrainId = voxelGrid->getTerrain(x, y, z);
            if (vaporTerrainId != static_cast<int>(TerrainIdTypeEnum::NONE)) {
                voxelGrid->setTerrain(x, y, z, static_cast<int>(TerrainIdTypeEnum::NONE));
                registry.destroy(static_cast<entt::entity>(vaporTerrainId));
            }
        }
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
        // Lock for atomic state change
        voxelGrid->terrainGridRepository->lockTerrainGrid();

        // Create a new water tile
        entt::entity newWaterEntity = registry.create();
        Position newPosition = {event.position.x, event.position.y, event.position.z,
                                DirectionEnum::DOWN};

        EntityTypeComponent newType = {};
        newType.mainType = static_cast<int>(EntityEnum::TERRAIN);
        newType.subType0 = static_cast<int>(TerrainEnum::WATER);
        newType.subType1 = 0;

        MatterContainer newMatterContainer = {};
        newMatterContainer.WaterMatter = event.fallingAmount;
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
        registry.emplace<StructuralIntegrityComponent>(newWaterEntity,
                                                       newStructuralIntegrityComponent);
        registry.emplace<PhysicsStats>(newWaterEntity, newPhysicsStats);

        voxelGrid->setTerrain(event.position.x, event.position.y, event.position.z,
                              static_cast<int>(newWaterEntity));

        matterContainer.WaterMatter -= event.fallingAmount;

        if (matterContainer.WaterVapor <= 0 && matterContainer.WaterMatter <= 0 &&
            type.mainType == static_cast<int>(EntityEnum::TERRAIN) &&
            type.subType0 == static_cast<int>(TerrainEnum::WATER)) {
            voxelGrid->setTerrain(pos.x, pos.y, pos.z, -1);
            registry.destroy(event.entity);
        }

        voxelGrid->terrainGridRepository->unlockTerrainGrid();
    }
}

// =========================================================================
// ================ Terrain Conversion Helper Functions ================
// These functions handle atomic terrain transformations for water/vapor flow.
// They must be in PhysicsEngine since they perform direct state modifications.

// Helper: Set vapor structural integrity properties
void PhysicsEngine::setVaporSI(int x, int y, int z, VoxelGrid& voxelGrid) {
    StructuralIntegrityComponent terrainSI =
        voxelGrid.terrainGridRepository->getTerrainStructuralIntegrity(x, y, z);
    terrainSI.canStackEntities = false;
    terrainSI.maxLoadCapacity = -1;
    terrainSI.matterState = MatterState::GAS;
    voxelGrid.terrainGridRepository->setTerrainStructuralIntegrity(x, y, z, terrainSI);
}

// Helper: Check and convert soft empty into water
void PhysicsEngine::checkAndConvertSoftEmptyIntoWater(entt::registry& registry,
                                                      VoxelGrid& voxelGrid, int terrainId, int x,
                                                      int y, int z) {
    if (getTypeAndCheckSoftEmpty(registry, voxelGrid, terrainId, x, y, z)) {
        convertSoftEmptyIntoWater(registry, voxelGrid, terrainId, x, y, z);
    }
}

// Helper: Check and convert soft empty into vapor
void PhysicsEngine::checkAndConvertSoftEmptyIntoVapor(entt::registry& registry,
                                                      VoxelGrid& voxelGrid, int terrainId, int x,
                                                      int y, int z) {
    if (getTypeAndCheckSoftEmpty(registry, voxelGrid, terrainId, x, y, z)) {
        convertSoftEmptyIntoVapor(registry, voxelGrid, terrainId, x, y, z);
    }
}

// Helper: Delete entity or convert to empty
void PhysicsEngine::deleteEntityOrConvertInEmpty(entt::registry& registry,
                                                 entt::dispatcher& dispatcher,
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
