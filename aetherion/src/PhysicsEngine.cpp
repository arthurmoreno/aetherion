#include "PhysicsEngine.hpp"

#include <algorithm>
#include <iostream>
#include <random>

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

inline bool hasCollision(entt::registry& registry, VoxelGrid& voxelGrid, entt::entity entity,
                         int movingToX, int movingToY, int movingToZ) {
    bool collision = false;
    // Check if the movement is within bounds for x, y, z
    if ((0 <= movingToX && movingToX < voxelGrid.width) &&
        (0 <= movingToY && movingToY < voxelGrid.height) &&
        (0 <= movingToZ && movingToZ < voxelGrid.depth)) {
        int movingToEntityId = voxelGrid.getEntity(movingToX, movingToY, movingToZ);
        int movingToTerrainId = voxelGrid.getTerrain(movingToX, movingToY, movingToZ);

        bool entityCollision = false;
        if (movingToEntityId != -1) {
            entityCollision = true;
        }

        bool terrainCollision = false;
        if (movingToTerrainId != -1) {
            EntityTypeComponent* etc = registry.try_get<EntityTypeComponent>(entity);
            EntityTypeComponent* movingToEtc =
                registry.try_get<EntityTypeComponent>(static_cast<entt::entity>(movingToTerrainId));
            // Any terrain that is different than water
            if (etc && etc->mainType == static_cast<int>(EntityEnum::TERRAIN)) {
                terrainCollision = true;
            } else if (movingToEtc &&
                       movingToEtc->subType0 != static_cast<int>(TerrainEnum::EMPTY) &&
                       movingToEtc->subType0 != static_cast<int>(TerrainEnum::WATER)) {
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
        int movingToSameZTerrainId = voxelGrid.getTerrain(movingToX, movingToY, movingToZ);
        int movingToBellowTerrainId = voxelGrid.getTerrain(movingToX, movingToY, movingToZ - 1);

        // Check if there is an entity or terrain blocking the destination
        if (movingToSameZTerrainId != -1) {
            auto& etc = registry.get<EntityTypeComponent>(
                static_cast<entt::entity>(movingToSameZTerrainId));
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
        } else if (movingToBellowTerrainId != -1) {
            auto& etc = registry.get<EntityTypeComponent>(
                static_cast<entt::entity>(movingToBellowTerrainId));
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
    int movingToTerrainId = voxelGrid.getTerrain(i, j, k - 1);

    bool canFallOnterrain = false;
    if (movingToTerrainId != -1) {
        auto& etc = registry.get<EntityTypeComponent>(static_cast<entt::entity>(movingToTerrainId));
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

void createMovingComponent(entt::registry& registry, VoxelGrid& voxelGrid, entt::entity entity,
                           Position& position, Velocity& velocity, int movingToX, int movingToY,
                           int movingToZ, float completionTime, bool willStopX, bool willStopY,
                           bool willStopZ) {
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

    DirectionEnum direction;
    if (velocity.vx > 0) {
        direction = DirectionEnum::RIGHT;
    } else if (velocity.vx < 0) {
        direction = DirectionEnum::LEFT;
    } else if (velocity.vy < 0) {
        direction = DirectionEnum::UP;
    } else if (velocity.vy > 0) {
        direction = DirectionEnum::DOWN;
    } else if (velocity.vz > 0) {
        direction = DirectionEnum::UPWARD;
    } else {
        direction = DirectionEnum::DOWNWARD;
    }

    movingComponent.direction = direction;
    registry.emplace<MovingComponent>(entity, movingComponent);

    EntityTypeComponent* etc = registry.try_get<EntityTypeComponent>(entity);
    if (etc && etc->mainType == static_cast<int>(EntityEnum::TERRAIN)) {
        // std::cout << "Setting movingTo positions for terrain moving."
        //     << "moving to: " << movingComponent.movingToX << ", "
        //     << movingComponent.movingToY << ", "
        //     << movingComponent.movingToZ
        //     << "\n";
        voxelGrid.setTerrain(position.x, position.y, position.z, -1);
        voxelGrid.setTerrain(movingComponent.movingToX, movingComponent.movingToY,
                             movingComponent.movingToZ, static_cast<int>(entity));
    } else {
        voxelGrid.setEntity(position.x, position.y, position.z, -1);
        voxelGrid.setEntity(movingComponent.movingToX, movingComponent.movingToY,
                            movingComponent.movingToZ, static_cast<int>(entity));
    }

    position.x = movingComponent.movingToX;
    position.y = movingComponent.movingToY;
    position.z = movingComponent.movingToZ;
}

void handleMovement(entt::registry& registry, VoxelGrid& voxelGrid, entt::entity entity,
                    entt::entity entityBeingDebugged) {
    auto&& [position, velocity, physicsStats] =
        registry.get<Position, Velocity, PhysicsStats>(entity);

    bool haveMovement = registry.all_of<MovingComponent>(entity);

    // ### Applying other forces
    float newVelocityX, newVelocityY, newVelocityZ;
    bool willStopX{false}, willStopY{false}, willStopZ{false};

    std::pair<float, bool> resultX;
    std::pair<float, bool> resultY;
    std::pair<float, bool> resultZ;

    MatterState matterState = MatterState::SOLID;
    StructuralIntegrityComponent* sic = registry.try_get<StructuralIntegrityComponent>(entity);
    if (sic) {
        matterState = sic->matterState;
    }

    if (matterState == MatterState::SOLID || matterState == MatterState::LIQUID) {
        if (entity == entityBeingDebugged) {
            std::cout << "handleMovement -> applying Gravity" << std::endl;
        }
        resultZ =
            applyGravity(registry, voxelGrid, position.x, position.y, position.z, velocity.vz, 1);
        newVelocityZ = resultZ.first;
        resultZ =
            applyGravity(registry, voxelGrid, position.x, position.y, position.z, velocity.vz, 2);
        willStopZ = resultZ.second;
    } else {
        newVelocityZ = velocity.vz;
    }

    int bellowEntityId = voxelGrid.getEntity(position.x, position.y, position.z - 1);
    int bellowTerrainId = voxelGrid.getTerrain(position.x, position.y, position.z - 1);

    bool bellowIsStable = false;
    if (bellowEntityId != -1) {
        entt::entity bellowEntity = static_cast<entt::entity>(bellowEntityId);
        StructuralIntegrityComponent* bellowEntitySic =
            registry.try_get<StructuralIntegrityComponent>(bellowEntity);
        if (bellowEntitySic && bellowEntitySic->canStackEntities) {
            bellowIsStable = true;
        }

    } else if (bellowTerrainId != -1) {
        entt::entity bellowTerrain = static_cast<entt::entity>(bellowTerrainId);
        StructuralIntegrityComponent* bellowTerrainSic =
            registry.try_get<StructuralIntegrityComponent>(bellowTerrain);
        if (bellowTerrainSic && bellowTerrainSic->canStackEntities) {
            bellowIsStable = true;
        }
    }

    if (matterState == MatterState::SOLID && bellowIsStable && newVelocityZ <= 0) {
        resultX = applyFrictionToVelocity(velocity.vx, 1);
        newVelocityX = resultX.first;

        resultY = applyFrictionToVelocity(velocity.vy, 1);
        newVelocityY = resultY.first;
        // newVelocityZ = velocity.vz;

        resultX = applyFrictionToVelocity(velocity.vx, 2);
        willStopX = resultX.second;

        resultY = applyFrictionToVelocity(velocity.vy, 2);
        willStopY = resultY.second;
    } else {
        newVelocityX = velocity.vx;
        newVelocityY = velocity.vy;
        willStopX = false;
        willStopY = false;
    }

    velocity.vx = newVelocityX;
    velocity.vy = newVelocityY;
    velocity.vz = newVelocityZ;

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

    bool collision = hasCollision(registry, voxelGrid, entity, movingToX, movingToY, movingToZ);

    // if (matterState == MatterState::GAS) {
    //     std::cout << "Gas"
    //         << " position: " << position.x << " " << position.y << " " << position.z
    //         << " movingTo: " << movingToX << " " << movingToY << " " << movingToZ
    //         << " hasCollision: " << collision
    //         << " completionTime: " << completionTime
    //         << " haveMovement: " << haveMovement
    //         << std::endl;
    // }

    if (!collision && completionTime < calculateTimeToMove(physicsStats.minSpeed)) {
        if (!haveMovement) {
            createMovingComponent(registry, voxelGrid, entity, position, velocity, movingToX,
                                  movingToY, movingToZ, completionTime, willStopX, willStopY,
                                  willStopZ);
        }

    } else {
        bool lateralCollision = false;
        if (getDirection(newVelocityX) != 0) {
            lateralCollision = true;
            velocity.vx = 0;
        }
        if (getDirection(newVelocityY) != 0) {
            lateralCollision = true;
            velocity.vy = 0;
        }
        if (lateralCollision && !specialCollision) {
            // velocity.vz = newVelocityZ;
            bool collisionZ = false;
            movingToX = position.x;
            movingToY = position.y;
            // Check if the movement is within bounds for x, y, z
            if ((0 <= movingToX && movingToX < voxelGrid.width) &&
                (0 <= movingToY && movingToY < voxelGrid.height) &&
                (0 <= movingToZ && movingToZ < voxelGrid.depth)) {
                int movingToEntityId = voxelGrid.getEntity(movingToX, movingToY, movingToZ);
                int movingToTerrainId = voxelGrid.getTerrain(movingToX, movingToY, movingToZ);

                // Check if there is an entity or terrain blocking the destination
                if (movingToEntityId != -1 || movingToTerrainId != -1) {
                    collisionZ = true;
                }
            } else {
                // Out of bounds collisionZ with the world boundary
                collisionZ = true;
            }

            if (!collisionZ && !haveMovement) {
                velocity.vz = newVelocityZ;

                createMovingComponent(registry, voxelGrid, entity, position, velocity, movingToX,
                                      movingToY, movingToZ, completionTime, willStopX, willStopY,
                                      willStopZ);
            } else {
                velocity.vz = 0;
            }

        } else {
            velocity.vz = 0;
        }

        // resultZ = applyGravity(voxelGrid, position.x, position.y, position.z, velocity.vz, 1);
        // newVelocityZ = resultZ.first;
        // velocity.vz = newVelocityZ;

        if (velocity.vx == 0 && velocity.vy == 0 && velocity.vz == 0) {
            registry.remove<Velocity>(entity);
        }
    }
}

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
        handleMovement(registry, voxelGrid, entity, entityBeingDebugged);
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

DirectionEnum getDirectionFromVelocities(float velocityX, float velocityY, float velocityZ) {
    // Update direction based on new velocities
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

void PhysicsEngine::onMoveGasEntityEvent(const MoveGasEntityEvent& event) {
    // spdlog::get("console")->debug("onMoveGasEntityEvent -> entered");
    // std::ostringstream ossMessage;

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

        if (event.entity == entityBeingDebugged) {
            std::cout << "onMoveGasEntityEvent ->" << " rhoEnv: " << event.rhoEnv
                      << " rhoGas: " << event.rhoGas << std::endl;
        }

        float gravity = PhysicsManager::Instance()->getGravity();
        float accelerationX = static_cast<float>(event.forceX) / physicsStats.mass;
        float accelerationY = static_cast<float>(event.forceY) / physicsStats.mass;
        float accelerationZ = 0.0f;
        if (event.rhoEnv > 0.0f && event.rhoGas > 0.0f) {
            accelerationZ = ((event.rhoEnv - event.rhoGas) * gravity) / event.rhoGas;
        }

        if (event.entity == entityBeingDebugged) {
            std::cout << "onMoveGasEntityEvent ->" << " Acceleration X: " << accelerationX
                      << " Acceleration Y: " << accelerationY
                      << " Acceleration Z: " << accelerationZ << std::endl;
        }
        // spdlog::get("console")->debug(ossMessage.str());
        // ossMessage.str("");
        // ossMessage.clear();

        // Translate physics to grid movement
        float newVelocityX, newVelocityY, newVelocityZ;
        std::tie(newVelocityX, newVelocityY, newVelocityZ) =
            translatePhysicsToGridMovement(velocity.vx, velocity.vy, velocity.vz, accelerationX,
                                           accelerationY, accelerationZ, physicsStats.maxSpeed);

        if (!haveMovement || event.forceApplyNewVelocity) {
            if (event.entity == entityBeingDebugged) {
                std::cout << "onMoveGasEntityEvent -> entered to apply newVelocity: "
                          << " newVelocityX: " << newVelocityX << " newVelocityY: " << newVelocityY
                          << " newVelocityZ: " << newVelocityZ << std::endl;
            }
            velocity.vx = newVelocityX;
            velocity.vy = newVelocityY;
            velocity.vz = newVelocityZ;
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
    // spdlog::get("console")->debug("onMoveSolidEntityEvent -> entered");

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

// Register event handlers
void PhysicsEngine::registerEventHandlers(entt::dispatcher& dispatcher) {
    dispatcher.sink<MoveGasEntityEvent>().connect<&PhysicsEngine::onMoveGasEntityEvent>(*this);
    dispatcher.sink<MoveSolidEntityEvent>().connect<&PhysicsEngine::onMoveSolidEntityEvent>(*this);
    dispatcher.sink<TakeItemEvent>().connect<&PhysicsEngine::onTakeItemEvent>(*this);
    dispatcher.sink<UseItemEvent>().connect<&PhysicsEngine::onUseItemEvent>(*this);
    dispatcher.sink<SetPhysicsEntityToDebug>().connect<&PhysicsEngine::onSetPhysicsEntityToDebug>(
        *this);
}