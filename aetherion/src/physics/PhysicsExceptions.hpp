#pragma once

#include <stdexcept>
#include <string>

namespace aetherion {

/**
 * @brief Base exception class for all physics-related errors
 *
 * This serves as the parent class for all custom physics exceptions,
 * allowing callers to catch all physics exceptions with a single catch block.
 */
class PhysicsException : public std::runtime_error {
   public:
    using std::runtime_error::runtime_error;
};

/**
 * @brief Exception thrown when terrain grid locking operations fail
 *
 * This exception indicates issues with acquiring, releasing, or managing
 * terrain grid locks, typically in concurrent physics operations.
 */
class TerrainLockException : public PhysicsException {
   public:
    using PhysicsException::PhysicsException;
};

/**
 * @brief Exception thrown when an entity is in an invalid state
 *
 * This exception is thrown when operations are attempted on entities that
 * are not properly initialized, have been destroyed, or are missing required
 * components for physics processing.
 */
class InvalidEntityException : public PhysicsException {
   public:
    using PhysicsException::PhysicsException;
};

/**
 * @brief Exception thrown when movement operations fail
 *
 * This exception indicates errors during entity movement, collision detection,
 * or velocity calculations in the physics engine.
 */
class MovementException : public PhysicsException {
   public:
    using PhysicsException::PhysicsException;
};

/**
 * @brief Exception thrown when terrain state is invalid or corrupted
 *
 * This exception is used when terrain data is in an unexpected or invalid state,
 * such as position mismatches, corrupted grid data, or TOCTOU race conditions.
 */
class InvalidTerrainStateException : public PhysicsException {
   public:
    using PhysicsException::PhysicsException;
};

/**
 * @brief Exception thrown when vapor upward movement is blocked and should diffuse sideways
 *
 * Thrown by `moveVaporUp` when upward movement cannot proceed (e.g. moving obstruction
 * or no suitable vapor above to merge). The orchestrator can catch this and call
 * `moveVaporSideways` to attempt lateral diffusion.
 */
class VaporMovementBlockedException : public MovementException {
   public:
    using MovementException::MovementException;
};

}  // namespace aetherion
