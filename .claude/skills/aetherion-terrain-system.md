# Aetherion Terrain System

Dual-layer VDB+ECS design, sentinel values, and the only safe read/write API. Read this before touching any terrain, water, or physics code.

---

## Two Layers, One Voxel

Every terrain voxel has **two independent storage layers**:

| Layer | Storage | What lives here |
|-------|---------|-----------------|
| Static | `TerrainStorage` (OpenVDB `Int32Grid` grids) | type, matter, physics stats, direction, mass, flags |
| Transient | EnTT ECS registry | `Velocity`, `MovingComponent` (only while voxel is in motion) |

A voxel can exist in the static layer with **no ECS entity at all**. This is the normal case for solid terrain and resting water. Do not assume that "terrain exists" implies "there is an EnTT entity."

---

## `TerrainIdTypeEnum` Sentinel Values (Critical)

The main `terrainGrid` (`openvdb::Int32Grid`) stores one `int` per voxel that encodes the voxel's activation state. Its values are defined by `TerrainIdTypeEnum` in `src/components/TerrainComponents.hpp`:

```
NONE            = -2   → voxel is empty, no terrain here
ON_GRID_STORAGE = -1   → terrain exists in VDB, no ECS entity (common static case)
ON_ENTT         =  0   → lower bound; any value ≥ 0 is a raw EnTT entity handle
```

Any value **`< -2` is invalid** — it signals EnTT version overflow or grid corruption. The test suite scans for these with `assert tid >= -2`.

Defined in `src/components/TerrainComponents.hpp`:
```cpp
enum struct TerrainIdTypeEnum { NONE = -2, ON_GRID_STORAGE = -1, ON_ENTT = 0 };
```

---

## Other Key Enums

```cpp
// src/components/TerrainComponents.hpp
enum struct TerrainEnum { EMPTY = -1, GRASS = 0, WATER = 1 };

struct MatterContainer {
  int TerrainMatter;   // solid substrate
  int WaterVapor;      // water in vapour form
  int WaterMatter;     // liquid water
  int BioMassMatter;   // biomass
};

// src/components/EntityTypeComponent.hpp
enum class EntityEnum { TERRAIN = 0, PLANT = 1, BEAST = 2, TILE_EFFECT = 3 };
```

---

## Read/Write API — Always Use `TerrainGridRepository`

**Never** access `TerrainStorage` grids directly from physics or ecosystem code. Always go through `TerrainGridRepository` (`src/terrain/TerrainGridRepository.hpp`):

```cpp
// Check existence (distinct from "has an ECS entity")
bool exists = repo.checkIfTerrainExists(x, y, z);
bool hasEntity = repo.checkIfTerrainHasEntity(x, y, z);

// Read matter
MatterContainer mc = repo.getTerrainMatterContainer(x, y, z);
int water = repo.getWaterMatter(x, y, z);
int vapor = repo.getVaporMatter(x, y, z);

// Write matter
repo.setWaterMatter(x, y, z, newAmount);
repo.setVaporMatter(x, y, z, newAmount);

// Read full terrain info (static + transient combined)
TerrainInfo info = repo.readTerrainInfo(x, y, z);
// info.active == true means a live ECS entity is attached
```

`checkIfTerrainExists(x, y, z)` returns `true` when `terrainGrid` is non-background — **this is not the same as** `terrainId >= 0`. A voxel can exist in VDB with `terrainId == -1` (`ON_GRID_STORAGE`) and have no ECS entity.

---

## `TerrainInfo` — The Arbitration Struct

`readTerrainInfo(x, y, z)` returns `TerrainInfo`, the single authoritative view of a voxel:

```cpp
struct TerrainInfo {
  int x, y, z;
  bool active;                       // true = ECS entity exists
  StaticData stat;                   // VDB-backed fields
  std::optional<TransientData> transient;  // ECS-backed (only when active)
};
```

Use `info.active` to check for a live ECS entity; more reliable than testing `terrainId >= 0` directly.

---

## Python API (via nanobind)

From Python tests or the `lifesim` layer, use the `TerrainStorage` binding (exposed on the `VoxelGrid` object) for reads, and `world.get_terrain`/`world.set_terrain` for high-level access:

```python
# Read terrain ID (the sentinel int stored in terrainGrid)
tid = voxel_grid.get_terrain(x, y, z)
assert tid >= -2, f"Corrupted terrain ID at ({x},{y},{z}): {tid}"

# Read matter via TerrainStorage binding
water = storage.get_terrain_water_matter(x, y, z)
vapor = storage.get_terrain_vapor_matter(x, y, z)

# Write matter
storage.set_terrain_water_matter(x, y, z, new_value)
```

---

## Canonical Entity Creation Sequence

When activating a voxel for transient behavior (e.g., water starting to flow):

```cpp
entt::entity e = repo.createEnttForTerrain(x, y, z);
registry.emplace<Velocity>(e, vx, vy, vz);
registry.emplace<MovingComponent>(e, ...);
repo.setTerrainId(x, y, z, static_cast<int>(e));    // sets grid sentinel to entity handle
repo.addToTrackingMaps(VoxelCoord{x, y, z}, e);     // keeps byCoord_/byEntity_ consistent
```

Always call `addToTrackingMaps` when creating and `removeFromTrackingMaps` when destroying. Never update just the VDB grid or just the ECS.

---

## Common Mistakes to Avoid

- **Do not test `terrainId >= 0` to check for existence** — the voxel can exist as `ON_GRID_STORAGE = -1`. Use `checkIfTerrainExists()`.
- **Do not access `TerrainStorage` grids directly** from PhysicsEngine or EcosystemEngine code. Use `TerrainGridRepository` methods.
- **Do not call `deleteTerrain(..., false)`** unless the caller already holds `TerrainGridLock` — see the concurrency skill.
- **Any terrain ID `< -2` is corruption** — caused by EnTT version overflow after ~256 recycles of the same entity index.
