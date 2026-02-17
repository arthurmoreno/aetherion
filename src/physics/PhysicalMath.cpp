
#include "physics/PhysicalMath.hpp"

std::pair<float, bool> calculateVelocityAfterFrictionStep(float velocity, int dt) {
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
