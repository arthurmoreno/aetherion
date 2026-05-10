Ecosystem Engine
================

The ``EcosystemEngine`` (:file:`src/EcosystemEngine.hpp`, :file:`src/EcosystemEngine.cpp`) drives environmental simulation: parallel water flow, plant photosynthesis / growth / fruiting, and drought stress. After the recent refactor it is a *detection and dispatch* layer — it reads grid state and enqueues events through the ``EventSink``, but the matter-mutating handlers themselves live on the ``PhysicsEngine`` (see :doc:`physics_engine`).

Public Entry Points
-------------------

* ``processEcosystem(registry, voxelGrid, sink, clock)`` — synchronous pass. Currently runs ``processPlants`` (photosynthesis, growth, fruiting, drought stress).
* ``processEcosystemAsync(registry, voxelGrid, sink, clock)`` — heavy pass. Drives ``WaterSimulationManager::processWaterSimulation`` plus the plant pipeline.

``World::runEcosystemStep`` (:file:`src/World.cpp`) chooses between two execution modes per tick, gated by ``PhysicsManager::getRunEcosystemSynchronously``:

* **Synchronous mode** — runs ``processEcosystemAsync`` inline on the main thread after waiting on any in-flight task. Useful for tests and deterministic scenarios.
* **Async mode** (default) — submits ``processEcosystemAsync`` to ``World::asyncTasks_`` (TBB ``task_group``). An atomic ``ecosystemState_.running`` gate prevents duplicate dispatch; captured exceptions are surfaced through ``ecosystemState_.lastException`` on the next tick.

The engine can also be turned off entirely via ``World::setProcessEcosystem(false)`` — the step is then skipped each tick.

Water Simulation
----------------

Architecture
~~~~~~~~~~~~

* **Grid Partitioning**: The world is divided into ``GridBox`` chunks of 32×32×32 voxels (``WaterSimulationManager::DEFAULT_MIN_BOX_SIZE``) for cache-friendly iteration.
* **GridBoxProcessor**: Owns thread-local OpenVDB ``Accessor``\ s for ``waterMatterGrid``, ``vaporMatterGrid``, ``mainTypeGrid``, ``subType0Grid`` and ``flagsGrid`` (read-only). Processes a single box and produces a ``std::vector<WaterFlow>`` describing the desired changes.
* **WaterSimulationManager**: Owns the worker thread pool, the pre-computed ``gridBoxes_`` vector, the ``RoundRobinScheduler`` (priority queue with aging), and an error-tracking queue (``ThreadError``) so worker-thread exceptions surface to the main thread via ``getWaterSimErrors`` / ``hasEncounteredCriticalError``.

Per-phase toggles on ``PhysicsManager`` (mirrored on ``World`` / ``PhysicsSettings``) let callers disable individual stages of the pipeline without recompiling:

* ``simulateWaterMovement`` — gravity flow + sideways spread
* ``simulateWaterEvaporation`` — water → vapor transitions
* ``simulateVaporMovement`` — vapor diffusion / rise
* ``simulateVaporCondensation`` — vapor → water transitions
* ``waterAutoBalancing`` — passive leveling of liquid water across neighbors

Flow Logic
~~~~~~~~~~

1. **Water Flow**: ``processVoxelWater`` checks neighbors to determine flow direction.

   * **Downwards**: Primary flow direction due to gravity.
   * **Sideways**: Secondary flow if the path below is blocked.

2. **Evaporation**: ``processVoxelEvaporation`` converts water to vapor.

   * **Triggers**: Sufficient ``SunIntensity`` and accumulated heat past ``PhysicsManager::getHeatToWaterEvaporation`` (default 120.0).
   * **Action**: Decrements ``WaterMatter`` and enqueues vapor creation/merge events that the physics layer applies.

3. **Condensation**: Vapor converts back to liquid.

   * **Triggers**: Vapor concentration exceeds saturation (altitude/heat dependent), or impact with a ceiling.
   * **Action**: Enqueues ``CondenseWaterEntityEvent`` carrying the destination cell's existing terrain id; the handler picks between merging into liquid below or materialising a fresh water cell.

The processor returns a ``std::vector<WaterFlow>``; the manager applies the modifications under a single shared/exclusive lock (``gridWriteMutex_``) so the per-box reads can stay lock-free.

Plant Pipeline
--------------

``processPlants`` (in :file:`src/EcosystemEngine.cpp`) runs every tick of the synchronous pass:

* **Photosynthesis** — energy production scales with ``SunIntensity``, ``PlantResources::water``, and the plant's health.
* **Growth & fruiting** — plants consume accumulated energy to grow ``FruitComponent`` and to heal damage.
* **Water uptake** — when the cell beneath a plant has ``WaterMatter > 0``, the worker enqueues a ``PlantWaterUptakeEvent``; the physics-side handler is the only writer that touches ``PlantResources`` and the grass cell's ``MatterContainer`` (avoids cross-thread races).
* **Drought stress** — a ``WaterStressComponent`` counter ticks up while the plant has no water source. When it crosses ``PhysicsManager::getMaxWaterStressTicks`` (default 1000) the plant takes ``getDroughtDamagePerCycle`` HP (default 10) and the counter resets, producing visible step-down damage rather than a smooth drain. ``HealthSystem::processHealth`` then takes care of the kill cascade once ``healthLevel <= 0``.

See :file:`src/components/WaterStressComponent.hpp` for the per-component baseline constants and :file:`src/components/PlantsComponents.hpp` for ``PlantResources`` / ``FruitComponent``.

Cross-Thread Event Submission
-----------------------------

The ecosystem worker pool runs off the main thread, so it cannot enqueue directly into the (non-thread-safe) ``entt::dispatcher``. All event submissions go through ``EventSink::enqueue<T>`` (:file:`src/EventSink.hpp`), which routes by calling thread:

* main thread → ``entt::dispatcher::enqueue<T>`` directly
* any other  → ``WorkerEventSink`` staging buffer (mutex-protected)

``World::update`` drains the staging buffer at the top of each tick, *before* ``dispatcher.update()``, so worker-staged events always replay on the main thread under the dispatcher's normal single-threaded contract. See :doc:`concurrency` for the full lock/staging diagram.
