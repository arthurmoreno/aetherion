#include "PhysicsEngine.hpp"

#include <algorithm>
#include <iostream>
#include <random>
#include <sstream>

#include "EcosystemEngine.hpp"
#include "settings.hpp"

bool PhysicsEngine::checkIfCanJump(const MoveSolidEntityEvent& event) {
    // Implement the logic to determine if the entity can jump
    // Placeholder implementation:
    return true;
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

inline int calculateTimeToMove(float velocityX, float velocityY, float velocityZ) {
    // Compute the squared velocity components
    float velocityXSq = velocityX * velocityX;
    float velocityYSq = velocityY * velocityY;
    float velocityZSq = velocityZ * velocityZ;

    // Calculate the magnitude of the velocity vector
    float velocityMagnitude = std::sqrt(velocityXSq + velocityYSq + velocityZSq);

    float timeToMove;

    // Avoid division by zero using epsilon
    if (velocityMagnitude > std::numeric_limits<float>::epsilon()) {
        timeToMove = 100.0f / velocityMagnitude;  // Time to move 100 units
    } else {
        timeToMove = std::numeric_limits<float>::max();  // Represents no movement
    }

    return static_cast<int>(timeToMove);
}

inline int calculateTimeToMove(float velocity) {
    float timeToMove;

    // Avoid division by zero using epsilon
    if (std::abs(velocity) > std::numeric_limits<float>::epsilon()) {
        timeToMove = 100.0f / std::abs(velocity);  // Time to move 100 units
    } else {
        timeToMove = std::numeric_limits<float>::max();  // Represents no movement
    }

    return static_cast<int>(timeToMove);
}

inline float calculateVelocityFromTime(int timeToMove) {
    // Handle the case where timeToMove is zero or extremely small to avoid division by zero
    if (timeToMove > 0) {
        return 100.0f / static_cast<float>(timeToMove);
    } else {
        return 0.0f;  // Represents infinite time meaning no movement (or stationary)
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

inline bool hasCollision(entt::registry& registry, VoxelGrid& voxelGrid, entt::entity entity,
                         int movingFromX, int movingFromY, int movingFromZ,
                         int movingToX, int movingToY, int movingToZ, bool isTerrain) {
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

std::pair<float, bool> applyFrictionToVelocity(float velocity, int dt) {
    // Determine the direction of movement
    int movingDirection = 0;
    if (velocity > 0.0f) {
        movingDirection = 1;
    } else if (velocity < 0.0f) {
        movingDirection = -1;
    }

    float mu;
    float newVelocity;

    if (GRAVITY_FRICTION) {
        float gravity = PhysicsManager::Instance()->getGravity();
        mu = PhysicsManager::Instance()->getFriction();
        float frictionAcceleration = mu * gravity;
        // Convert 'dt' to float for accurate calculation
        newVelocity = velocity - (frictionAcceleration * static_cast<float>(movingDirection) *
                                  static_cast<float>(dt));
    } else {
        mu = 0.02f;
        newVelocity = velocity * mu;
    }

    // Determine if the object will stop
    bool willStop = false;
    if (velocity * newVelocity < 0.0f) {
        newVelocity = 0.0f;
        willStop = true;
    }

    return std::make_pair(newVelocity, willStop);
}

bool checkIfCanFall(entt::registry& registry, VoxelGrid& voxelGrid, int i, int j, int k) {
    // return false;

    int movingToEntityId = voxelGrid.getEntity(i, j, k - 1);
    bool canFallOnterrain = false;
    if (voxelGrid.checkIfTerrainExists(i, j, k - 1)) {
        EntityTypeComponent etc =
            voxelGrid.terrainGridRepository->getTerrainEntityType(i, j, k - 1);
        // Any terrain that is different than water
        if (etc.subType0 == 1) {
            canFallOnterrain = true;
        }
    } else {
        canFallOnterrain = true;
    }

    return (k > 0 and movingToEntityId == -1 and canFallOnterrain);
}

std::pair<float, bool> applyGravity(entt::registry& registry, VoxelGrid& voxelGrid, int i, int j,
                                    int k, float velocityZ, int dt) {
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

// Helper function to determine direction
int getDirection(float velocity) {
    if (velocity > 0.0f) {
        return 1;
    } else if (velocity < 0.0f) {
        return -1;
    } else {
        return 0;
    }
}

// ================ createMovingComponent functions helpers ================

// Helper: Get DirectionEnum from velocities
DirectionEnum getDirectionFromVelocities(float velocityX, float velocityY, float velocityZ) {
    DirectionEnum direction;
    if (velocityX > 0) {
        direction = DirectionEnum::RIGHT;
    } else if (velocityX < 0) {
        direction = DirectionEnum::LEFT;
    } else if (velocityY < 0) {
        direction = DirectionEnum::UP;
    } else if (velocityY > 0) {
        direction = DirectionEnum::DOWN;
    } else if (velocityZ > 0) {
        direction = DirectionEnum::UPWARD;
    } else {
        direction = DirectionEnum::DOWNWARD;
    }

    return direction;
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

// Helper: Validate terrain entity ID
inline void validateTerrainEntityId(entt::entity entity) {
    int entityId = static_cast<int>(entity);
    if (entityId == -1 || entityId == -2) {
        throw std::runtime_error("Invalid terrain entity ID in createMovingComponent");
    }
}

// Helper: Apply terrain movement in VoxelGrid
inline void applyTerrainMovement(VoxelGrid& voxelGrid, entt::entity entity,
                                 const MovingComponent& movingComponent) {
    std::cout << "Setting movingTo positions for terrain moving."
              << "moving to: " << movingComponent.movingToX << ", " << movingComponent.movingToY
              << ", " << movingComponent.movingToZ << "\n";

    validateTerrainEntityId(entity);
    voxelGrid.terrainGridRepository->moveTerrain(
        const_cast<MovingComponent&>(movingComponent));
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

// Helper: Update position to destination
inline void updatePositionToDestination(Position& position, const MovingComponent& movingComponent) {
    position.x = movingComponent.movingToX;
    position.y = movingComponent.movingToY;
    position.z = movingComponent.movingToZ;
}

// Main function: Create and apply movement component
void createMovingComponent(entt::registry& registry, entt::dispatcher& dispatcher,
                           VoxelGrid& voxelGrid, entt::entity entity, Position& position,
                           Velocity& velocity, int movingToX, int movingToY, int movingToZ,
                           float completionTime, bool willStopX, bool willStopY, bool willStopZ,
                           bool isTerrain) {
    MovingComponent movingComponent = initializeMovingComponent(
        position, velocity, movingToX, movingToY, movingToZ, completionTime, willStopX, willStopY,
        willStopZ);

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

// =========================================================================
// ================ handleMovement functions helpers ================

// Helper: Load entity data (Position, Velocity, PhysicsStats) from either ECS or terrain storage
// ATOMIC: For terrain, uses getPhysicsSnapshot() to read all data under a single lock
inline std::tuple<Position&, Velocity&, PhysicsStats&> loadEntityPhysicsData(
    entt::registry& registry, VoxelGrid& voxelGrid, entt::entity entity, bool isTerrain,
    Position& terrainPos, Velocity& terrainVel, PhysicsStats& terrainPS) {
    if (isTerrain) {
        std::cout << "[loadEntityPhysicsData] Loading terrain physics data for entity ID=" << int(entity) << "\n";
        if (!registry.valid(entity)) {
            throw std::runtime_error("Invalid terrain entity");
        }

        terrainPos = voxelGrid.terrainGridRepository->getPositionOfEntt(entity);

        if (!voxelGrid.checkIfTerrainExists(terrainPos.x, terrainPos.y, terrainPos.z)) {
            throw std::runtime_error("Terrain does not exist at the given position");
        }

        terrainVel = voxelGrid.terrainGridRepository->getVelocity(terrainPos.x, terrainPos.y,
                                                                   terrainPos.z);
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

// Helper: Get matter state for entity or terrain
inline MatterState getMatterState(entt::registry& registry, VoxelGrid& voxelGrid,
                                  entt::entity entity, const Position& position, bool isTerrain) {
    if (!isTerrain) {
        StructuralIntegrityComponent* sic =
            registry.try_get<StructuralIntegrityComponent>(entity);
        if (sic) {
            return sic->matterState;
        }
        return MatterState::SOLID;
    } else {
        StructuralIntegrityComponent bellowTerrainSic =
            voxelGrid.terrainGridRepository->getTerrainStructuralIntegrity(
                position.x, position.y, position.z - 1);
        return bellowTerrainSic.matterState;
    }
}

// Helper: Apply gravity and get new Z velocity
inline std::pair<float, bool> applyGravityForces(entt::registry& registry, VoxelGrid& voxelGrid,
                                                  const Position& position, float velocityZ,
                                                  MatterState matterState,
                                                  entt::entity entityBeingDebugged,
                                                  entt::entity entity) {
    if (matterState == MatterState::SOLID || matterState == MatterState::LIQUID) {
        if (entity == entityBeingDebugged) {
            std::cout << "handleMovement -> applying Gravity" << std::endl;
        }
        auto resultZ =
            applyGravity(registry, voxelGrid, position.x, position.y, position.z, velocityZ, 1);
        float newVelocityZ = resultZ.first;
        resultZ =
            applyGravity(registry, voxelGrid, position.x, position.y, position.z, velocityZ, 2);
        bool willStopZ = resultZ.second;
        return {newVelocityZ, willStopZ};
    }
    return {velocityZ, false};
}

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
            voxelGrid.terrainGridRepository->getTerrainStructuralIntegrity(
                position.x, position.y, position.z - 1);
        return bellowTerrainSic.canStackEntities;
    }
    return false;
}

// Helper: Apply friction to horizontal velocities
inline std::tuple<float, float, bool, bool> applyFrictionForces(float velocityX, float velocityY,
                                                                 MatterState matterState,
                                                                 bool bellowIsStable,
                                                                 float newVelocityZ) {
    if (matterState == MatterState::SOLID && bellowIsStable && newVelocityZ <= 0) {
        auto resultX = applyFrictionToVelocity(velocityX, 1);
        float newVelocityX = resultX.first;

        auto resultY = applyFrictionToVelocity(velocityY, 1);
        float newVelocityY = resultY.first;

        resultX = applyFrictionToVelocity(velocityX, 2);
        bool willStopX = resultX.second;

        resultY = applyFrictionToVelocity(velocityY, 2);
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
    int movingToX = position.x + getDirection(newVelocityX);
    int movingToY = position.y + getDirection(newVelocityY);
    int movingToZ = position.z + getDirection(newVelocityZ);

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
    if (getDirection(newVelocityX) != 0) {
        lateralCollision = true;
        velocity.vx = 0;
    }
    if (getDirection(newVelocityY) != 0) {
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
    int movingToZ = position.z + getDirection(newVelocityZ);

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

// Helper: Clean up zero velocity
inline void cleanupZeroVelocity(entt::registry& registry, VoxelGrid& voxelGrid,
                                entt::entity entity, const Position& position,
                                const Velocity& velocity, bool isTerrain) {
    if (velocity.vx == 0 && velocity.vy == 0 && velocity.vz == 0) {
        if (isTerrain) {
            std::cout << "[handleMovement] Deleting Velocity from Terrain!\n";
            voxelGrid.terrainGridRepository->setVelocity(position.x, position.y, position.z,
                                                         {0.0f, 0.0f, 0.0f});
        } else {
            registry.remove<Velocity>(entity);
        }
    }
}

// Main function: Handle entity movement with physics
void handleMovement(entt::registry& registry, entt::dispatcher& dispatcher, VoxelGrid& voxelGrid,
                    entt::entity entity, entt::entity entityBeingDebugged, bool isTerrain) {

    if (isTerrain) {
        std::cout << "[handleMovement] Handling terrain entity ID=" << int(entity) << "\n";
    }

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

        auto [newVelocityZ, willStopZ] = applyGravityForces(registry, voxelGrid, position, velocity.vz,
                                                             matterState, entityBeingDebugged, entity);

        // Check stability below entity and apply friction
        bool bellowIsStable = checkBelowStability(registry, voxelGrid, position);

        auto [newVelocityX, newVelocityY, willStopX, willStopY] =
            applyFrictionForces(velocity.vx, velocity.vy, matterState, bellowIsStable, newVelocityZ);

        // Update velocities
        velocity.vx = newVelocityX;
        velocity.vy = newVelocityY;
        velocity.vz = newVelocityZ;

        // Calculate movement destination with special collision handling
        auto [movingToX, movingToY, movingToZ, completionTime] = calculateMovementDestination(
            registry, voxelGrid, position, velocity, physicsStats, newVelocityX, newVelocityY,
            newVelocityZ);

        // NOTE: Terrain grid lock already acquired above (line ~741) for terrain entities
        
        // Check collision and handle movement
        bool collision = hasCollision(registry, voxelGrid, entity, position.x, position.y, position.z, movingToX, movingToY, movingToZ, isTerrain);

        if (!collision && completionTime < calculateTimeToMove(physicsStats.minSpeed)) {
            if (!haveMovement) {
                createMovingComponent(registry, dispatcher, voxelGrid, entity, position, velocity,
                                      movingToX, movingToY, movingToZ, completionTime, willStopX,
                                      willStopY, willStopZ, isTerrain);
            }
        } else {
            // Handle lateral collision and try Z-axis movement
            bool handled = handleLateralCollision(registry, dispatcher, voxelGrid, entity, position,
                                                   velocity, newVelocityX, newVelocityY, newVelocityZ,
                                                   completionTime, willStopX, willStopY, willStopZ,
                                                   haveMovement, isTerrain);

            if (!handled) {
                velocity.vz = 0;
            }

            // Clean up zero velocity
            cleanupZeroVelocity(registry, voxelGrid, entity, position, velocity, isTerrain);
        }
    } catch (...) {
        // CRITICAL: Release lock before re-throwing exception
        if (isTerrain) {
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
            resultZ = applyGravity(registry, voxelGrid, position.x, position.y, position.z,
                                   velocity.vz, 1);
            newVelocityZ = resultZ.first;
            resultZ = applyGravity(registry, voxelGrid, position.x, position.y, position.z,
                                   velocity.vz, 2);
            willStopZ = resultZ.second;

            velocity.vz = newVelocityZ;
        }

        if (!hasVelocity) {
            registry.emplace<Velocity>(entity, velocity);
        }

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
        Position pos = registry.get<Position>(entity);
        int entityId = static_cast<int>(entity);
        int entityVoxelGridId = voxelGrid.getEntity(pos.x, pos.y, pos.z);
        if (entityId == entityVoxelGridId) {
            handleMovement(registry, dispatcher, voxelGrid, entity, entityBeingDebugged, false);
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
    // std::cout << "onMoveGasEntityEvent -> entered" << std::endl;
    // std::cout << "onMoveGasEntityEvent: terrainId at position (" << event.position.x << ", "
    //     << event.position.y << ", " << event.position.z << ") " << std::endl;
    int terrainId = static_cast<int>(TerrainIdTypeEnum::NONE);
    if (voxelGrid == nullptr) {
        // Not initialized yet
        // std::cout << "onMoveGasEntityEvent: voxelGrid is null" << std::endl;
        throw std::runtime_error(
            "onMoveGasEntityEvent: Exception thrown for testing purposes. voxelGrid is null.");
    } else {
        // std::cout << "onMoveGasEntityEvent: voxelGrid is not null" << std::endl;
        terrainId = voxelGrid->getTerrain(event.position.x, event.position.y, event.position.z);
        // std::cout << "onMoveGasEntityEvent: terrainId at position (" << event.position.x << ", "
        //   << event.position.y << ", " << event.position.z << ") is " << terrainId << std::endl;
        // throw std::runtime_error("onMoveGasEntityEvent: Exception thrown for testing purposes.
        // voxelGrid is not null.");
    }
    // bool terrainExists = voxelGrid->checkIfTerrainExists(event.position.x, event.position.y,
    // event.position.z); std::cout << "onMoveGasEntityEvent: terrainExists: " << terrainExists <<
    // std::endl; int terrainId = voxelGrid->getTerrain(event.position.x, event.position.y,
    // event.position.z); std::cout << "onMoveGasEntityEvent: terrainId at position (" <<
    // event.position.x << ", "
    //           << event.position.y << ", " << event.position.z << ") is " << terrainId <<
    //           std::endl;

    if (terrainId != static_cast<int>(TerrainIdTypeEnum::NONE)) {
        Position pos = voxelGrid->terrainGridRepository->getPosition(
            event.position.x, event.position.y, event.position.z);
        EntityTypeComponent etc =
            voxelGrid->terrainGridRepository->getTerrainEntityType(pos.x, pos.y, pos.z);
        PhysicsStats physicsStats =
            voxelGrid->terrainGridRepository->getPhysicsStats(pos.x, pos.y, pos.z);
        // }
        // if (registry.valid(event.entity) &&
        //     registry.all_of<Position, EntityTypeComponent, PhysicsStats>(event.entity)) {
        //     auto&& [pos, type, physicsStats] =
        //         registry.get<Position, EntityTypeComponent, PhysicsStats>(event.entity);

        // Attempt to retrieve the optional Velocity component
        // std::cout << "onMoveGasEntityEvent: Just before checks for entity " <<
        // static_cast<int>(event.entity) << std::endl;
        bool hasEntity =
            event.entity != entt::null &&
            static_cast<int>(event.entity) != static_cast<int>(TerrainIdTypeEnum::NONE) &&
            static_cast<int>(event.entity) != static_cast<int>(TerrainIdTypeEnum::ON_GRID_STORAGE);
        bool haveMovement = registry.all_of<MovingComponent>(event.entity);
        bool hasEnTTPosition = registry.all_of<Position>(event.entity);
        if (!hasEntity) {
            std::cout << "onMoveGasEntityEvent: event.entity is null or invalid (Either none or on "
                         "grid) :"
                      << static_cast<int>(event.entity) << std::endl;
            return;
        }
        if (!hasEnTTPosition) {
            std::cout << "onMoveGasEntityEvent: entity does not have Position component. Emplacing "
                         "Position component."
                      << std::endl;
            registry.emplace<Position>(event.entity, pos);
            // throw std::runtime_error("onMoveGasEntityEvent: Entity does not have Position
            // component");
        }
        // if (!hasVelocity) {
        //     Velocity velocity = {0.0f, 0.0f, 0.0f};
        //     voxelGrid->terrainGridRepository->setVelocity(pos.x, pos.y, pos.z, velocity);
        // }
        Velocity velocity = voxelGrid->terrainGridRepository->getVelocity(pos.x, pos.y, pos.z);

        // std::cout << "onMoveGasEntityEvent: Retrieved Velocity component for entity " <<
        // velocity.vx << ", "
        //           << velocity.vy << ", " << velocity.vz << std::endl;

        // if (event.entity == entityBeingDebugged) {
        //     std::cout << "onMoveGasEntityEvent ->" << " rhoEnv: " << event.rhoEnv
        //               << " rhoGas: " << event.rhoGas << std::endl;
        // }

        float gravity = PhysicsManager::Instance()->getGravity();
        float accelerationX = static_cast<float>(event.forceX) / physicsStats.mass;
        float accelerationY = static_cast<float>(event.forceY) / physicsStats.mass;
        float accelerationZ = 0.0f;
        if (event.rhoEnv > 0.0f && event.rhoGas > 0.0f) {
            accelerationZ = ((event.rhoEnv - event.rhoGas) * gravity) / event.rhoGas;
        }

        // if (event.entity == entityBeingDebugged) {
        //     std::cout << "onMoveGasEntityEvent ->" << " Acceleration X: " << accelerationX
        //               << " Acceleration Y: " << accelerationY
        //               << " Acceleration Z: " << accelerationZ << std::endl;
        // }
        // spdlog::get("console")->debug(ossMessage.str());
        // ossMessage.str("");
        // ossMessage.clear();

        // Translate physics to grid movement
        float newVelocityX, newVelocityY, newVelocityZ;
        std::tie(newVelocityX, newVelocityY, newVelocityZ) =
            translatePhysicsToGridMovement(velocity.vx, velocity.vy, velocity.vz, accelerationX,
                                           accelerationY, accelerationZ, physicsStats.maxSpeed);

        if (!haveMovement || event.forceApplyNewVelocity) {
            // if (event.entity == entityBeingDebugged) {
            std::cout << "onMoveGasEntityEvent -> entered to apply newVelocity: "
                      << " newVelocityX: " << newVelocityX << " newVelocityY: " << newVelocityY
                      << " newVelocityZ: " << newVelocityZ << std::endl;
            // }
            velocity.vx = newVelocityX;
            velocity.vy = newVelocityY;
            velocity.vz = newVelocityZ;
            voxelGrid->terrainGridRepository->setVelocity(pos.x, pos.y, pos.z, velocity);
            // registry.emplace<Velocity>(event.entity, velocity);
        }

        // ossMessage << "onMoveGasEntityEvent -> velocity X: " << velocity.vx <<
        //     " -> velocity Y: " << velocity.vy <<
        //     " -> velocity Z: " << velocity.vz;
        // spdlog::get("console")->debug(ossMessage.str());
        // ossMessage.str("");
        // ossMessage.clear();
    } else {
        std::cout << "onMoveGasEntityEvent: entity does not have components" << std::endl;
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

// Water evaporation event handler - moved from EcosystemEngine
void PhysicsEngine::onEvaporateWaterEntityEvent(const EvaporateWaterEntityEvent& event) {
    int terrainId = voxelGrid->getTerrain(event.position.x, event.position.y, event.position.z);
    if (terrainId == static_cast<int>(TerrainIdTypeEnum::NONE)) {
        return;  // No terrain to evaporate from
    }

    Position pos = voxelGrid->terrainGridRepository->getPosition(
        event.position.x, event.position.y, event.position.z);
    EntityTypeComponent type = voxelGrid->terrainGridRepository->getTerrainEntityType(
        event.position.x, event.position.y, event.position.z);
    MatterContainer matterContainer =
        voxelGrid->terrainGridRepository->getTerrainMatterContainer(
            event.position.x, event.position.y, event.position.z);

    // Validate physics constraints
    bool canEvaporate = (event.sunIntensity > 0.0f &&
                         type.mainType == static_cast<int>(EntityEnum::TERRAIN) &&
                         (type.subType0 == static_cast<int>(TerrainEnum::WATER) ||
                          type.subType0 == static_cast<int>(TerrainEnum::GRASS)) &&
                         matterContainer.WaterMatter > 0);

    if (canEvaporate) {
        // Lock terrain grid for atomic state change
        voxelGrid->terrainGridRepository->lockTerrainGrid();

        int waterEvaporated = 1;
        matterContainer.WaterMatter -= waterEvaporated;
        voxelGrid->terrainGridRepository->setTerrainMatterContainer(pos.x, pos.y, pos.z,
                                                                    matterContainer);

        // Create or add vapor on z+1
        createOrAddVaporPhysics(registry, *voxelGrid, pos.x, pos.y, pos.z, waterEvaporated);

        voxelGrid->terrainGridRepository->unlockTerrainGrid();
    }
}

// Water condensation event handler - moved from EcosystemEngine
void PhysicsEngine::onCondenseWaterEntityEvent(const CondenseWaterEntityEvent& event) {
    if (!registry.valid(event.entity) ||
        !registry.all_of<Position, EntityTypeComponent, MatterContainer>(event.entity)) {
        return;
    }

    auto&& [pos, type, matterContainer] =
        registry.get<Position, EntityTypeComponent, MatterContainer>(event.entity);

    int terrainBelowId = voxelGrid->getTerrain(pos.x, pos.y, pos.z - 1);
    if (terrainBelowId == -1) {
        // Lock for atomic state change
        voxelGrid->terrainGridRepository->lockTerrainGrid();

        // Create a new water tile below
        entt::entity newWaterEntity = registry.create();
        Position newPosition = {pos.x, pos.y, pos.z - 1, DirectionEnum::DOWN};

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

        voxelGrid->setTerrain(pos.x, pos.y, pos.z - 1, static_cast<int>(newWaterEntity));

        matterContainer.WaterVapor -= event.condensationAmount;

        if (matterContainer.WaterVapor <= 0) {
            voxelGrid->setTerrain(pos.x, pos.y, pos.z, -1);
            registry.destroy(event.entity);
        }

        voxelGrid->terrainGridRepository->unlockTerrainGrid();
    }
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
    voxelGrid->terrainGridRepository->setTerrainMatterContainer(
        event.target.x, event.target.y, event.target.z, targetMatter);
    voxelGrid->terrainGridRepository->setTerrainMatterContainer(
        event.source.x, event.source.y, event.source.z, sourceMatter);

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
    voxelGrid->terrainGridRepository->setTerrainMatterContainer(
        event.target.x, event.target.y, event.target.z, targetMatter);
    voxelGrid->terrainGridRepository->setTerrainMatterContainer(
        event.source.x, event.source.y, event.source.z, sourceMatter);

    voxelGrid->terrainGridRepository->unlockTerrainGrid();
}

void PhysicsEngine::onTerrainPhaseConversionEvent(const TerrainPhaseConversionEvent& event) {
    // Lock terrain grid for atomic state change
    voxelGrid->terrainGridRepository->lockTerrainGrid();

    // Apply terrain phase conversion (e.g., soft-empty -> water/vapor)
    voxelGrid->terrainGridRepository->setTerrainEntityType(
        event.position.x, event.position.y, event.position.z, event.newType);
    voxelGrid->terrainGridRepository->setTerrainMatterContainer(
        event.position.x, event.position.y, event.position.z, event.newMatter);
    voxelGrid->terrainGridRepository->setTerrainStructuralIntegrity(
        event.position.x, event.position.y, event.position.z, event.newStructuralIntegrity);

    voxelGrid->terrainGridRepository->unlockTerrainGrid();
}

void PhysicsEngine::onVaporCreationEvent(const VaporCreationEvent& event) {
    // Reuse existing helper function which already has proper locking
    createOrAddVaporPhysics(registry, *voxelGrid, event.position.x, event.position.y, 
                           event.position.z, event.amount);
}

void PhysicsEngine::onVaporMergeUpEvent(const VaporMergeUpEvent& event) {
    // Lock terrain grid for atomic state change
    voxelGrid->terrainGridRepository->lockTerrainGrid();

    // Get target vapor and merge
    MatterContainer targetMatter = voxelGrid->terrainGridRepository->getTerrainMatterContainer(
        event.target.x, event.target.y, event.target.z);
    targetMatter.WaterVapor += event.amount;
    voxelGrid->terrainGridRepository->setTerrainMatterContainer(
        event.target.x, event.target.y, event.target.z, targetMatter);

    // Clear source vapor
    MatterContainer sourceMatter = voxelGrid->terrainGridRepository->getTerrainMatterContainer(
        event.source.x, event.source.y, event.source.z);
    sourceMatter.WaterVapor = 0;
    voxelGrid->terrainGridRepository->setTerrainMatterContainer(
        event.source.x, event.source.y, event.source.z, sourceMatter);

    voxelGrid->terrainGridRepository->unlockTerrainGrid();

    // Delete or convert source entity after unlocking
    if (registry.valid(event.sourceEntity)) {
        dispatcher.enqueue<KillEntityEvent>(event.sourceEntity);
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
    dispatcher.sink<EvaporateWaterEntityEvent>().connect<&PhysicsEngine::onEvaporateWaterEntityEvent>(*this);
    dispatcher.sink<CondenseWaterEntityEvent>().connect<&PhysicsEngine::onCondenseWaterEntityEvent>(*this);
    dispatcher.sink<WaterFallEntityEvent>().connect<&PhysicsEngine::onWaterFallEntityEvent>(*this);

    // Register water flow event handlers (new architecture)
    dispatcher.sink<WaterSpreadEvent>().connect<&PhysicsEngine::onWaterSpreadEvent>(*this);
    dispatcher.sink<WaterGravityFlowEvent>().connect<&PhysicsEngine::onWaterGravityFlowEvent>(*this);
    dispatcher.sink<TerrainPhaseConversionEvent>().connect<&PhysicsEngine::onTerrainPhaseConversionEvent>(*this);
    
    // Register vapor event handlers
    dispatcher.sink<VaporCreationEvent>().connect<&PhysicsEngine::onVaporCreationEvent>(*this);
    dispatcher.sink<VaporMergeUpEvent>().connect<&PhysicsEngine::onVaporMergeUpEvent>(*this);
}