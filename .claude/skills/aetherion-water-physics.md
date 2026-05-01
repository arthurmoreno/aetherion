# Aetherion Water Physics

Water simulation in Aetherion involves three cooperating actors, five simulation toggles, and several invariants that have caused bugs when misunderstood. Read this before editing any water code.

---

## Three Actors (all active in the same tick)

| Actor | File | Role |
|-------|------|------|
| `processTileWater` | `src/EcosystemEngine.cpp` | Per-voxel scan; classifies water state; dispatches movement and evaporation events |
| `_handleWaterGravityFlowEvent` | `src/PhysicsEngine.cpp` | Gravity-driven downward flow handler |
| `onWaterFallEntityEvent` | `src/PhysicsEngine.cpp` | Handles water arriving at a previously empty voxel after a fall |

These run across two threads (`EcosystemEngine` and `PhysicsEngine`). A voxel can be touched by more than one actor per tick — see `aetherion-concurrency.md`.

---

## Water States in a Single Voxel

Derived from `MatterContainer` fields — mutually exclusive categories:

```cpp
bool isVapor       = isWater && (matterContainer.WaterVapor > 0  && matterContainer.WaterMatter == 0);
bool isLiquidWater = isWater && (matterContainer.WaterMatter > 0 && matterContainer.WaterVapor == 0);
bool emptyWater    = isWater && (matterContainer.WaterMatter == 0 && matterContainer.WaterVapor == 0);
// WaterMatter > 0 && WaterVapor > 0 simultaneously is an error state
```

`isWater` means the voxel's `EntityTypeComponent` has `mainType == TERRAIN` and `subType0 == WATER`.

---

## PhysicsManager Simulation Toggles

Singleton: `PhysicsManager::Instance()`. Always check before dispatching or processing water events.

```cpp
PhysicsManager::Instance()->getSimulateWaterMovement()      // horizontal spread
PhysicsManager::Instance()->getSimulateWaterEvaporation()   // liquid → vapor
PhysicsManager::Instance()->getSimulateVaporCondensation()  // vapor → liquid
PhysicsManager::Instance()->getSimulateVaporMovement()      // vapor drift
PhysicsManager::Instance()->getWaterAutoBalancing()         // equalise adjacent voxels
```

`processTileWater` builds an action list from these flags and shuffles it for randomised order:

```cpp
if (PhysicsManager::Instance()->getSimulateWaterMovement())    actions.push_back(1);
if (PhysicsManager::Instance()->getSimulateWaterEvaporation()) actions.push_back(2);
if (actions.size() > 1) std::shuffle(actions.begin(), actions.end(), gen);
```

---

## Key Invariant: Empty Water Entity Cleanup

When `emptyWater == true` in `processTileWater`:

- If `terrainId` is a live entity handle (not `ON_GRID_STORAGE` or `NONE`), enqueue deletion:
  ```cpp
  entt::entity entity = static_cast<entt::entity>(terrainId);
  dispatcher.enqueue<DeleteOrConvertTerrainEvent>(entity);
  ```
- If `terrainId == ON_GRID_STORAGE` or `NONE`, log and take no action — this is not an error.

**Never leave a water entity alive with zero `WaterMatter` and zero `WaterVapor`.** The cleanup path above is the only safe exit.

---

## Spring Injection (`SpringWaterSystem`)

`SpringWaterSystem` is a Python system (`src/aetherion/reference/systems/spring_water.py`) registered via `world.add_python_system()`. It adds `+1 water_matter` to a fixed source voxel every `pace` ticks. If the source voxel has no terrain, it creates one.

```python
mc.water_matter += 1
voxel_grid.set_terrain_matter_container_component(x, y, z, mc)
```

The spring injects water regardless of the current simulation state (no toggle guards). If water oscillates in a staircase or corridor, the spring is not the root cause — look at the flow or gravity handlers first.

---

## Stale Event Hazard: `WaterFallEntityEvent`

`WaterFallEntityEvent` carries an `entt::entity` handle captured **at enqueue time**. Between enqueue and dispatch, the entity may be destroyed or recycled. Always validate before use:

```cpp
void PhysicsEngine::onWaterFallEntityEvent(const WaterFallEntityEvent &event) {
    entt::entity terrainEntity = event.entity;
    // sentinel check first
    if (static_cast<int>(terrainEntity) == static_cast<int>(TerrainIdTypeEnum::NONE)) {
        return;  // ON_GRID_STORAGE path — no ECS entity to check
    }
    if (!registry.valid(terrainEntity)) {
        // entity recycled between enqueue and dispatch — handle gracefully
        voxelGrid->deleteTerrain(dispatcher, event.position.x, event.position.y,
                                 event.position.z, true);
        // ... recovery path
    }
}
```

Do **not** use an entity handle after calling `dispatcher.enqueue<DeleteOrConvertTerrainEvent>(entity)` — the entity may be destroyed before this tick's handlers run.
