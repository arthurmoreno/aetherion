#ifndef PHYSICS_UTILS_HPP
#define PHYSICS_UTILS_HPP

#include "components/MovingComponent.hpp"
#include "physics/ReadonlyQueries.hpp"

// Helper: Initialize MovingComponent with movement parameters
MovingComponent initializeMovingComponent(const Position& position, const Velocity& velocity,
                                          int movingToX, int movingToY, int movingToZ,
                                          float completionTime, bool willStopX, bool willStopY,
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

    movingComponent.direction = getDirectionFromVelocities(velocity.vx, velocity.vy, velocity.vz);

    return movingComponent;
};

#endif