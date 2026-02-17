Performance & Bottlenecks
=========================

Understanding the performance characteristics of the Physics and Ecosystem engines is crucial for optimization.

Hotspots
--------

Voxel Traversal
~~~~~~~~~~~~~~~
Iterating over voxels is the most computationally expensive operation.

*   **Location**: ``GridBoxProcessor::processBox``
*   **Impact**: Even with OpenVDB's efficient accessors, the sheer volume of voxels in a large world creates significant overhead.
*   **Mitigation**: Use active voxel iterators where possible instead of dense iteration over the bounding box.

Entity Iteration
~~~~~~~~~~~~~~~~
Linear scans of entities can become a bottleneck as the population grows.

*   **Location**: ``PhysicsEngine::processPhysics``
*   **Impact**: Iterating over all entities with ``MovingComponent`` scales linearly with entity count.
*   **Mitigation**: Use spatial partitioning or chunks to limit processing to active areas.

Lock Contention
---------------

TerrainStorage Lock
~~~~~~~~~~~~~~~~~~~
The ``TerrainStorage`` lock is a central contention point.

*   **Issue**: If both engines and multiple worker threads try to lock the terrain grid simultaneously, performance degrades significantly due to thread blocking.
*   **Observation**: High contention is observed during periods of intense water simulation (e.g., heavy rain) when many write operations are queued.

Memory Bandwidth
----------------

*   **Issue**: The ``EcosystemEngine`` creates many small ``WaterFlow`` objects and pushes them to concurrent queues.
*   **Impact**: High water activity can saturate memory bandwidth with allocation and queue operations, leading to cache thrashing.
