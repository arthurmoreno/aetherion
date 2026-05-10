Physics Engine
==============

The ``PhysicsEngine`` (:file:`src/PhysicsEngine.hpp`, :file:`src/PhysicsEngine.cpp`) drives entity motion *and* the application side of the water cycle. After the recent refactor, all matter-mutating events that the ``EcosystemEngine`` used to handle — evaporation, condensation, water spread, gravity flow, vapor merges, water/vapor creation, terrain phase conversion — are routed to handlers on the ``PhysicsEngine``. The ``EcosystemEngine`` only *detects* the conditions and *enqueues* events through the ``EventSink``.

Public Entry Points
-------------------

The engine exposes two top-level passes that ``World::update`` (:file:`src/World.cpp`) drives each tick:

* ``processPhysics(registry, voxelGrid, sink, clock)`` — synchronous pass on the main thread. Runs every tick, after the worker staging buffer has been drained and ``dispatcher.update()`` has fired the previous tick's events.
* ``processPhysicsAsync(registry, voxelGrid, sink, clock)`` — submitted to the shared ``tbb::task_group`` (``World::asyncTasks_``). Runs in parallel with ``runEcosystemStep`` and the next tick's main-thread work; an atomic ``physicsState_.running`` gate prevents duplicate dispatch.

Iteration Model
---------------

The synchronous pass splits into three iteration helpers, each kept clearly separated by *iteration source*:

1. **Gravity-force enqueue (ECS-backed)** — ``applyGravityForcesToECSEntities`` walks ``registry.view<Position>()`` and delegates to ``applyGravityForceToEntity`` for the per-entity decision tree (terrain support check, fall trigger, force enqueue).
2. **Velocity processing (ECS-backed)** — ``processVelocityForECSEntities`` walks ``registry.view<Velocity>()`` and runs per-entity validation, cold-vapor revival, and ``handleMovement``.
3. **Velocity processing (VDB-backed)** — ``processVelocityForVDBVoxels`` is two-phase: it collects active voxels from the VDB velocity grids (``velXGrid``/``velYGrid``/``velZGrid`` in :file:`src/terrain/TerrainStorage.hpp`), skipping any whose terrain id maps to an active ECS entity (those were already processed in step 2), then runs the per-voxel velocity update + movement trigger via ``processVelocityForVoxel``.

The third path exists because terrain matter (water, vapor) carries velocity directly in OpenVDB grids rather than as ECS components — see :doc:`world_simulation` for the storage rationale.

Movement Mutators
-----------------

The actual position/velocity arithmetic lives in :file:`src/physics/mutators/MovementMutators.cpp` and :file:`src/physics/PhysicalMath.cpp`, with shared helpers in :file:`src/physics/PhysicsUtils.hpp`, validators in :file:`src/physics/PhysicsValidators.hpp`, and TOCTOU-safe component reads in :file:`src/physics/ComponentMutators.hpp`. Collision predicates (including ramp/slope handling) are in :file:`src/physics/Collision.hpp`.

Water Phase Handlers
--------------------

The following events are dispatched by the ecosystem layer (and by tests / Python via ``World::dispatch*`` helpers) and resolved on the physics side via handlers registered in ``PhysicsEngine::registerEventHandlers``:

* ``EvaporateWaterEntityEvent`` → ``onEvaporateWaterEntityEvent``
* ``CondenseWaterEntityEvent`` → ``onCondenseWaterEntityEvent``
* ``WaterFallEntityEvent`` → ``onWaterFallEntityEvent``
* ``WaterSpreadEvent`` → ``onWaterSpreadEvent``
* ``WaterGravityFlowEvent`` → ``onWaterGravityFlowEvent``
* ``TerrainPhaseConversionEvent`` → ``onTerrainPhaseConversionEvent``
* ``VaporCreationEvent`` / ``WaterCreationEvent`` → ``onVaporCreationEvent`` / ``onWaterCreationEvent``
* ``VaporMergeUpEvent`` / ``VaporMergeSidewaysEvent`` → ``onVaporMergeUpEvent`` / ``onVaporMergeSidewaysEvent``
* ``AddVaporToTileAboveEvent`` → ``onAddVaporToTileAboveEvent``
* ``DeleteOrConvertTerrainEvent`` → ``onDeleteOrConvertTerrainEvent``
* ``PlantWaterUptakeEvent`` → ``onPlantWaterUptakeEvent``

The water-creation and condensation events carry a bounded ``retryCount`` so vapor/water conflicts at the destination cell are re-dispatched up to a limit and then dropped, which prevents infinite loops on sealed vapor pockets. See :file:`src/ecosystem/EcosystemEvents.hpp` for the event payloads.

Collision Detection
-------------------

The engine checks the target voxel via ``VoxelGrid``/``TerrainGridRepository`` accessors:

* **Standard collision** — solid terrain or another solid entity at the destination cell blocks the move.
* **Ramp/slope collision** — entities moving into a ``RAMP_*`` or ``CORNER_*`` variant (see ``TerrainVariantEnum`` in :file:`src/components/TerrainComponents.hpp`) get their target z adjusted so they slide up or down a single step. The ``GradientVector`` (now ``float gx, gy, gz``) on ``StructuralIntegrityComponent`` carries the local slope.

State Management
----------------

Entities transition between states during movement:

1.  **Start** — ``MovingComponent`` is added to the entity (or, for terrain voxels, to the repository's coord-keyed ``movingByCoord_`` map) and the velocity is committed.
2.  **Update** — ``handleMovement`` advances position, performs collision/ramp adjustment, and writes the new ``Position`` back through ``VoxelGrid`` / ``TerrainGridRepository::moveTerrain``.
3.  **End** — On arrival, ``MovingComponent`` is cleared. For terrain voxels with no remaining transient state, ``softDeactivateEntity`` returns the cell to ``ON_GRID_STORAGE`` (see ``TerrainIdTypeEnum``: ``NONE = -2``, ``ON_GRID_STORAGE = -1``, ``ON_ENTT >= 0``).

Telemetry
---------

The engine owns a ``PhysicsCounters`` struct of ``aetherion::diag::Counter`` handles (one per event type plus the move events). ``registerDiagCounters`` wires them to the GameDB sink via ``aetherion::diag::Registry``; samples that arrive before initialise are silently dropped. The counter names match the legacy ``physics_*`` GameDB series so historical plots keep working. See :doc:`../profiling_and_debugging/diagnostic_module` for the registry side.
