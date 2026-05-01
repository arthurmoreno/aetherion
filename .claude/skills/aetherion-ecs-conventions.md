# Aetherion ECS Conventions

EnTT entity lifecycle, storage rules, overflow hazards, and tracking-map consistency for Aetherion.

---

## Entity Types and Where They Live

| Entity type | VDB storage | ECS entity |
|-------------|-------------|------------|
| Static terrain (rock, grass) | Yes | No — `ON_GRID_STORAGE` |
| Moving / active terrain (water in motion) | Yes | Yes |
| Plants, beasts, tile effects | No | Yes |

**Do not create an ECS entity for a terrain voxel unless the voxel is actively moving or has transient components (`Velocity`, `MovingComponent`).** Creating unnecessary entities accelerates version-counter exhaustion.

---

## `TerrainIdTypeEnum` Sentinel Values

```cpp
// src/components/TerrainComponents.hpp
enum struct TerrainIdTypeEnum { NONE = -2, ON_GRID_STORAGE = -1, ON_ENTT = 0 };
```

- `NONE = -2`: voxel is empty — no terrain data at all.
- `ON_GRID_STORAGE = -1`: terrain exists in VDB; no active ECS entity.
- `>= 0`: raw EnTT entity handle cast to `int`.
- **`< -2` is invalid** — indicates EnTT version-counter overflow or corruption. Never treat a value `< -2` as a valid handle.

---

## EnTT Version-Counter Overflow

EnTT v3 uses a 32-bit entity handle: low 23 bits = entity index, high 9 bits = version counter. The engine stores handles as `static_cast<int>(entity)` in the VDB grid.

**The overflow threshold**: after the same index is recycled **256 times**, the version field overflows and `static_cast<int>(entity)` produces a negative value. This is detected as `terrainId < -2`.

**This is not a fringe case.** A high-traffic water corridor can exhaust all 256 version slots in ~700 simulation steps.

Detection pattern used in tests:
```python
# Check no terrain ID has overflowed
for x, y, z in all_voxels:
    tid = world.get_terrain_id(x, y, z)
    assert tid >= -2, f"overflow at ({x},{y},{z}): {tid}"
```

---

## Tracking Maps: `byCoord_` and `byEntity_`

`TerrainGridRepository` maintains two maps that mirror the VDB terrain grid and the ECS:

- `byCoord_: VoxelCoord → entt::entity` — look up the entity at a position.
- `byEntity_: entt::entity → VoxelCoord` — look up the position of an entity.

**These three must always be kept consistent**: VDB grid, `byCoord_`, `byEntity_`. Updating one without the others leaves a "ghost entity" that causes duplicate deletion attempts or silent position mismatches.

Always use `addToTrackingMaps` / `removeFromTrackingMaps` — never write to the maps directly.

---

## Canonical Entity Creation Sequence

```cpp
// From TerrainGridRepository::ensureActive (TerrainGridRepository.cpp:438)
entt::entity e = registry_.create();
registry_.emplace<Position>(e, Position{x, y, z, direction});
registry_.emplace<Velocity>(e, Velocity{0.f, 0.f, 0.f});
registry_.emplace<MovingComponent>(e, MovingComponent{0});

VoxelCoord key{x, y, z};

// Set terrain ID in grid BEFORE tracking maps
if (storage_.terrainGrid) {
    storage_.terrainGrid->tree().setValue(C(x, y, z), static_cast<int>(e));
}
addToTrackingMaps(key, e);
```

Order matters: VDB grid set first, tracking maps second.

---

## `registry.valid(entity)` — When to Call It

`registry.valid(entity)` returns `false` if the entity has been destroyed **or** if its index has been recycled (version no longer matches the handle). Always call it before dereferencing a stored entity handle.

**Stale-handle hazard**: events carry an `entt::entity` captured at enqueue time. By dispatch time the entity may have been destroyed. Always validate at the dispatch site:

```cpp
// PhysicsEngine.cpp — pattern used throughout
if (!registry.valid(entity)) {
    // entity was destroyed before this event was dispatched
    return;
}
```

**Do not use an entity handle after calling `dispatcher.enqueue<DeleteOrConvertTerrainEvent>(entity)`.** The entity may be destroyed before the current tick returns — any use of the handle after that point is undefined behaviour.

---

## Cleanup Sequence for Terrain Entities

When destroying a terrain entity, the inverse of creation applies:

1. Call `removeFromTrackingMaps(key, entity)`.
2. Set the VDB grid cell to `ON_GRID_STORAGE` (`-1`) if static terrain remains, or `NONE` (`-2`) if the voxel is empty.
3. Call `registry_.destroy(entity)` last.

Reversing steps 2 and 3 leaves a window where the grid holds a destroyed handle that still looks like a live entity.

---

## Quick-Reference Rules

- Never create an ECS entity for static terrain — use `ON_GRID_STORAGE`.
- Never store a raw entity handle without also updating `byCoord_` / `byEntity_`.
- Always call `registry.valid(entity)` before using a handle that was not just created in the same call frame.
- Treat any `terrainId < -2` as a hard error — log, increment `physics_invalid_terrain_found`, and skip the voxel.
- Default to `addToTrackingMaps` / `removeFromTrackingMaps`; never touch the maps directly.
