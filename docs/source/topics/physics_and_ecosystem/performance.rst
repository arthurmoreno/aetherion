Performance & Bottlenecks
=========================

Understanding the performance characteristics of the Physics and Ecosystem engines is crucial for optimization.

Hotspots
--------

Voxel Traversal
~~~~~~~~~~~~~~~
Iterating over voxels is the most computationally expensive operation.

*   **Location**: ``GridBoxProcessor::processBox`` in :file:`src/EcosystemEngine.cpp`, plus the matter/velocity iterators on ``TerrainStorage`` (``iterateWaterMatter``, ``iterateVaporMatter``, ``iterateVelocityVoxels``) consumed by ``TerrainGridRepository``.
*   **Impact**: Even with OpenVDB's efficient accessors, the sheer volume of voxels in a large world creates significant overhead. The 32³ ``GridBox`` partitioning is the cache-friendly answer.
*   **Mitigation**: Always go through the ``cbeginValueOn`` iterators in ``TerrainStorage`` rather than dense bounding-box scans — the templated ``iterateGrid`` helper enforces this.

Entity / Voxel Iteration in PhysicsEngine
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The synchronous physics pass does three full iterations per tick:

*   ``applyGravityForcesToECSEntities`` walks ``registry.view<Position>()``.
*   ``processVelocityForECSEntities`` walks ``registry.view<Velocity>()``.
*   ``processVelocityForVDBVoxels`` walks the active set of the VDB velocity grids (``velXGrid``/``velYGrid``/``velZGrid`` in :file:`src/terrain/TerrainStorage.hpp`).

The third iteration scales with the number of voxels currently carrying non-zero velocity, not with total terrain — this is what makes terrain-matter physics affordable. The TOCTOU-safe ``TerrainGridRepository::getPhysicsSnapshot`` collapses what used to be three separate component reads into one; prefer it over ad-hoc combinations of ``getPosition``/``getVelocity``/``getPhysicsStats``.

Lock Contention
---------------

Terrain grid lock
~~~~~~~~~~~~~~~~~

``TerrainGridRepository::terrainGridMutex`` (``std::shared_mutex``) is the central contention point. Most read paths go through the shared lock; the exclusive lock is taken during multi-step mutations or via the explicit ``lockTerrainGrid()`` API.

*   **Re-entrancy**: ``withSharedLock`` / ``withUniqueLock`` short-circuit when the calling thread already holds the exclusive lock — keep code paths consistent so the guard fires.
*   **Observation**: High contention is observed during periods of intense water simulation (e.g., heavy rain) when the ``WaterSimulationManager`` worker pool feeds modifications back through ``applyModificationsWithLock``.

Tracking-maps lock
~~~~~~~~~~~~~~~~~~

``trackingMapsMutex_`` is a *separate* ``shared_mutex`` for ``byCoord_`` / ``byEntity_`` so coord ↔ entity lookups don't contend with the broader terrain-grid lock.

Memory Bandwidth
----------------

*   **Issue**: ``GridBoxProcessor`` produces many small ``WaterFlow`` objects per box, returned through ``WaterSimulationManager::resultQueue_`` (``tbb::concurrent_queue``).
*   **Impact**: High water activity can saturate memory bandwidth with allocation and queue operations, leading to cache thrashing.

Async dispatch overhead
~~~~~~~~~~~~~~~~~~~~~~~

Physics-async and ecosystem-async dispatch share ``World::asyncTasks_`` (``tbb::task_group``) — a single persistent worker arena. This replaced the previous ``std::async`` model (which spawned a fresh OS thread per tick and slowly grew RSS through glibc per-thread arena accumulation). Submitting a task is sub-microsecond and never spawns a thread.

See also
--------

The bottlenecks above are starting points; the playbook for *measuring*
them lives in :doc:`../profiling_and_debugging/index`.

For instrumenting any of the hot paths on this page with durable counters
or gauges that survive across sessions, see
:doc:`../profiling_and_debugging/diagnostic_module`. For attributing a
slow tick or a memory regression to a specific call stack with valgrind,
heaptrack, AddressSanitizer, or perf, see
:doc:`../profiling_and_debugging/external_profilers`.
