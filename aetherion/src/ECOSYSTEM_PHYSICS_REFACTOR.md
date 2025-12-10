Plan: PhysicsEngine State Transition Safety Architecture
The PhysicsEngine suffers from dual-storage inconsistency: terrain entities exist partially in ECS (transient data like Velocity) and partially in OpenVDB (static data like mass, position). The line 509 error "Terrain does not exist" occurs when the byEntity_ position cache becomes stale during concurrent terrain movement or migration. The core issue is that state transitions span multiple non-atomic operations across different storage backends.

Steps
1. Introduce EntityHandle abstraction — Replace raw entt::entity + bool isTerrain parameter pairs with a type-safe EntityHandle variant that encapsulates storage location, prevents invalid casts between entity IDs and sentinel values (-1/-2), and provides isValid() method that checks both registry validity AND terrain existence in single atomic operation (see PhysicsEngine.cpp:488-509 where stale position lookups cause crashes).

2. Implement atomic multi-read transactions — Create SnapshotReader<T> class that acquires appropriate locks (TerrainGridRepository mutex OR ECS guarantees) and reads Position + Velocity + PhysicsStats in single critical section, returning immutable snapshot to prevent TOCTOU bugs like those in PhysicsEngine.cpp:488-527 where terrain can move between position lookup and velocity read.

3. Add movement reservation system — Before creating MovingComponent, reserve BOTH source and destination voxels atomically via MovementTransaction::reserve(from, to) that checks collision, marks voxels as "pending", and rolls back on failure. This prevents the race in PhysicsEngine.cpp:467-491 where applyTerrainMovement and updatePositionToDestination are separate operations allowing other threads to see partial state.

4. Unify terrain/non-terrain code paths — Refactor PhysicsEngine.cpp:162-200 and movement handlers to use polymorphic IPhysicsEntity interface instead of if (isTerrain) branches, eliminating code duplication and ensuring consistent locking/validation patterns regardless of storage backend.

5. Eliminate byEntity_ position cache — Remove std::unordered_map<entt::entity, Key> from TerrainGridRepository and make getPositionOfEntt() directly query OpenVDB TerrainStorage, accepting slower lookups in exchange for guaranteed consistency. This fixes the root cause where moveTerrain updates VDB but forgets to update the cache, causing line 509 crashes.

Defer event processing to frame boundary — Change event handlers like PhysicsEngine.cpp:771-884 to queue operations instead of executing immediately, preventing reentrancy and lock-order violations like those documented in EcosystemEngine.cpp:1155-1158 where dispatch-during-lock causes deadlocks.

Further Considerations
Performance regression from locking changes — Eliminating the byEntity_ cache and extending lock scopes will reduce throughput in high-entity-count scenarios (1000+ moving terrain). Should we implement a lock-free read path using RCU (Read-Copy-Update) or MVCC (Multi-Version Concurrency Control) to allow reads without blocking writers, or accept 20-30% performance hit for correctness?

Backward compatibility with saved games — The EntityHandle abstraction changes how entity references are serialized. Do we need migration path for old save files that store raw entt::entity integers, or is this acceptable breaking change given the feature branch context?

Integration with EcosystemEngine changes — Should PhysicsEngine changes wait for EcosystemEngine's water simulation refactor to complete (per the working journal's vapor movement bugs), or proceed independently and risk merge conflicts in VoxelGrid/TerrainStorage interfaces?