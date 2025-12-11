#ifndef PHYSICAL_MATH_HPP
#define PHYSICAL_MATH_HPP

#include <cmath>
#include <limits>

#include "physics/PhysicsConstants.hpp"
#include "physics/PhysicsManager.hpp"

/**
 * @brief Calculate the time required to move 100 units given a 3D velocity vector
 * @param velocityX X component of velocity
 * @param velocityY Y component of velocity
 * @param velocityZ Z component of velocity
 * @return Time to move 100 units as an integer
 */
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

/**
 * @brief Calculate the time required to move 100 units given a 1D velocity
 * @param velocity Single dimensional velocity
 * @return Time to move 100 units as an integer
 */
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

/**
 * @brief Calculate velocity from time to move 100 units
 * @param timeToMove Time required to move 100 units
 * @return Calculated velocity
 */
inline float calculateVelocityFromTime(int timeToMove) {
    // Handle the case where timeToMove is zero or extremely small to avoid division by zero
    if (timeToMove > 0) {
        return 100.0f / static_cast<float>(timeToMove);
    } else {
        return 0.0f;  // Represents infinite time meaning no movement (or stationary)
    }
}

std::pair<float, bool> calculateVelocityAfterFrictionStep(float velocity, int dt);

#endif  // PHYSICAL_MATH_HPP