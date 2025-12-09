Physics Engine
==============

The ``PhysicsEngine`` handles the movement and physical interactions of entities within the world. It translates continuous physical forces into discrete voxel movements.

Core Concepts
-------------

Movement Logic
~~~~~~~~~~~~~~
The core logic resides in ``PhysicsEngine::processPhysics``. It calculates new velocities based on:

*   **Acceleration**: Forces applied to the entity.
*   **Gravity**: Constant downward force defined in ``PhysicsManager``.
*   **Friction**: Deceleration applied when moving against surfaces or through media.

Grid Translation
~~~~~~~~~~~~~~~~
The engine converts continuous physics values (velocity/acceleration) into discrete grid movements.

*   **Translation**: ``translatePhysicsToGridMovement`` maps float velocities to integer grid steps.
*   **Clamping**: Speeds are clamped to ``MAX_SPEED`` to prevent tunneling or instability.

Collision Detection
-------------------

Standard Collision
~~~~~~~~~~~~~~~~~~
The engine checks if the target voxel contains a solid entity or terrain using ``VoxelGrid`` accessors.

Special Collision (Slopes)
~~~~~~~~~~~~~~~~~~~~~~~~~~
The ``hasSpecialCollision`` method handles interactions with non-block terrain, such as slopes or ramps.

*   **Ramps**: If an entity moves into a ramp, the engine checks if it can "step up" or slide down based on the ramp's orientation.
*   **Adjustment**: The target coordinate is adjusted (z + 1 or z - 1) to simulate smooth movement over the slope.

State Management
----------------

Entities transition between states during movement:

1.  **Start**: ``MovingComponent`` is added to the entity.
2.  **Update**: Position is updated in ``VoxelGrid`` and ``entt::registry``.
3.  **End**: Upon reaching the destination, ``MovingComponent`` is removed or updated for the next step.
