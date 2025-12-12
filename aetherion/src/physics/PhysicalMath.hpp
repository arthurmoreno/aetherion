#ifndef PHYSICAL_MATH_HPP
#define PHYSICAL_MATH_HPP

#include <cmath>
#include <limits>

#include "components/PhysicsComponents.hpp"
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

// Helper: Apply friction to horizontal velocities
inline std::tuple<float, float, bool, bool> applyKineticFrictionDamping(float velocityX,
                                                                        float velocityY,
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

#endif  // PHYSICAL_MATH_HPP