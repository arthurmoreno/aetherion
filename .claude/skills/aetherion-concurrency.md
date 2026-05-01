# Aetherion Concurrency

`TerrainGridLock` RAII guard, locking model, `takeLock` rules, and known race patterns.

---

## `TerrainGridLock` — Always Use RAII

`TerrainGridLock` (`src/terrain/TerrainGridLock.hpp`) is a non-copyable, non-movable RAII guard. Constructing it calls `repo->lockTerrainGrid()`; the destructor calls `repo->unlockTerrainGrid()`.

```cpp
// Correct — lock lives for the entire critical section
TerrainGridLock lock(voxelGrid.terrainGridRepository.get());
// ... all terrain writes here ...
// lock released automatically at scope exit
```

**Never call `lockTerrainGrid()` / `unlockTerrainGrid()` directly** — manual pairing is error-prone and lacks exception safety.

---

## The RAII Scope Bug (Historical — Do Not Repeat)

```cpp
// WRONG — lock is destroyed at the closing brace of the if-block,
// before storage_.deleteTerrain() runs below.
if (takeLock) {
    TerrainGridLock lock(this);  // released here ← bug
}
storage_.deleteTerrain(x, y, z);  // runs without the lock
```

The correct pattern for optional locking is `std::unique_ptr<TerrainGridLock>` with function-scope lifetime:

```cpp
// Correct — lock stays alive until end of function
std::unique_ptr<TerrainGridLock> lock;
if (takeLock) {
    lock = std::make_unique<TerrainGridLock>(this);
}
storage_.deleteTerrain(x, y, z);  // protected when lock is held
```

This is the pattern used in `TerrainGridRepository::deleteTerrain` (line ~925) and in `PhysicsEngine::handleMovement` (line ~607).

---

## `deleteTerrain` and the `takeLock` Parameter

```cpp
// VoxelGrid.hpp
void deleteTerrain(entt::dispatcher &dispatcher, int x, int y, int z,
                   bool takeLock = true);
```

**Default rule: always pass `takeLock = true`** (or omit the parameter — `true` is the default).

`takeLock = false` is only safe when the calling function already holds the lock via its own `TerrainGridLock`. When passing `false`, the caller must be able to guarantee lock ownership — in debug builds add:

```cpp
assert(voxelGrid.terrainGridRepository->isTerrainGridLocked());
```

The same rule applies to `setTerrainId`, `clearActive`, and any other `TerrainGridRepository` method that accepts `takeLock`.

---

## Re-entrancy: `withUniqueLock` / `withSharedLock`

Internal repository methods route through `withUniqueLock` / `withSharedLock` helpers (defined in `TerrainGridRepository.hpp`). These check `currentThreadHoldsTerrainGridLock()` before trying to re-acquire `terrainGridMutex`. If the current thread already holds the lock, the helpers skip re-acquisition — making inner calls safe.

```cpp
// withUniqueLock skips mutex if this thread already holds TerrainGridLock
template <typename Func>
auto withUniqueLock(Func &&func, bool takeLock = true) -> decltype(func()) {
    if (!takeLock) return func();
    if (!currentThreadHoldsTerrainGridLock()) {
        std::unique_lock<std::shared_mutex> lock(terrainGridMutex);
        return func();
    }
    return func();  // already locked — proceed directly
}
```

`isTerrainGridLocked()` reads an `std::atomic<bool>` (`terrainGridLocked_`) set by `lockTerrainGrid()`. Use it to verify lock state at call sites, but prefer `currentThreadHoldsTerrainGridLock()` when checking re-entrancy.

---

## Two Threads That Can Collide on the Same Voxel

In a single simulation tick two threads can reach the same voxel simultaneously:

| Thread | Code path | Lock state |
|--------|-----------|------------|
| `EcosystemEngine` worker | `processTileWater` — per-voxel scan, dispatches events | **No lock held by default** |
| `PhysicsEngine` event handler | `_handleWaterGravityFlowEvent` | Acquires `TerrainGridLock` at entry |

The `PhysicsEngine` side (`_handleWaterGravityFlowEvent`, `onWaterSpreadEvent`, `onTerrainPhaseConversionEvent`) consistently acquires `TerrainGridLock` via `std::make_unique<TerrainGridLock>(...)` before reading or writing terrain state. The `EcosystemEngine` acquires the lock at the start of `processTileWater` for write paths.

**Historical hazard**: `onWaterFallEntityEvent` did not acquire the lock in earlier versions, creating a window where it could race with the ecosystem scan. The fix: acquire `TerrainGridLock` at the top of any event handler that modifies terrain.

---

## Rule of Thumb for New Code

When adding a new `deleteTerrain`, `setTerrainId`, or any terrain-mutating call:

1. **Default to `takeLock = true`** (or omit the parameter).
2. Only change to `false` after explicitly verifying the caller already holds a `TerrainGridLock` with function-scope lifetime.
3. Prefer placing `TerrainGridLock lock(repo)` at the **top** of the function, not inside an `if`-block.
4. Never hold the terrain grid lock across a `dispatcher.enqueue<...>()` call that could itself acquire the lock — dispatch first, then lock (or use `dispatcher.trigger` synchronously within the lock window only when safe).
