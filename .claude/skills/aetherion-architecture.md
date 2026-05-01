# Aetherion Architecture — System Topology, Simulation Loop, Coordinate Convention

This skill encodes the non-obvious engine structure that most contradicts Unity/Unreal assumptions.
Read this before touching any simulation, terrain, water, or ECS code.

---

## System Topology

`World` (`src/World.hpp:37`) is the central controller. It owns all subsystems and holds:

- `entt::registry registry` — ECS entity and component storage
- `entt::dispatcher dispatcher` — event bus for all inter-system communication
- `VoxelGrid* voxelGrid` — spatial 3D grid; wraps OpenVDB grids and the entity grid
- `GameClock gameClock` — simulation time tracking

Subsystems instantiated in `World::World()` (`src/World.cpp:26`):

| Subsystem | File | Role |
|-----------|------|------|
| `PhysicsEngine` | `src/PhysicsEngine.hpp` | Event-driven: gravity, movement, collisions, all water phase changes |
| `EcosystemEngine` | `src/EcosystemEngine.hpp` | Per-voxel scanning: water flow dispatch, plant growth |
| `LifeEngine` | `src/LifeEvents.hpp` | Entity lifecycle, deferred deletion queue |
| `HealthSystem` | `src/HealthSystem.hpp` | Health tracking and damage |
| `MetabolismSystem` | `src/MetabolismSystem.hpp` | Energy and digestion |
| `CombatSystem` | `src/CombatSystem.hpp` | Combat mechanics |
| `EffectsSystem` | `src/EffectsSystem.hpp` | Tile effects (blood, environmental damage) |

There is **no monolithic "game loop"** in the Unity sense. The two primary simulation engines work differently:

- **`EcosystemEngine`** (`processEcosystem`): iterates voxels region-by-region via `WaterSimulationManager` + `GridBoxProcessor`. It *enqueues* events for water movement, evaporation, etc. — it does not apply changes directly.
- **`PhysicsEngine`** (`processPhysics`): event-driven. It *handles* the events enqueued by EcosystemEngine (and others), applying actual state changes. Contains all water phase handlers (`onWaterGravityFlowEvent`, `onWaterFallEntityEvent`, etc.).

---

## Simulation Loop

`World::update()` (`src/World.cpp:841`) is called once per tick. Exact call order:

```
gameClock.tick()                          // advance simulation time
healthSystem->processHealth(...)          // damage, health regen
flushPhysicsMetrics() / flushLifeMetrics()// write time-series DB
dispatcher.update()                       // flush queued events → call handlers
physicsEngine->processPhysics(...)        // movement, collisions, water phases
metabolismSystem->processMetabolism(...)  // energy (sync unless async flag set)
ecosystemEngine->processEcosystem(...)    // per-voxel scan → enqueues next tick's events
effectsSystem->processEffects(...)        // tile effect updates
for system in pythonSystems: system.update(...)  // custom Python systems
// deferred entity deletion (only after all async tasks complete)
```

Key implication: **EcosystemEngine enqueues events; PhysicsEngine handles them one tick later.**
`dispatcher.update()` flushes events enqueued in the *previous* tick before physics runs.

### Event pattern

Registering a handler (called once at startup in `registerEventHandlers`):
```cpp
dispatcher.sink<MyEvent>().connect<&MySystem::onMyEvent>(*this);
```

Dispatching an event (from any system, processed next `dispatcher.update()`):
```cpp
dispatcher.enqueue<MyEvent>(event_data);
```

### Async variants

`processPhysicsAsync`, `processEcosystemAsync`, `processMetabolismAsync` exist and run on background futures.
Entity deletion is deferred until **all async futures complete** to prevent TOCTOU races.

---

## Coordinate Convention

**Z is the vertical axis (up = higher Z).** This is the opposite of Unity (Y-up) and most 3D engines.

```
         +Z (up / UPWARD)
          │
          │
          └──────── +X (right)
         /
        /
      +Y (north / UP direction)
```

`DirectionEnum` (`src/components/PhysicsComponents.hpp:31`):

| Value | Constant | Meaning |
|-------|----------|---------|
| 1 | `UP` | +Y (north in the horizontal plane) |
| 2 | `RIGHT` | +X (east) |
| 3 | `DOWN` | -Y (south) |
| 4 | `LEFT` | -X (west) |
| 5 | `UPWARD` | +Z (vertical up) |
| 6 | `DOWNWARD` | -Z (vertical down, gravity direction) |

`Position` struct (`src/components/PhysicsComponents.hpp:43`):
```cpp
struct Position {
  int x;   // east–west
  int y;   // north–south
  int z;   // height (vertical, up = higher)
  DirectionEnum direction;
};
```

World dimensions: `width` (X) × `height` (Y) × `depth` (Z).
All terrain and voxel access uses `(x, y, z)` with Z as the vertical index.

---

## Python Bindings

Module: `_aetherion` (built with nanobind, `src/aetherion.cpp`).
In tests and lifesim, the engine is driven through `WorldManager` (`aetherion.world.manager`):

```python
manager.update()        # calls World::update() — one simulation tick
manager.get_terrain_id(x, y, z)
manager.get_water_matter(x, y, z)
```

All terrain reads/writes from Python go through `TerrainGridRepository` methods exposed on the world object — never touch `TerrainStorage` grids directly.

---

## What this engine is NOT

- **Not Unity/Unreal**: no scene graph, no per-component `Update()`, no frame-rate-driven loop.
- **Terrain is not purely ECS**: static terrain lives in OpenVDB (`TerrainStorage`); only *moving* terrain gets an ECS entity. See `aetherion-terrain-system` skill.
- **ECS entities are not stable IDs**: EnTT recycles entity handles. See `aetherion-ecs-conventions` skill.
