#ifndef COMPONENT_MUTATORS_HPP
#define COMPONENT_MUTATORS_HPP

/**
 * @file ComponentMutators.hpp
 * @brief Focused, in-place ECS component mutators.
 *
 * This module groups small functions that perform direct, in-place updates to
 * `entt` components (for example: `Position`, `Velocity`, `EntityTypeComponent`,
 * `StructuralIntegrityComponent`, and `MatterContainer`) for a single entity.
 *
 * Characteristics that set these functions apart:
 * - They operate only on ECS component storage and do not modify `VoxelGrid`
 *   / `TerrainGridRepository` storage directly.
 * - They avoid taking global locks or performing long-running work; callers
 *   (higher-level orchestrators or event handlers) are responsible for
 *   synchronization (e.g., `TerrainGridLock`).
 * - Implementations live in `ComponentMutators.cpp` to keep linkage consistent
 *   and prevent ODR/static linkage mismatches.
 *
 * Good candidates for this module:
 * - Small helpers that add/modify/remove components from a single entity.
 * - Conversions that change an entity's component set or normalize fields.
 * - Functions that are cheap and side-effect-local to an entity's components.
 *
 * Not suitable for this module:
 * - Creating or destroying entities, or multi-tile/VoxelGrid repository writes.
 * - Operations that must acquire `TerrainGridLock` or perform bulk updates.
 */

#include <entt/entt.hpp>

#include "components/EntityTypeComponent.hpp"
#include "components/PhysicsComponents.hpp"
#include "voxelgrid/VoxelGrid.hpp"

// Direct component mutators (definitions in ComponentMutators.cpp)
/**
 * @brief Directly modifies the fields of a Velocity component.
 * @param velocity Reference to the Velocity component to modify.
 * @param newVx The new velocity on the X-axis.
 * @param newVy The new velocity on the Y-axis.
 * @param newVz The new velocity on the Z-axis.
 */
void updateEntityVelocity(Velocity& velocity, float newVx, float newVy, float newVz);

/**
 * @brief Ensures a terrain entity has a Position component, adding one if it's missing.
 * @details This is a safety check for terrain entities (e.g., vapor) that might be processed
 * by physics before being fully initialized. It fetches the position from the
 * TerrainGridRepository.
 * @param registry The entt::registry.
 * @param voxelGrid The VoxelGrid for accessing the terrain repository.
 * @param entity The entity to check.
 * @param isTerrain Flag indicating if the entity is a terrain entity.
 * @throws std::runtime_error if the entity is missing a position and it cannot be found in the
 * repository.
 */
void ensurePositionComponentForTerrain(entt::registry& registry, VoxelGrid& voxelGrid,
                                       entt::entity entity, bool isTerrain);

/**
 * @brief Converts an entity into a "soft empty" terrain block.
 * @details Used when an entity cannot be immediately destroyed because there are
 * ongoing tile effects; converts the entity's components so it behaves as an empty tile
 * while effects finish processing.
 * @param registry The entt::registry.
 * @param terrain The terrain entity to convert.
 */
void convertIntoSoftEmpty(entt::registry& registry, entt::entity& terrain);

/**
 * @brief Sets or overwrites components of an entity in the ECS to represent an empty water tile.
 * @param registry The entt::registry.
 * @param terrain The entity to modify.
 * @param matterState The matter state (e.g., LIQUID) to assign to the entity.
 */
void setEmptyWaterComponentsEnTT(entt::registry& registry, entt::entity& terrain,
                                 MatterState matterState);

#endif  // COMPONENT_MUTATORS_HPP
