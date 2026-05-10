Concurrency & Threading
=======================

Aetherion uses a hybrid concurrency model: the dispatcher runs single-threaded on the main thread, while the heavy work is broken up across a TBB-backed worker pool with a staged event-submission path that lets worker threads safely enqueue events.

Threading Model
---------------

Main thread (per-tick orchestration in ``World::update``)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

1. ``HealthSystem::processHealth``
2. ``aetherion::diag::Registry::tick`` (flush counters whose window has elapsed)
3. ``WorkerEventSink::drain`` — replay every event staged by worker threads since the previous tick
4. ``dispatcher.update()`` — fire all queued events on the main thread
5. ``PhysicsEngine::processPhysics`` (synchronous pass — gravity, ECS-velocity, VDB-velocity)
6. ``MetabolismSystem::processMetabolism``
7. ``ecosystemEngine->processEcosystem`` (sync plant pipeline)
8. ``EffectsSystem::processEffects``
9. Python systems and per-tick scripts
10. Cleanup (entity deletion, gated on no async tasks running)
11. Async dispatch: ``physicsEngine->processPhysicsAsync`` and ``runEcosystemStep``

Worker pool (``World::asyncTasks_``)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

A single ``tbb::task_group`` is shared between physics-async and ecosystem-async dispatch, backed by TBB's process-wide persistent worker arena (default-sized to ``std::thread::hardware_concurrency()``). Submitting a task is sub-microsecond and never spawns an OS thread — a deliberate replacement for the previous ``std::future`` design which spun a fresh thread per tick and accumulated glibc per-thread arena state until RSS climbed.

Atomic gates (``physicsState_.running`` / ``ecosystemState_.running``) prevent duplicate enqueue if the next tick races the previous task's completion. Captured exceptions are stashed under each state's ``exceptionMutex`` and surfaced on the *next* tick, matching the pre-migration ``future.get()`` try/catch behaviour. ``~World()`` calls ``asyncTasks_.wait()`` before destruction (TBB asserts otherwise).

Water-simulation pool
~~~~~~~~~~~~~~~~~~~~~

``EcosystemEngine::waterSimManager_`` (``WaterSimulationManager``) owns its own worker threads that consume ``GridBoxTask``\ s from a ``RoundRobinScheduler`` (priority queue with aging). Each worker holds a ``GridBoxProcessor`` with thread-local OpenVDB ``Accessor``\ s — read-only during the per-box pass, so reads stay lock-free.

Cross-Thread Event Submission
-----------------------------

EnTT's ``entt::dispatcher`` is **not** thread-safe. Worker threads therefore cannot call ``dispatcher.enqueue<T>`` directly — they go through ``EventSink::enqueue<T>`` (:file:`src/EventSink.hpp`):

* main thread → ``entt::dispatcher::enqueue<T>`` directly
* any other  → ``WorkerEventSink`` (mutex-protected staging buffer)

``World::update`` drains the staging buffer at the top of every tick, *before* ``dispatcher.update()``, so worker-staged events replay on the main thread under the dispatcher's normal single-threaded contract.

Locking Strategies
------------------

Terrain grid lock
~~~~~~~~~~~~~~~~~

``TerrainGridRepository`` exposes a coarse external lock used during multi-step terrain mutations:

* ``lockTerrainGrid()`` / ``unlockTerrainGrid()`` — exclusive write lock; ``terrainGridLocked_`` (atomic) tracks state for re-entrancy detection.
* ``withSharedLock`` / ``withUniqueLock`` — internal helpers that skip locking when the calling thread already holds the exclusive lock (re-entrancy guard via ``currentThreadHoldsTerrainGridLock``).
* ``trackingMapsMutex_`` — separate ``shared_mutex`` for ``byCoord_`` / ``byEntity_`` so coord ↔ entity lookups don't contend with the terrain-grid lock during routine reads.

Most ``set*`` / ``get*`` repository methods accept a ``takeLock`` parameter so callers that already hold the lock can pass ``false`` and avoid double-locking.

Entity grid lock
~~~~~~~~~~~~~~~~

``VoxelGrid::entityGridMutex`` (``std::shared_mutex``) protects the non-terrain entity grid. Use ``VoxelGrid::getEntityUnsafe`` for hot read paths that can tolerate a lock-free read.

Atomic snapshots
~~~~~~~~~~~~~~~~

``TerrainGridRepository::getPhysicsSnapshot`` returns a ``TerrainPhysicsSnapshot`` (position + velocity + stats + existence) under a single read lock — collapsing what used to be three separate getters into one, and closing the TOCTOU window the physics math relied on.

Cross-thread queues
~~~~~~~~~~~~~~~~~~~

``WaterSimulationManager`` uses ``tbb::concurrent_queue`` for two cross-thread channels:

* ``resultQueue_`` — workers push ``std::vector<WaterFlow>`` that the manager applies under ``gridWriteMutex_`` once the box pass completes.
* ``errorQueue_`` — worker exceptions surface as ``ThreadError`` records that the main thread retrieves via ``getWaterSimErrors``.

Sync vs Async Toggles
---------------------

Two toggles on ``PhysicsManager`` change the threading shape at runtime:

* ``runEcosystemSynchronously`` — when ``true``, ``runEcosystemStep`` runs ``processEcosystemAsync`` inline on the main thread instead of submitting it to ``asyncTasks_``. Toggles every layer of the ecosystem path (water sim + plants + worker pool), not just water sim.
* ``processEcosystem_`` (on ``World``) — when ``false``, the ecosystem step is skipped entirely each tick.

Deadlock Notes
--------------

The previous coarse "physicsMutex held while taking TerrainStorage locks" deadlock window has been closed by the EventSink design — worker threads do not call into the dispatcher, and the physics-side event handlers always run on the main thread. The remaining lock-ordering rules:

* Acquire the **terrain grid** lock before the **tracking maps** lock when both are needed (the repository's helper methods enforce this internally).
* Never hold a TerrainStorage write lock across a Python callback or a ``dispatcher.update`` call.
* Worker threads must never call ``EventSink::raw_dispatcher_main_only`` — debug builds enforce this with an assert.
