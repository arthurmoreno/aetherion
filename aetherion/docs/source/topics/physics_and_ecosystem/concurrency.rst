Concurrency & Threading
=======================

The Aetherion engine uses a hybrid concurrency model, combining coarse-grained locking for physics with fine-grained task parallelism for the ecosystem.

Threading Model
---------------

Physics
~~~~~~~
*   **Execution**: Primarily runs on the main thread or a dedicated physics thread.
*   **Locking**: Uses ``physicsMutex`` to lock the entire engine during critical processing steps.

Ecosystem
~~~~~~~~~
*   **Execution**: Uses a **Worker Thread Pool** managed by ``WaterSimulationManager``.
*   **Task-Based**: The world is split into tasks (``GridBoxTask``) processed concurrently by worker threads.

Locking Strategies
------------------

TerrainStorage Lock
~~~~~~~~~~~~~~~~~~~
The ``TerrainStorage`` (part of ``VoxelGrid``) is a shared resource protected by a read-write lock.

*   **Readers**: Worker threads acquire read locks to analyze grid state.
*   **Writers**: The main thread or specific update tasks acquire write locks to apply changes (e.g., moving water).
*   **Manual Control**: The code explicitly calls ``lockTerrainGrid()`` and ``unlockTerrainGrid()`` in ``EcosystemEngine.cpp``.

Concurrent Queues
~~~~~~~~~~~~~~~~~
``tbb::concurrent_queue`` is used extensively for event handling between threads:

*   ``pendingEvaporateWater``
*   ``pendingCreateWater``
*   ``pendingWaterFall``

Deadlock Risks
--------------

.. warning::
    There is a high risk of deadlocks due to the interaction between manual locks and mutexes.

*   **Scenario**: If ``physicsMutex`` is held while trying to acquire ``TerrainStorage`` locks, and a worker thread holding a ``TerrainStorage`` lock tries to acquire ``physicsMutex`` (e.g., via an event), a deadlock will occur.
*   **Mitigation**: Ensure a strict lock ordering hierarchy. Always acquire ``physicsMutex`` *before* ``TerrainStorage`` locks if both are needed, or decouple the systems using event queues.
