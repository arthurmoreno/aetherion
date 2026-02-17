#ifndef PHYSICAL_MATH_HPP
#define PHYSICAL_MATH_HPP

#include <algorithm>
#include <cmath>
#include <limits>
#include <tuple>

#include "components/PhysicsComponents.hpp"
#include "physics/PhysicsConstants.hpp"
#include "physics/PhysicsManager.hpp"

inline std::tuple<float, float, float> translatePhysicsToGridMovement(
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