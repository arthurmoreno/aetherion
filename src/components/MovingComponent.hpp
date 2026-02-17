#ifndef MOVING_COMPONENT_HPP
#define MOVING_COMPONENT_HPP

#include "components/PhysicsComponents.hpp"

struct MovingComponent {
    bool isMoving;

    int movingFromX;
    int movingFromY;
    int movingFromZ;

    int movingToX;
    int movingToY;
    int movingToZ;

    float vx;
    float vy;
    float vz;

    bool willStopX;
    bool willStopY;
    bool willStopZ;

    int completionTime;
    int timeRemaining;

    DirectionEnum direction;
};

#endif  // MOVING_COMPONENT_HPP