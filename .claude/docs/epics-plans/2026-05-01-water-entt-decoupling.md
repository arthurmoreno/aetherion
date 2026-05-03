# Water Physics — Remove EnTT Dependency (Decoupling Epic)
_Created: 2026-05-01_
_Updated: 2026-05-01 — TDD mindset added (RED/GREEN, nanobind exposure). Tasks A + B done._
_Updated: 2026-05-01 — Velocity caller audit added migration-period tasks C-Pre.1 / C-Pre.2 (parallel VDB iteration in `processPhysics` so ON_GRID_STORAGE voxels stay visible). Task C1 done._
_Updated: 2026-05-02 — Cross-referenced with `2026-05-02-water-fall-event-merge-fix.md`: merge-fix E1.1 made `createWaterTerrainFromFall` additive + vapor-safe (the contract C2/C3/C4 must adopt). Required Nanobind Additions §1/§2 done; `World.dispatch_water_fall_event` and `TerrainGridRepository.sum_total_water` shipped via E1.1._
_Updated: 2026-05-02 — Task C2 done; reference scenario package `aetherion.reference.world.scenarios` shipped with shared placement primitives. Audit of `registry.create()` sites: C3 retargeted to `createWaterTerrainBelowVapor` (the original `createWaterTerrainFromSpread` never existed); C4 expanded to 3 vapor sites; new Task R extracts water-cluster mutators to `WaterPhysicsMutators.cpp` (4 of 13 done in `0bf0b69`)._
_Updated: 2026-05-02 — C3 implementation shipped; live-game testing exposed two issues. (1) ON_GRID_STORAGE water didn't fall because the gravity path was ECS-only — fixed by velocity-grid-driven strategy (C3.0 byte-identical extraction commit `6055fa2`; C3.1 + C3.1.1 seed-on-creation + propagate-across-`moveTerrain` commit `efda44c`); C3.2 dropped (no coord-based event needed). (2) Falling water onto vapor + condensation onto vapor both violated the both-WaterMatter-and-WaterVapor invariant — fixed by retry-then-abort guards C3.6 + C3.9 (commit `a3c6b97`)._
_Updated: 2026-05-02 — Architectural cleanup: `TerrainGridRepository` must stay pure CRUD. C3.8 (commit `efda44c`) moved the deletion wake-up out of the repository into `processVelocityForVoxel`; `TerrainDeletedEvent` deleted, `moveTerrain` reverted to single-arg signature. Sequencing for remaining work: C3.7 quick-win → C3.3 audit → C3.4/C3.5 regression scenarios._
_Updated: 2026-05-02 — C4 sites 1+2 shipped (`createVaporTerrainEntity`, `createEmptyActiveTerrain` rewritten to write `ON_GRID_STORAGE` directly; `void` return). The original `Entity has both WaterMatter and WaterVapor` invariant crash is **gone** in the live game; C3.6/C3.9 retry guards plus the C4 changes appear to have closed the path that produced it. New crash surfaced: `[Thread N] Exception at box M: Resource deadlock avoided` — `std::system_error(EDEADLK)` from `pthread_rwlock_wrlock` when the same thread re-acquires the non-recursive `terrainGridMutex`. Two new follow-up subtasks added: **C4.2** (make `TerrainGridLock` recursion-safe — already has the thread-local plumbing it needs to detect re-entry, just doesn't use it in the constructor) and **C4.3** (the noisy `[moveVaporUp] Error: Vapor entity in ON_GRID_STORAGE at (...)` floods are coming from `validateVaporTerrainId` rejecting ON_GRID_STORAGE before the dedicated branch at `moveVaporUp:1052` can run; reorder so the function actually drives the create-vapor-entity-then-move path). Site 3 (`createAndRegisterVaporEntity`) remains under C4.1 — it's structurally tied to `MoveGasEntityEvent` and depends on either C4.1's coord-aware variant or skipping the entity creation altogether._
_Updated: 2026-05-03 — Returning `Entity has both WaterMatter and WaterVapor` invariant violation finally caught and band-aid-fixed. Two prior attempts at runtime instrumentation inside `setTerrainMatterContainer` (storage-read snapshot, then parameter-only with `std::source_location` + throw) both segfaulted; lesson recorded in user-memory and revisited under Task R Phase 3. Successful approach: `_logIfViolatingMatterWrite(mutatorName, x, y, z, outgoing)` helper added to `PhysicsMutators.hpp`, called immediately before every `setTerrainMatterContainer` inside each mutator (parameter-only check, no storage reads, no throw, nullptr-guarded `spdlog::get("console")`). On the first run with instrumentation in place, the live game logged `[INVARIANT-WRITE][createWaterTerrainFromFall:dest] coord=(49, 49, 2) WaterMatter=1 WaterVapor=1` immediately before the per-tick crash. Root cause: `createWaterTerrainFromFall` read `destMatter` from VDB **unconditionally** at the top of the function, even when `terrainId == NONE`; if a prior path left orphan `WaterVapor > 0` in the WaterVapor grid for that voxel, the additive `destMatter.WaterMatter += fallingAmount` produced the both-fields-positive violation. Fixed by adopting `_handleWaterCreationEvent`'s conditional-read pattern: zero-init `destMatter` and read from VDB only when `!destinationIsEmpty`. The subsequent `setTerrainMatterContainer` call also overwrites the orphan WaterVapor in storage as a side effect, so the band-aid both prevents future violations *and* cleans the residue. Deeper architectural bug remains: an upstream path is leaving orphan `WaterVapor > 0` in `terrainId == NONE` cells. Tracking now under **R.1** (the band-aid that landed) and **R.2** (the orphan-source hunt). Next session: rising vapor not moving sideways randomly at the top of the column._

## Executive Summary

Water voxels in the current physics engine are backed by EnTT entities. Every water creation
calls `registry.create()`, every deletion calls `registry.destroy()`. Because EnTT recycles
entity indices with a version counter, ~700 recycles of the same high-traffic voxel push the
version bits past `INT_MAX`, causing `static_cast<int>(entity)` to produce a negative terrain
ID — the root of the crash documented in `2026-04-30-water-sim-crash-fix-plan.md`. The fix is
architectural: water voxels must have no EnTT entities at all. All water state (including the
two currently ECS-only pieces: `Velocity` and `MovingComponent`) must live in
`TerrainGridRepository` / `TerrainStorage` (OpenVDB), which was already designed for this
purpose. EnTT stays only for entities with `Inventory`, `DigestionComponent`,
`PerceptionComponent`, or other list/callback-based state. See full analysis in
`.claude/docs/analysis/2026-05-01-water-entt-removal.md`.

---

## Confirmed Facts

**All static terrain data is already in VDB (TerrainStorage).** `EntityTypeComponent`,
`MatterContainer`, `PhysicsStats`, `StructuralIntegrityComponent` are all fully migrated.
No ECS reads of these for terrain in the water physics path.

**Two components are still ECS-only for terrain:**
- `Velocity` — backed by `onConstructVelocity` hook in `TerrainGridRepository.cpp:31–40`
- `MovingComponent` — backed by `onConstructMoving` hook in `TerrainGridRepository.cpp:34–39`

**`registry.create()` sites in the water path** (all must be eliminated). The original
audit listed 7 sites by line number; subsequent audits found 3 more. Current state by
function:

| Function | File:line | Status |
|----------|-----------|--------|
| `createVaporTerrainEntity` | `PhysicsMutators.hpp:116` | done — C4 |
| `createEmptyActiveTerrain` | `PhysicsMutators.hpp:175` | done — C4 |
| `createWaterTerrainFromFall` | `PhysicsMutators.hpp:~1219` (gone) | done — C1 |
| `createWaterTerrainBelowVapor` | `PhysicsMutators.hpp:1460` | pending — C3 (was missed by original audit) |
| `createWaterTerrainFromGravityFlow` | `PhysicsMutators.hpp:~1410` (gone) | done — C2 |
| `createAndRegisterVaporEntity` | `PhysicsMutators.hpp` (deleted) | done — C4.1, closed by C4.3 (function and its lone consumer event/handler all removed once `onMoveGasEntityEvent` became ON_GRID_STORAGE-aware) |
| recovery branch in `onWaterFallEntityEvent` | `PhysicsEngine.cpp:2201` | pending — addressed by merge-fix E2.1, not C1–C4 |
| `createEnttForTerrain` (called from `setVelocity`) | `TerrainGridRepository.cpp:407` | pending — F1 |
| `ensureActive` (lazy-create) | `TerrainGridRepository.cpp:438` | pending — F1 |
| ~~"line 1548"~~ (was a stale pointer to a non-existent `createWaterTerrainFromSpread`) | — | dropped — function never existed |

After C1+C2 (done) and merge-fix E2.1 (pending): **6 sites remain** — 3 in the
vapor-creation cluster (C4: lines 116, 175, 1672), 1 in the water-from-vapor path
(C3: line 1460), and 2 in `TerrainGridRepository.cpp` (F1: lines 407, 438). The
`dropEntityItems` call at `PhysicsMutators.hpp:729` is out of scope (non-water, items).

**`Position` ECS component for terrain is redundant:** the coordinate is already the lookup
key in `byCoord_`, in `MovingComponent.movingFromX/Y/Z`, and in the VDB.

**`byCoord_`/`byEntity_` tracking maps** exist only to associate entity handles with
coordinates. Once terrain has no entities, these maps are empty for terrain and can be removed.

**Non-terrain entities are unaffected.** Beasts, plants, items — anything with `Inventory`,
`DigestionComponent`, `PerceptionComponent`, `OnTakeItemBehavior` — remain pure ECS. Terrain
with `TileEffectsList` remains the only terrain exception (keeps its ECS entity for visual
effects, confirmed at `PhysicsMutators.hpp:360-363`).

**Migration-period dual-loop strategy (C-Pre.1 + C-Pre.2):** During the transition window
between Task B (complete) and C1–C4 (not yet started), `processPhysics` must serve two
populations of velocity-bearing voxels simultaneously:

1. **ECS-backed terrain** — still has an entity with a `Velocity` component (pre-C1–C4 state).
   Driven by the existing `registry.view<Velocity>()` loop.
2. **`ON_GRID_STORAGE` terrain** — no entity; velocity stored only in VDB `velXGrid/Y/Z`
   grids (post-C1–C4 state). Invisible to `registry.view<Velocity>()`.

The fix is a **parallel VDB iteration path**: add `iterateVelocityVoxels(Callback)` to
`TerrainStorage`/`TerrainGridRepository` (Task C-Pre.1), then call it from `processPhysics`
as a second loop after the ECS view (Task C-Pre.2). Deduplication is by
`getTerrainIdIfExists(x,y,z) >= 0`: if an entity exists, Loop 1 already handled it; skip.

**VDB activation contract:** `setVelocity` must call `velXGrid->tree().setValue(coord, vx)`
(activates the voxel in the sparse tree, even at 0.0 if the voxel is still moving).
Zeroing-out must call `velXGrid->tree().setValueOff(coord, 0.0f)` to deactivate the voxel
and remove it from the iteration. This ensures `cbeginValueOn()` / `cbeginLeaf()` yields
exactly the moving voxels — no dead entries, no missed entries.

**Relationship to crash fix plan:** Tasks 5a, 5b, 5c, 6a, 6b in
`2026-04-30-water-sim-crash-fix-plan.md` are superseded by Phases A–D of this plan. Tasks 5a
and 5b are stop-gap symptom fixes; this plan eliminates the root cause. Task 5c
(coord-based `WaterFallEntityEvent`) maps directly to Phase D here.

---

## Task Tracking

> **Pending-priority ordering (2026-05-02).** With the water-creation cluster (C1, C2, C3)
> done and the vapor-collision invariants closed (C3.6, C3.9), the next migration target is
> **vapor creation (C4)**. Three pending tasks form the vapor track and are promoted to the
> top of the pending list — C4 (the principal migration), C3.7 (defensive guard against
> vapor-at-spring), and C3.4 (regression coverage with `simulate_water_evaporation=True`).
> Migrating C4 should also flush out residual TOCTOU/invariant inconsistencies that the
> C3.6/C3.9 retry guards currently absorb. Done tasks are kept at the top of the table
> in their original dependency order.

| # | Task | Status | Can run in parallel with | Notes |
|---|------|--------|--------------------------|-------|
| A | Velocity to VDB (TerrainStorage + TerrainGridRepository) | `done` | B | Phase A + B are independent; ship together before C |
| B | MovingComponent to coord-keyed map (TerrainGridRepository) | `done` | A | |
| C-Pre.1 | `iterateVelocityVoxels` on `TerrainStorage` / `TerrainGridRepository` | `done` | — | Prerequisite for C-Pre.2; makes VDB velocity voxels iterable |
| C-Pre.2 | Dual-loop `processPhysics`: ECS view + VDB terrain loop | `done` | — | Prerequisite for C1–C4; depends on C-Pre.1 |
| C1 | Remove `registry.create()` from `createWaterTerrainFromFall` | `done` | C2, C3, C4 | Retro-hardened by merge-fix E1.1 (additive + vapor-safe) on 2026-05-02 |
| C2 | Remove `registry.create()` from `createWaterTerrainFromGravityFlow` | `done` | C1, C3, C4 | Done 2026-05-02; scenario module + dispatch binding shipped, function returns `void`, entity-reuse branch dropped |
| C3 | Remove `registry.create()` from `createWaterTerrainBelowVapor` (water from vapor condensation) | `done` | C1, C2, C4 | `registry.create()` removed + additive contract shipped; all C3.x follow-ups done (C3.0–C3.9). C4 closes the remaining vapor-creation cluster |
| C3.0 | Refactor: two-level extraction of `processPhysicsAsync` + `processPhysics` Loops 1 and 2 (six new helper methods on `PhysicsEngine`) | `done` | — | Done 2026-05-02 (commit `6055fa2`) — byte-identical extraction. Six new helpers: `applyGravityForcesToECSEntities`/`applyGravityForceToEntity` (Async); `processVelocityForECSEntities`/`processVelocityForEntity` (Loop 1); `processVelocityForVDBVoxels`/`processVelocityForVoxel` (Loop 2). Loop 3 (MovingComponent) untouched. Build GREEN, game runs identically. Seams are now in place for C3.1's velocity-grid sibling helper |
| C3.1 | Velocity-grid-driven gravity discovery (seed velocity at creation + wake-up neighbors on deletion) | `done` | C3.1.1 | Shipped 2026-05-02 in commit `efda44c`. Seed-on-creation in all three water-creation paths + caller-level wake-up cascade via `PhysicsEngine::nudgeSettledWaterAfterDrain` |
| C3.1.1 | Defect V3: propagate velocity across `moveTerrain` (prerequisite for C3.1 multi-cell falling) | `done` | C3 | Shipped 2026-05-02 in commit `efda44c`. `setVelocity(to, sourceVelocity)` in `moveTerrain`'s success branch — multi-cell falling works in the live game |
| ~~C3.2~~ | ~~Coord-based path in `onMoveSolidLiquidTerrainEvent`~~ | `dropped` | — | Not needed under C3.1's velocity-driven strategy — gravity flows through `processPhysics` Loop 2, not via a new event |
| C3.6 | Retry-then-abort guard for falling water onto unresolved vapor (fixes both-WaterMatter-and-WaterVapor invariant violation) | `done` | — | Shipped 2026-05-02 in commit `a3c6b97`. `WaterFallEntityEvent.retryCount` field; re-dispatches up to `WATER_VAPOR_CONFLICT_RETRY_LIMIT = 3` when the 4-neighbor redirect scan finds no EMPTY/liquid-water target; abort with `warn` log on exhaustion |
| C3.8 | Move terrain-deletion wake-up out of `TerrainGridRepository` (caller-level validation, no storage-emitted event) | `done` | C3.1 | Shipped 2026-05-02 in commit `efda44c`. `TerrainDeletedEvent` deleted, `moveTerrain` reverted to single-arg signature, `nudgeSettledWaterAfterDrain` invoked directly by `processVelocityForVoxel`. Storage layer is pure CRUD again |
| C3.9 | Retry-then-abort guard for condensation onto unresolved vapor (twin of C3.6 in `createWaterTerrainBelowVapor`) | `done` | — | Shipped 2026-05-02 in commit `a3c6b97`. `CondenseWaterEntityEvent.retryCount` shares the `WATER_VAPOR_CONFLICT_RETRY_LIMIT = 3` cap with C3.6; re-dispatch on TOCTOU vapor-at-destination, warn-log + return on exhaustion |
| **C4** | **Remove `registry.create()` from vapor entity creation (3 sites)** | **`done (sites 1, 2, 3 all closed)`** | **C3.7, C3.4, R** | All three sites resolved across two changes. Sites 1 and 2 converted to ON_GRID_STORAGE writers (`void` return). Site 3 (`createAndRegisterVaporEntity`) closed by C4.3, which removed every caller — the gas-movement path now operates coord-based on ON_GRID_STORAGE vapor, so synthesising an EnTT entity solely to satisfy `MoveGasEntityEvent` is no longer needed. Original `Entity has both WaterMatter and WaterVapor` crash is gone. New `Resource deadlock avoided` crash tracked under **C4.2** |
| **C4.1** | **Decouple `createAndRegisterVaporEntity` from `MoveGasEntityEvent`** | **`done (closed by C4.3)`** | **C4** | Resolved as a side effect of C4.3: once `onMoveGasEntityEvent` accepts ON_GRID_STORAGE entities, `moveVaporUp` and `moveVaporSideways` stop creating EnTT entities altogether, so `createAndRegisterVaporEntity` and `_handleCreateVaporEntityEvent` lost all callers. Both helpers, the `CreateVaporEntityEvent` struct, and the handler/registration were deleted in the same change |
| **C4.2** | **Make `TerrainGridLock` recursion-safe** | **`pending`** | — | Live-game crash signature changed from "Entity has both WaterMatter and WaterVapor" → "Resource deadlock avoided" after C4 sites 1+2 went in. Root cause: `TerrainGridLock`'s constructor calls `lockTerrainGrid()` unconditionally on a non-recursive `std::shared_mutex`. Same-thread re-acquisition throws `std::system_error(EDEADLK)`. Pre-C4 nesting was rare; post-C4 the increased volume of ON_GRID_STORAGE vapor handling exposes it. Fix: have the `TerrainGridLock` constructor and `lockTerrainGrid()` check `currentThreadHoldsTerrainGridLock()` (already a thread_local in the class) and treat re-acquisition as a no-op. Callers that genuinely want to know they're top-level can keep the existing `isTerrainGridLocked()` global atomic |
| **C4.3** | **Make `moveVaporUp` / `moveVaporSideways` route ON_GRID_STORAGE vapor through a working branch** | **`done`** | — | Done in this commit. `validateVaporTerrainId` no longer rejects ON_GRID_STORAGE. `moveVaporUp` and `moveVaporSideways` dispatch `MoveGasEntityEvent` directly with the ON_GRID_STORAGE-as-entity sentinel — no `CreateVaporEntityEvent` redirect, no `createAndRegisterVaporEntity` call. `onMoveGasEntityEvent` is now coord-aware: ECS Position/MovingComponent operations are skipped when there is no entity backing, and direction-check reads use `terrainGridRepository->getMovingComponent` instead of `registry.get<MovingComponent>`. Dead vapor-creation event/handler/struct removed (C4.1 closed implicitly) |
| **C3.7** | **SpringWaterSystem invariant guard** | **`done`** | — | Defensive check in `spring_water.py`: when the spring source cell already holds `water_vapor > 0`, skip the injection and log a warning. Confirmed root cause of the post-C4.3 invariant crash — the spring sits near max altitude where rising vapor accumulates, and the previous unconditional `mc.water_matter += 1` produced cells with both `WaterMatter > 0` and `WaterVapor > 0`, which `processTileWater` rejects. Side effect: the spring effectively pauses while vapor lingers at the source coord; if that becomes a balance issue, swap to a displacement model later (clear vapor before incrementing matter) |
| **C3.4** | **Mountain-side regression with evaporation enabled** | **`pending`** | **C4** | **Vapor track — regression coverage.** New sibling test that flips `simulate_water_evaporation = True`; verifies the C3 + C4 vapor path under real-world load. Needed before declaring vapor migration done |
| C3.3 | Audit ECS-only physics loops touching terrain | `pending` | C3.0 | Output: list every `registry.view<X>()` in physics that needs a parallel VDB iteration. Easier to write after C3.0 extracts loop bodies. Likely informs C4 by surfacing any vapor-only ECS loops |
| C3.5 | Mid-air falling-water reference scenario + binding | `pending` | — | New `water_fall_from_midair_world_factory`; used by C3.1's test, loadable in the live game |
| R | Refactor: extract water-cluster mutators to `src/physics/mutators/WaterPhysicsMutators.cpp` | `in-progress` | C1–C4 | Header keeps declarations; bodies move byte-identical. 4 of 13 done in commit `0bf0b69`; remaining 9 migrate one-at-a-time per safe-slice protocol. Several remaining mutators are vapor-cluster (`createVaporTerrainEntity`, `addOrCreateVaporAbove`, `createAndRegisterVaporEntity`, `_handleCreateVaporEntityEvent`, `_handleVaporMergeSidewaysEvent`) — coordinate with C4 to move them in the same commit since the bodies need editing for both anyway |
| **R.1** | **Band-aid: zero-init `destMatter` in `createWaterTerrainFromFall` when destination is NONE** | **`done`** | — | Identified by Task R Phase 3 instrumentation. Adopted `_handleWaterCreationEvent`'s conditional-read pattern: `MatterContainer destMatter{}; if (!destinationIsEmpty) { destMatter = repo.getTerrainMatterContainer(...); }`. Prevents the additive write atop orphan WaterVapor; the subsequent `setTerrainMatterContainer` also overwrites the orphan grid value, sweeping the residue clean as a side effect. Closes the immediate `Entity has both WaterMatter and WaterVapor` crash signature observed at coord (49, 49, 2) on 2026-05-03 |
| **R.2** | **Find the upstream path that leaves orphan `WaterVapor > 0` in `terrainId == NONE` cells** | **`pending`** | **R.1** | The deeper bug. R.1 is a band-aid at the read-side; the writer is still out there. Hypothesis: some path sets terrainId to NONE (or calls `voxelGrid.setTerrain(... NONE)` / `softDeactivateEntity`) without going through `setGravityFlowEmptySourceDefaults`, `convertTerrainTileToEmpty`, or `setEmptyWaterComponentsStorage` — the three known mutators that *do* zero matter. Plan: add a symmetric `_logIfOrphanMatterAfterDelete(callerName, x, y, z)` probe at every place that sets terrainId to NONE / soft-deactivates a cell with non-zero matter; first hit identifies the offender. Use the same parameter-only / nullptr-guarded / no-throw pattern from R.1 |
| **R.3** | **Migrate the 7 ⚠️-partial rows to Rule 4b (mechanically enforced outgoing-write check)** | **`pending`** | **R.1, R.2** | Phase 2's second-pass audit identified 7 mutators that *preserve* pre-existing both-fields-positive violations rather than creating them: `_handleWaterCreationEvent`, `_handleWaterSpreadEvent`, `_handleWaterGravityFlowEvent`, `createWaterTerrainFromFall`, `createWaterTerrainBelowVapor`, `onCondenseWaterEntityEvent` Path 1, `onEvaporateWaterEntityEvent`. With R.1 + R.2 closing the orphan-vapor source, the remaining ⚠️ rows can be upgraded from "log" to "throw" once R.2 confirms no more violations are produced; until then, the `_logIfViolatingMatterWrite` calls already in place from Phase 3 keep the diagnostic surface |
| D | Refactor `WaterFallEntityEvent` + handler to be coord-based | `in-progress (E2.1 + E2.2 + handler simplification done; E1.3/E1.4/E2.3 still pending)` | — | `onWaterFallEntityEvent` is now coord-based — `registry.create()` recovery branch deleted (E2.1), entity field treated as advisory only (E2.2), centralized stale-cell recovery via `_recoverStaleTerrainCellIfTransitory`. Remaining merge-fix work: E1.3 (additive merge into populated destinations), E1.4 (re-emit when source no longer has water), E2.3 (funnel all bail-outs through a re-emission primitive) |
| E | Remove `registry.destroy()` from terrain deletion paths | `pending` | — | Depends on C1–C4 + D; merge-fix E2.1 lands in D's scope first |
| F1 | Remove ECS hooks (`onConstructVelocity`, `onConstructMoving`) from terrain | `pending` | F2, F3 | Depends on A + B; keep hooks for non-terrain |
| F2 | Remove `byCoord_`/`byEntity_` tracking maps for terrain | `pending` | F1, F3 | Depends on C1–C4 |
| F3 | Remove `registry_.emplace<Position>` / `registry_.remove<Position>` from `moveTerrain` | `pending` | F1, F2 | Depends on C1–C4 |
| G | Update `createEntityFromPython` to write terrain directly to VDB (no entity) | `pending` | — | Depends on B; supersedes crash-fix tasks 6a + 6b |

---

## Required Nanobind Additions

Before writing the tests for Tasks A, B, E, and G, expose these methods in
`src/aetherion.cpp`. Add them to the existing `TerrainGridRepository` and `World` binding
blocks. These are thin wrappers — no new C++ code needed — and they enable Python-level
observation of state that would otherwise require digging into ECS internals.

> **Status (2026-05-02):** §1 and §2 are **shipped** (covered by Tasks A and B). §3
> (`World.get_alive_entity_count`) is **still pending** for Tasks E and G. Three
> additional bindings shipped under the gravity-flow / fall-event work and are reusable
> here: `World.dispatch_water_fall_event(source_pos, dest_pos, falling_amount, entity=-2)`,
> `World.dispatch_water_gravity_flow_event(source_pos, target_pos, amount, target_terrain_id=-2)`,
> and `TerrainGridRepository.sum_total_water() -> int64`. Voxel-placement primitives
> (`place_stone`, `place_water`, `place_empty`, `water_matter`, `fall_event_position`,
> `make_position`) live in `aetherion.reference.world.scenarios.primitives` and are
> re-exported by `tests/reference/helpers.py`. Hand-crafted scenario factories live in
> `aetherion.reference.world.scenarios` (currently: `water_gravity_flow_world_factory`)
> and are loadable via `WorldManager` for experiential validation in the live game.

**1. ✅ `TerrainGridRepository.get_terrain_velocity(x, y, z) → tuple[float, float, float]`** _(done — Task A)_
Wraps the existing `TerrainGridRepository::getVelocity(x, y, z)` method (declared in
`TerrainGridRepository.hpp:154`). Needed by Task A to assert velocity is stored in and
readable from VDB without ECS involvement.

```cpp
// In the nb::class_<TerrainGridRepository> block in src/aetherion.cpp
.def("get_terrain_velocity",
     [](const TerrainGridRepository &repo, int x, int y, int z)
         -> std::tuple<float, float, float> {
       Velocity v = repo.getVelocity(x, y, z);
       return {v.x, v.y, v.z};
     })
```

**2. ✅ `TerrainGridRepository.has_terrain_moving_component(x, y, z) → bool`** _(done — Task B)_
Wraps `TerrainGridRepository::hasMovingComponent(x, y, z)` (`TerrainGridRepository.hpp:180`).
Needed by Task B to assert moving state lives in the coord-keyed map, not the ECS registry.

```cpp
.def("has_terrain_moving_component",
     [](const TerrainGridRepository &repo, int x, int y, int z) -> bool {
       return repo.hasMovingComponent(x, y, z);
     })
```

**3. ⏳ `World.get_alive_entity_count() → int`** _(pending — needed for Tasks E and G)_
Wraps `registry.alive()` via `World`. Needed by Tasks E and G to assert the registry size
does not grow with water/terrain creation. Expose it through a lambda on `World`:

```cpp
// In the nb::class_<World> block in src/aetherion.cpp
.def("get_alive_entity_count",
     [](World &w) -> int {
       return static_cast<int>(w.getRegistry().alive());
     })
```

`World::getRegistry()` must be added (or already exist) to expose the registry reference.
If `getRegistry()` does not exist, expose via `VoxelGrid`'s registry reference instead.

> **Performance note:** All three are read-only queries on existing data structures. No
> allocation, no locking beyond what the underlying methods already do. Safe to call from
> Python test assertions after each `manager.update()` call.

---

## Task Details

> **TDD Guidance (see `.claude/skills/aetherion-tdd.md`):** The goal is confidence, not
> ceremony. Two valid patterns exist here:
>
> - **RED → GREEN** (preferred for new behaviour): write the failing test first, confirm RED,
>   implement, confirm GREEN. Use this for tasks that add observable state (D, E, G) or expose
>   new bindings (A, B).
> - **GREEN → GREEN** (acceptable for pure refactoring): when moving an implementation detail
>   between storage backends without changing observable behaviour (e.g., Task A moves velocity
>   from ECS to VDB — the water still flows the same way), the existing passing tests are the
>   safety net. Write the new targeted test alongside the implementation; confirm it was never
>   failing if the behaviour is truly equivalent.
>
> In both cases: run `make build-install-test` before marking a task done, and test through
> the Python public surface — never assert on raw EnTT entity IDs or registry internals.

---

### Task A — Velocity to VDB

**Goal:** `TerrainGridRepository::getVelocity(x,y,z)` reads from three `FloatGrid` VDB
grids instead of an ECS `Velocity` component. `setVelocity(x,y,z, vel)` writes to VDB
without calling `ensureActive()` or touching the registry. Public API is unchanged.

**Files:**
- `src/terrain/TerrainStorage.hpp` — add `velXGrid`, `velYGrid`, `velZGrid` (`FloatGrid::Ptr`)
  and their thread-local accessors to the `ThreadCache` struct
- `src/terrain/TerrainStorage.cpp` — initialize grids in constructor/initializer, implement
  `getVelocity(x,y,z)` / `setVelocity(x,y,z, vx, vy, vz)` methods; clear velocity grids in
  `deleteTerrain()` so movement state is wiped on deletion
- `src/terrain/TerrainGridRepository.hpp` — update `getVelocity` / `setVelocity` declarations;
  add private `getVelocityFromVDB` / `setVelocityToVDB` helpers
- `src/terrain/TerrainGridRepository.cpp` — rewrite `getVelocity` and `setVelocity` bodies
  to call VDB helpers; remove the `ensureActive()` call in `setVelocity` (line 788)

**Do NOT remove the `onConstructVelocity` / `onDestroyVelocity` hooks yet** — they still fire
for non-terrain entities (beasts with velocity). Removal is in Task F1.

**Test:** `tests/reference/test_velocity_vdb.py` (shipped). Two tests against the
`mountain_side_world_factory`: (1) after spring water has fallen for a few ticks,
`get_terrain_velocity` at the moving voxel returns a real float without crashing the ECS
path; (2) static grass at `(50, 50, 0)` reports `(0, 0, 0)` — VDB background value, no
ghost activations.

**Acceptance:** both tests GREEN; `setVelocity` no longer appears in callgrind under
`ensureActive` → `registry.create`; `velXGrid.memUsage()` grows only when water is moving.

---

### Task B — MovingComponent to coord-keyed map

**Goal:** `TerrainGridRepository` owns a `std::unordered_map<VoxelCoord, MovingComponent,
VoxelCoordHash> movingByCoord_` protected by `terrainGridMutex`. Public
`setMovingComponent`, `getMovingComponent`, `hasMovingComponent`, `clearMovingComponent`
methods replace ECS-backed access. `moveTerrain` reads/writes the map instead of the
registry.

**Files:**
- `src/terrain/TerrainGridRepository.hpp` — add `movingByCoord_` member; add four public
  method declarations; update `hasMovingComponent` declaration
- `src/terrain/TerrainGridRepository.cpp` — implement the four new methods (all protected by
  `withUniqueLock`); rewrite `moveTerrain` (lines 66–189) to:
  - replace `registry_.remove<MovingComponent>(terrainID)` at line 84 with
    `clearMovingComponent(movingComponent.movingFromX, ...)`
  - update `hasMovingComponent(x,y,z)` at line 979 to check `movingByCoord_` count instead
    of ECS query

**Do NOT remove `onConstructMoving` / `onDestroyMoving` hooks yet** — Task F1.

**Test:** `tests/reference/test_moving_component_vdb.py` (shipped). Two tests:
(1) `has_terrain_moving_component(50, 50, 0)` is `false` for static grass; (2) after 50
ticks, settled water voxels (zero velocity) report no `MovingComponent` in the map.

**Acceptance:** tests GREEN; `registry.emplace<MovingComponent>` / `registry.remove<MovingComponent>`
no longer appear in the water simulation callstack; `movingByCoord_` size is 0 at end of run.

---

### Task C-Pre.1 — `iterateVelocityVoxels` on `TerrainStorage` / `TerrainGridRepository`

**Goal:** Add a `iterateVelocityVoxels(Callback)` method that walks every VDB voxel that
currently carries a non-zero velocity. This is the foundation for Loop 2 in `processPhysics`
(C-Pre.2). The method must be efficient: it iterates only the sparse active set of `velXGrid`,
not the full grid.

Additionally, harden `setVelocity` / clearing in `TerrainStorage` to honour the activation
contract (see Confirmed Facts above):
- **Setting** any component of velocity → `tree().setValue(coord, value)` — marks the voxel
  active in the VDB sparse tree.
- **Zeroing out** all three components → call `tree().setValueOff(coord, 0.0f)` on each grid
  — removes the voxel from the active set so iteration skips it automatically.

**Files:**
- `src/terrain/TerrainStorage.hpp` — declare `iterateVelocityVoxels(Callback)` template (or
  a concrete variant using `std::function<void(int,int,int,float,float,float)>`)
- `src/terrain/TerrainStorage.cpp` — implement by iterating `velXGrid->cbeginValueOn()`;
  for each active coord read `velY` and `velZ` from their grids and call the callback; also
  tighten `setVelocity` to use `setValueOff` when all three components are zero
- `src/terrain/TerrainGridRepository.hpp` — declare `iterateVelocityVoxels(Callback)` as a
  thin pass-through to `storage_.iterateVelocityVoxels(callback)`
- `src/terrain/TerrainGridRepository.cpp` — implement the pass-through

**Test:** `tests/reference/test_velocity_vdb_iteration.py` (shipped — 5 tests). Asserts
that `iterateVelocityVoxels` yields exactly the voxels written via `set_terrain_velocity`
(zero on fresh world; activated on set; deactivated when zeroed; counted correctly across
multiple voxels; round-trips via `get_terrain_velocity`). A temporary diagnostic binding
`count_active_velocity_voxels()` drives the assertions and stays for now (still useful
for C3.x diagnostics; remove once those settle).

**Acceptance:** all 5 tests GREEN; `velXGrid->activeVoxelCount()` matches the count.

---

### Task C-Pre.2 — Dual-loop `processPhysics`: ECS view + VDB terrain loop

**Goal.** Extend `processPhysics` with Loop 2 that handles voxels whose velocity lives
only in VDB (`ON_GRID_STORAGE`). After C1–C4, all water is in this category.

**Loop 2 contract:** call `iterateVelocityVoxels`, dedup-skip if
`getTerrainIdIfExists(x, y, z) >= 0` (already serviced by Loop 1), apply the same
gravity/friction/clamp/movement as Loop 1 but coord-based, then `setVelocity` (which
auto-deactivates via `setValueOff` when zero). Two-phase under the terrain grid lock:
collect coords first, then process outside.

**Implementation (shipped):**
- `PhysicsEngine.cpp` — Loop 2 added after the ECS view in `processPhysics`. Reuses
  `resolveVerticalMotion`, `applyKineticFrictionDamping`, `calculateMovementDestination`,
  `hasCollision`, `moveTerrain` with `entt::null` (terrain helpers read from VDB when
  `isTerrain=true`).
- `TerrainGridRepository.cpp` — `moveTerrain` guards all ECS operations behind
  `const bool hasEntity = terrainID >= 0`; VDB operations always run.

**Test:** `tests/reference/test_physics_dual_loop.py` — (1) Loop 2 yields nothing when
no VDB velocity is written; (2) seeded VDB velocity on an ON_GRID_STORAGE voxel runs the
full gravity/friction/move pipeline. Dedup naturally testable post-C1–C4 (mountain world
has no ECS-backed terrain pre-migration).

---

### Tasks C1–C4 — Remove `registry.create()` from water creation (parallel batch)

**C1, C2, C3, C4 can be executed by four separate agents or sequentially in one session.**
They share the same pattern but touch different functions. All depend on A + B being
merged first.

> **Additive contract (post-merge-fix E1.1).** C1 was retro-hardened on 2026-05-02 to
> be additive on the destination matter container (`WaterMatter += fallingAmount`,
> scaffolding writes gated on `destinationIsEmpty`, vapor-only safeguard with
> 4-cardinal redirect). **C2, C3, C4 must adopt the same contract** when they remove
> `registry.create()` — read the existing destination matter, add to it, and gate the
> Position/EntityType/SIC/PhysicsStats writes on `destinationIsEmpty`. Today most of
> these paths are reached only on empty destinations (so the additive vs. overwrite
> distinction is invisible), but the contract is mandatory for consistency and to
> avoid regressing the merge-fix epic.

**Per-task TDD strategy:**

Each C-task targets one specific water-creation path. Instead of one shared 200-step
mountain-side test, each task gets its **own minimal world + scenario** that exercises
that path in 1–5 steps. This makes each test:

- **Fast** (no need to run 200 steps and wait for the scenario to arise organically).
- **Focused** (failure points to the specific path being changed; no cross-talk).
- **Deterministic** (small handcrafted world; no spring randomness, no neighbor noise).

A new helper `build_minimal_test_manager(width, height, depth, *, gravity=5.0, friction=1.0)`
should be added to `tests/reference/helpers.py`. It builds a `WorldManager` against an
**empty grid** (no spring, no preset terrain) using a `WorldInstanceTypes.SYNCHRONOUS`
factory — call sites then place stone/water/vapor explicitly via the repository API.

A second helper `place_water(voxel_grid, x, y, z, *, water_matter=1000)` should encapsulate
the multi-step incantation needed to materialize a water voxel as ON_GRID_STORAGE without
an ECS entity (set terrain id = -1, entity type = TERRAIN/WATER, matter container with
water_matter, matter state = LIQUID, physics stats). A `place_stone(...)` analogue covers
the immovable-floor case.

Each task's test asserts the **specific behavior change** that task introduces (water lands,
flows, spreads, evaporates) **and** that the resulting voxel(s) have `terrain_id == -1`.
Tests are RED before the task's implementation and GREEN after.

> A broader 200-step regression check already lives in
> `tests/reference/test_mountain_side_regression.py`; once C1–C4 are merged that test
> indirectly proves all four paths cooperate. No separate `test_no_water_entities.py`
> is needed.

#### C1 — `createWaterTerrainFromFall` (`PhysicsMutators.hpp:1207–1295`)

**Goal:** Remove `registry.create()` at line 1219 and `registry.emplace<Position>` at
line 1221. Water is created as a pure VDB write. Return type changes from `entt::entity` to
`void` (or returns a sentinel `VoxelCoord`).

**Steps:**
1. Remove lines 1219–1221 (`registry.create` + `emplace<Position>`).
2. Replace `setTerrainId(x, y, z, static_cast<int>(newWaterEntity))` with
   `setTerrainId(x, y, z, static_cast<int>(TerrainIdTypeEnum::ON_GRID_STORAGE))`.
3. Remove `addToTrackingMaps(...)` call (no entity to track).
4. All `terrainGridRepository->set*` calls that follow are unchanged — they already write VDB.
5. Return type: change to `void`; update all call sites.

**Files:** `src/physics/PhysicsMutators.hpp` (lines 1207–1295), `src/PhysicsEngine.cpp`
(call site — update to not capture return value)

**Test:** `tests/reference/test_c1_water_fall.py` (shipped) — minimal 5×5×5 world,
stone floor at z=0, single water voxel placed mid-air with downward velocity. After
2–5 steps, water has landed at z=1 with `terrain_id == -1`.

##### C1 follow-ups (live-game integration, all fixed in same commit)

1. **`VoxelGrid::setTerrain` rejects `-1` / `-2`** — replaced with the
   `terrainGridRepository->setTerrainId(..., ON_GRID_STORAGE)` write path.
2. **`processTileWater` throw on stale ON_GRID_STORAGE water** during fast
   cascades: temporarily commented out as a soft skip. Permanent home is
   merge-fix **E1.4 + E2.3** — once the `deferWaterFallEvent` re-emission
   primitive lands, transient stale states become "retry next tick" and the
   throw can be reinstated as a hard invariant.
3. **`moveWater` sentinel propagation:** `terrainEntityId == -1`/`-2` now
   forwards as `static_cast<entt::entity>(terrainEntityId)` (not `entt::null`)
   so the fall handler can distinguish "no entity, but real voxel" from "no terrain".

---

#### Gravity-flow water creation _(done 2026-05-02 — was C2)_

**What shipped.** `createWaterTerrainFromGravityFlow` is now `void`, drops `registry`,
`sourceEntity`, `reuseSourceEntity`, `sourcePos` parameters. The three old branches
collapse into a single scaffolding-gated write:
```cpp
bool destinationNeedsScaffolding =
    targetTerrainId == NONE
    || existingType.mainType != TERRAIN
    || existingType.subType0 != WATER;
if (destinationNeedsScaffolding) {
  setGravityFlowWaterTargetDefaults(voxelGrid, gravityTargetPos, sourcePhysicsStats);
}
setTerrainId(..., ON_GRID_STORAGE);
```
No `registry.create()` / `createEnttForTerrain` / `addToTrackingMaps` / entity reuse.
The matter merge is owned by `_handleWaterGravityFlowEvent` (already additive); vapor
safeguard not needed (caller already aborts on vapor target,
`PhysicsMutators.hpp:1703-1707`).

`_handleWaterGravityFlowEvent` simplified — `sourceEntity`, `reusedSourceEntity`,
`sourceWillBecomeEmpty`, and the `if (!reusedSourceEntity)` deletion guard removed.

The originally-planned `storage_.copyVoxel` entity-reuse optimization was unnecessary —
the reuse branch was an artifact of the old ECS-only design (moving source entity
handle), and water having no entity post-decoupling deletes it cleanly.

**Reference scenario infrastructure shipped:**
- `src/aetherion/reference/world/scenarios/` package with placement primitives
  (`place_stone`, `place_water`, `place_empty`, `water_matter`, `fall_event_position`).
- `water_gravity_flow_world_factory` (3x3x3, water source at `(1,1,1)`, NONE at `(1,1,0)`).
- `World.dispatch_water_gravity_flow_event` binding (parallel to `dispatch_water_fall_event`).

**Tests.** `tests/reference/test_water_gravity_flow.py` 2/2 GREEN (lands as
`-1`, conserves `sum_total_water()`); `test_mountain_side_regression.py` 2/2 GREEN.

---

#### C3 — `createWaterTerrainBelowVapor` (water from vapor condensation)

> **Retargeted 2026-05-02.** The original C3 description pointed at a function called
> `createWaterTerrainFromSpread` at line 1548 — that function has never existed in the
> codebase (verified via `git log -S createWaterTerrainFromSpread` returning empty). The
> spread path runs through `_handleWaterSpreadEvent` which is purely additive and
> entity-free. The actual third water-creation site is `createWaterTerrainBelowVapor`
> (`PhysicsMutators.hpp:1434–1532`), which condenses vapor into liquid water at z-1 and
> calls `registry.create()` at line 1460.

**Goal:** Remove `registry.create()` and `registry.emplace<Position>` from
`createWaterTerrainBelowVapor`. Adopt the additive contract from C1 (merge-fix E1.1):
read existing destination matter, add `condensationAmount`, gate scaffolding writes on
`destinationIsEmpty`, write `setTerrainId(ON_GRID_STORAGE)`. Source vapor decrement
stays inside the same lock scope. This is also where merge-fix **E1.2** lives — both
land in this single task.

**Steps:**
1. Remove `registry.create()` + `registry.emplace<Position>` (lines ~1461–1463).
2. Remove `addToTrackingMaps` call (no entity to track).
3. Replace `setTerrainId(x, y, z, static_cast<int>(newWaterEntity))` with
   `setTerrainId(x, y, z, static_cast<int>(TerrainIdTypeEnum::ON_GRID_STORAGE))`.
4. Read existing destination matter; replace `WaterMatter = condensationAmount` with
   `WaterMatter += condensationAmount` (the merge-fix E1.2 fix).
5. Gate the scaffolding writes (Position, EntityType, SIC, PhysicsStats) on
   `destinationIsEmpty` (per C1's pattern). Same vapor-only safeguard is **not**
   needed here — caller already owns the vapor → liquid invariant.

**Files:** `src/physics/PhysicsMutators.hpp` (lines ~1434–1532).

**Test:** `tests/reference/test_water_condensation.py` (shipped) — 3x3x3 world, vapor
above empty z-1; dispatched via the new `World.dispatch_condense_water_event(vapor_pos,
condensation_amount)` binding (mirrors `dispatch_water_fall_event`). Asserts target
lands `terrain_id == -1`, `sum_total_water()` conserved, additive into a populated
destination. Scenario `water_condense_below_vapor_world_factory` added under
`aetherion.reference.world.scenarios`.

##### C3 follow-up: ON_GRID_STORAGE water doesn't fall under gravity (live-game regression, fixed)

**Symptom (2026-05-02, after C3 commit):** unit + mountain-side tests passed but live
game showed condensation-born water hanging mid-air. **Diagnosis:** two ECS-only gates
in the gravity path — `processPhysicsAsync` (`registry.view<Position>()`) and
`onMoveSolidLiquidTerrainEvent` (`registry.valid(event.entity)`) — both invisible to
ON_GRID_STORAGE voxels. Mountain-side passed only because it inherits ECS entities
from stone cells when seeding water and flows by gradient via the already-coord-safe
`moveWater`. **Resolution:** velocity-grid-driven strategy (C3.0–C3.5, C3.8) below.

**C3.0 — Refactor: two-level extraction (no logic change). DONE — commit `6055fa2`.**

Pure code-organization refactor with strict byte-identical-bodies rule. Six new
private helpers on `PhysicsEngine`, paired by iteration source so a future VDB
sibling reads symmetrically at the call site:

- `applyGravityForcesToECSEntities` / `applyGravityForceToEntity` — `processPhysicsAsync`.
- `processVelocityForECSEntities` / `processVelocityForEntity` — `processPhysics` Loop 1
  (ECS Velocity view).
- `processVelocityForVDBVoxels` / `processVelocityForVoxel` — `processPhysics` Loop 2
  (VDB velocity voxels, from C-Pre.2).

Loop 3 (MovingComponent) intentionally untouched. Only translation was `continue` → `return`
on early-outs in the inner methods. Mountain-side regression + water tests still GREEN, live
game runs unchanged.

**C3.1 — Velocity-grid-driven gravity discovery. DONE — commit `efda44c`.**

Strategy: the velocity grid is the index of voxels that need physics this tick.
Don't iterate all terrain — make sure every voxel that should fall is already in
the velocity grid, and let the C-Pre.2 dual-loop service it. Two parts:

1. **Seed velocity at creation:** in all three water-creation sites
   (`createWaterTerrainFromFall`, `createWaterTerrainFromGravityFlow`,
   `createWaterTerrainBelowVapor`), gated on `checkIfTerrainCanFall`, set
   `{0, 0, -gravity*dt}` after `setTerrainId(ON_GRID_STORAGE)`.
2. **Wake-up cascade on deletion:** caller-level (see C3.8) — when a cell drains,
   inspect `(x, y, z+1)`; if settled water, kick it with a small `-z` velocity.

This is O(M) (sparse moving voxels), needs no new event handler, and reuses the
already-iterable velocity grid.

**C3.1.1 — Defect V3: propagate velocity across `moveTerrain`. DONE — same commit.**

Hidden prerequisite — without it, seeded velocity buys exactly one cell of fall.
`moveTerrain` previously copied type/SIC/matter/physics but not velocity, so the
post-move voxel dropped out of `iterateVelocityVoxels`. Fix: in `moveTerrain`'s
success branch, just before clearing the source, call
`setVelocity(toX, toY, toZ, sourceVelocity)`. Verified in the live game —
multi-cell falling works.

**C3.2 — DROPPED.** Originally proposed adding a coord-based path to
`onMoveSolidLiquidTerrainEvent`. Under the velocity-grid-driven strategy
(C3.1), gravity is not dispatched as an event for ON_GRID_STORAGE voxels —
it's a velocity already in the grid, processed by `processPhysics` Loop 2.
The `MoveSolidLiquidTerrainEvent` handler stays entity-only and is the
domain of ECS-backed terrain (e.g., `TileEffectsList`-bearing voxels). If a
future scenario needs to dispatch a coord-based force, revisit then; for now
the velocity grid covers all known cases.

**C3.3 — Audit other ECS-only physics loops that touch terrain.**

`processPhysicsAsync` is one of several places in `PhysicsEngine.cpp` that
iterates the ECS registry to drive terrain physics. Each one is a
candidate for the same "ON_GRID_STORAGE invisible" bug. Targets to audit:

- `processPhysics` (line 953) — already dual-loop'd by C-Pre.2; verify still
  correct after C3.1 lands.
- `processPhysicsAsync` (line 1342) — fixed by C3.1.
- The `MovingComponent` view at line 1217 — check whether ON_GRID_STORAGE
  voxels need a parallel iteration here.
- Any other `registry.view<Position>` / `view<Velocity>` /
  `view<MovingComponent>` calls in the engine.

**Output:** an audit document (or amendment to this plan) listing every
ECS-only iteration in the physics path and either (a) confirming it doesn't
need a parallel VDB loop (give the reason), or (b) opening a follow-up task
to add one. Done before C3.1's loop is merged so we know the full scope.

**C3.4 — Mountain-side regression with evaporation enabled.**

The existing mountain-side regression test (`test_mountain_side_regression.py`)
disables evaporation (`simulate_water_evaporation = False`) so condensation
never runs. Add a sibling test
`test_mountain_side_with_condensation_advances_water` that flips
`simulate_water_evaporation = True` and `simulate_vapor_movement = True` and
asserts water still advances through the corridor over a meaningful step
budget (e.g., 500 steps). This catches the C3 regression and any future
similar regression caused by condensation-side changes.

**C3.5 — Mid-air falling-water reference scenario.**

Add `water_fall_from_midair_world_factory` to
`aetherion.reference.world.scenarios` — minimal 5×5×10 world, stone floor at
z=0, water voxel at z=8 with no initial velocity. Used by C3.1's test, also
loadable in the live game for visual inspection. Constants:
`MIDAIR_WATER_POS = (2, 2, 8)`, `INITIAL_WATER_MATTER = 1000`.

**C3.6 — Retry-then-abort guard for falling water onto unresolved vapor. DONE — commit `a3c6b97`.**

**Bug.** `createWaterTerrainFromFall`'s vapor-only safeguard scans 4 horizontal
neighbors for an `EMPTY` / liquid-water redirect target; when none match (vapor
sealed by stone or other vapor), the old code fell through and ran
`destMatter.WaterMatter += fallingAmount` against a `destMatter` with
`WaterVapor > 0`. Next tick, `processTileWater`'s invariant check threw
`runtime_error("Entity has both WaterMatter and WaterVapor")` — the
graceful-crash signature observed in `water_gravity_flow_world_factory`.

**Why retry over abort.** Vapor is dynamic — the "no neighbor works" condition is
usually transient (vapor migrates via `moveVapor` / `VaporMergeUpEvent` / sideways).
Pure abort would lose the falling water's momentum (`processVelocityForVoxel` clears
source velocity on collision, so the source becomes "settled on stone" by next tick
and would need a fresh wake-up trigger). Retrying re-dispatches the
`WaterFallEntityEvent` with `retryCount + 1`; cap at 3 so genuinely-sealed pockets
abort with a `spdlog::warn` instead of looping forever. A comment beside the branch
documents that pure abort is a valid fallback if retries ever prove buggy.

**Files:** `WaterFallEntityEvent.retryCount` field added in
`src/ecosystem/EcosystemEvents.hpp`; retry branch in `createWaterTerrainFromFall`
(`src/physics/PhysicsMutators.hpp`).

**Test:** `tests/reference/test_water_fall_event.py` — sealed-vapor scenario
(retry observed via `World.peek_pending_water_fall_events()`; abort warn-log
fires once after 4 updates) + happy-path companion (one neighbor empty → redirect
succeeds, no retry).

**C3.7 — SpringWaterSystem invariant guard.**

**Context.** The reference Python system at
`src/aetherion/reference/systems/spring_water.py:77` performs
`mc.water_matter += 1` and writes back without checking `mc.water_vapor`.
If vapor ever floats up to (or condenses into) the spring source coord,
the spring perpetuates the both-non-zero invariant violation every pace
tick. Pre-C3 this was effectively unreachable in practice because the
spring source was always raised on stone with no path for vapor; with C3
changes vapor distribution is more dynamic, and the spring source coord
can transiently hold vapor. Found in passing while diagnosing C3.6's
root cause.

**Goal.** `SpringWaterSystem` refuses to inject when the source cell
contains vapor; logs the skip exactly once per occurrence (rate-limited
to avoid log spam) and proceeds normally on the next tick. Strictly
defensive — does not _fix_ vapor-at-spring (that's a separate physics
question), only _stops the spring from making things worse_.

**Files:**
- `src/aetherion/reference/systems/spring_water.py` — between the
  existing `mc is None` early-out and the `mc.water_matter += 1` line,
  add `if mc.water_vapor > 0: print(...skip with reason...); return`.

**Test:** `tests/reference/test_spring_water.py` (new file).
- _RED first._ Build a minimal scenario where the spring source coord is
  initialised with `water_vapor = 5, water_matter = 0`. Run
  `manager.update()` once. Assert: source's `water_matter` is still `0`;
  source's `water_vapor` is still `5`; no invariant violation.
- Companion: existing happy path (source coord empty water → `+1` each
  tick) still works after the guard is added.

**C3.8 — Move terrain-deletion wake-up out of `TerrainGridRepository`. DONE — commit `efda44c`.**

**Background.** Interim C3.1 shipped with a `TerrainDeletedEvent` emitted from
`deleteTerrain` / `moveTerrain` and consumed by `PhysicsEngine::onTerrainDeletedEvent`,
which also widened `moveTerrain`'s signature with `entt::dispatcher &`. Working but
wrong layering — the repository is pure CRUD and must not emit domain events.

**Resolution.** Wake-up moved into `processVelocityForVoxel` after the `moveTerrain`
call (and into other in-physics `deleteTerrain` callers as needed). Implementation:

- `PhysicsEngine::nudgeSettledWaterAfterDrain(x, y, z, voxelGrid)` extracted from
  the deleted handler.
- `TerrainDeletedEvent` deleted; `onTerrainDeletedEvent` handler removed; sink
  disconnect dropped from `registerEventHandlers`.
- `moveTerrain` reverted to single-arg `void moveTerrain(MovingComponent &)`;
  `dispatcher.enqueue<TerrainDeletedEvent>` calls removed from `deleteTerrain` and
  `moveTerrain`'s post-source-clear; `#include "physics/PhysicsEvents.hpp"` dropped
  from the repository.

C3.1's observable behaviour (seed at creation, wake column above on drain) is
unchanged — only the layering moves. Storage stays pure CRUD.

**Test:** existing `test_water_gravity_flow.py` + C3.6 retry tests stay GREEN.

**C3.9 — Retry-then-abort guard for condensation onto unresolved vapor. DONE — commit `a3c6b97`.**

**Bug.** Twin of C3.6 on the condensation path. `createWaterTerrainBelowVapor`
unconditionally ran `destMatter.WaterMatter += condensationAmount` against a freshly-read
`destMatter` without checking `destMatter.WaterVapor`. `onCondenseWaterEntityEvent` only
gated on `event.terrainBelowId == NONE` — the event's *enqueue-time snapshot*. Between
enqueue and dispatch the cell can be populated with vapor (cascading `VaporMergeUpEvent`,
or another condensation event firing on a neighbour first), so the late-firing handler
wrote liquid water into a vapor-only cell → invariant throw on next tick.

**Resolution.** `CondenseWaterEntityEvent.retryCount` field added; shares the
`WATER_VAPOR_CONFLICT_RETRY_LIMIT = 3` cap (renamed from C3.6's `FALL_VAPOR_RETRY_LIMIT`)
with the fall path. `createWaterTerrainBelowVapor` takes `int retryCount = 0` and the
existing `dispatcher`; if `destMatter.WaterVapor > 0 && destMatter.WaterMatter <= 0` it
re-dispatches the event with `retryCount + 1`, or warn-logs and returns on exhaustion.
The existing `vaporMatter.WaterMatter > 0` invariant throw on the source vapor cell stays
(it's a separate violation). The non-NONE branch in `onCondenseWaterEntityEvent` already
guards on `matterBelow.WaterVapor == 0` and is unchanged.

**Test:** `tests/reference/test_water_condensation.py` — sealed vapor-vapor scenario
(retry observed via dispatcher peek; abort warn-log fires once after 4 updates) +
happy-path companion (truly-NONE destination → water lands as ON_GRID_STORAGE, no retry).

**Sequencing (remaining C3.x + handoff to C4).**

C3.0, C3.1, C3.1.1, C3.6, C3.8, C3.9 all shipped (commits `6055fa2`, `efda44c`,
`a3c6b97`). C3.2 is dropped (tombstone retained). Remaining order — pivot toward the
vapor-creation migration:

1. **C3.7** — one-line `SpringWaterSystem` vapor guard. Quick win, no dependencies, ship
   first to clear noise during C4 debugging.
2. **C3.3** audit — short read-only inventory of ECS iteration sites against the
   post-C3.0 codebase. Likely surfaces vapor-only ECS loops that C4 needs to handle.
3. **C4** — vapor entity creation migration (3 sites). Principal vapor-track work; should
   flush the residual TOCTOU/invariant inconsistencies that C3.6/C3.9 currently absorb.
4. **C3.4** — mountain-side regression with `simulate_water_evaporation = True`. Needs
   C4 stable to assert correctness on the full vapor path.
5. **C3.5** — mid-air falling-water scenario. Independent; ship whenever convenient.

---

#### C4 — Vapor entity creation (3 sites)

**Goal:** Remove `registry.create()` + `registry.emplace<Position>` from the three
remaining vapor-creation sites. After C4, every vapor voxel is a pure VDB entry like
water (post C1–C3).

**Sites covered (3, not the original 2):**

1. `createVaporTerrainEntity` (`PhysicsMutators.hpp:109–153`, `registry.create()` at line 116)
   — primary vapor creator, called by `addOrCreateVaporAbove`.
2. `createEmptyActiveTerrain` (`PhysicsMutators.hpp:168–212`, `registry.create()` at line 175)
   — creates an EMPTY-typed terrain entity; copy-paste docstring still mentions "vapor",
   but the function actually creates an empty active cell. Audit its callers during C4
   to confirm scope.
3. `createAndRegisterVaporEntity` (`PhysicsMutators.hpp:1651–1678`, `registry.create()`
   at line 1672) — vapor entity creation called by `_handleCreateVaporEntityEvent`.
   **Added 2026-05-02** — original audit at the start of this epic missed it.

> **Note on the original C4/E1.2 cross-reference.** Earlier revisions of this plan
> bundled the `createWaterTerrainBelowVapor` additive-merge fix (merge-fix E1.2) into
> C4. That work has now moved to **C3**: `createWaterTerrainBelowVapor` is itself the
> third water-creation function (water from vapor), so it sits with C1+C2 in the
> water-creation track. C4 is now strictly vapor-creation.

**Per-site steps (same pattern for all 3):**
1. Remove `registry.create()` and `registry.emplace<Position>` calls.
2. Replace `setTerrainId(x, y, z, static_cast<int>(newEntity))` with
   `setTerrainId(x, y, z, static_cast<int>(TerrainIdTypeEnum::ON_GRID_STORAGE))`.
3. Remove `addToTrackingMaps(...)` calls (no entity to track).
4. Function return type changes from `entt::entity` to `void` (or returns `entt::null`
   as a no-op for callers that ignore the return value — audit caller usage first).

**Files:** `src/physics/PhysicsMutators.hpp` (lines ~109, ~168, ~1651).

**Test:** `tests/reference/test_c4_vapor_creation.py` — minimal 5×5×3 world with a high
evaporation coefficient. A single water voxel + a heat boost forces evaporation; after a
few steps a vapor voxel exists, and that voxel is `terrain_id == -1`.

```python
# tests/reference/test_c4_vapor_creation.py
"""C4: createVaporTerrain (evaporation/condensation) must produce ON_GRID_STORAGE vapor."""

from helpers import build_minimal_test_manager, place_stone, place_water

def test_evaporated_vapor_voxel_is_on_grid_storage():
    # Tune the world so evaporation fires within a few steps.
    manager = build_minimal_test_manager(
        5, 5, 3,
        evaporation_coefficient=80.0,
        heat_to_water_evaporation=1.0,
    )
    world = manager.current.world
    vg = world.get_voxel_grid()
    repo = vg.terrain_grid_repository

    # Stone floor at z=0; water sitting on top at z=1 with high heat to trigger evaporation.
    for x in range(5):
        for y in range(5):
            place_stone(vg, x, y, 0)
    place_water(vg, 2, 2, 1, water_matter=500)
    # Inject heat at the water voxel (API name TBD — set via PhysicsStats heat field
    # or a dedicated set_heat helper); see helpers.set_heat for the canonical call.

    for _ in range(10):
        manager.update()

    # Locate any voxel with water_vapor > 0 (evaporation produced one).
    found = None
    for x in range(5):
        for y in range(5):
            for z in range(3):
                m = vg.get_terrain_matter_container_component(x, y, z)
                if m.water_vapor > 0:
                    found = (x, y, z)
                    break
            if found: break
        if found: break

    assert found is not None, "No vapor produced — evaporation did not fire"
    tid = vg.get_terrain(*found)
    assert tid == -1, f"Vapor voxel at {found} has tid={tid}, expected -1"
```

> **Note for C4:** the heat-injection API call (`set_heat` or similar) and the exact
> evaporation tuning may need a small extension to bindings/helpers. Confirm the necessary
> API surface during implementation; if a binding is missing, expose it as a test-only
> helper alongside `set_terrain_id_raw`.

---

### Task R — Extract water-cluster mutators into `WaterPhysicsMutators.cpp`

**Goal.** Move the implementations of the 13 water-simulation mutators out of the
1900-line `PhysicsMutators.hpp` and into a dedicated implementation file at
`src/physics/mutators/WaterPhysicsMutators.cpp`. The header keeps the canonical
declarations so existing includers (`PhysicsEngine.cpp`, `EcosystemEngine.cpp`,
`World.cpp`, ...) don't change. Pure refactor — no signature changes, no logic
changes; bodies move byte-identical (only the `inline` keyword stripped from each
signature line).

**Why.** The header has become unreadable at this size. Splitting the water cluster
into its own .cpp gives code review a clear seam, makes it easier to spot where
state mutations happen, and unblocks future per-domain splits (vapor, terrain, life
cycle, etc.) without ballooning the header further.

**Functions in scope (originally 13; 2 deleted under C4 — net 11):**

| Function | Status |
|----------|--------|
| `_handleWaterSpreadEvent` | done (commit `0bf0b69`) |
| `setGravityFlowWaterTargetDefaults` | done (commit `0bf0b69`) |
| `setGravityFlowEmptySourceDefaults` | done (commit `0bf0b69`) |
| `_handleTerrainPhaseConversionEvent` | done (commit `0bf0b69`) |
| `createVaporTerrainEntity` | pending |
| `createWaterTerrainFromFall` | pending |
| `createWaterTerrainFromGravityFlow` | pending |
| `addOrCreateVaporAbove` | pending |
| `createWaterTerrainBelowVapor` | pending |
| `_handleWaterGravityFlowEvent` | pending |
| `~~createAndRegisterVaporEntity~~` | ~~pending~~ — **deleted by C4 / C4.3** (no callers after the gas-movement path went coord-based) |
| `~~_handleCreateVaporEntityEvent~~` | ~~pending~~ — **deleted by C4.3** (no `CreateVaporEntityEvent` dispatchers remain) |
| `_handleVaporMergeSidewaysEvent` | pending |

**Build setup (one-time, already done in commit `0bf0b69`):**

- `src/physics/CMakeLists.txt` globs extended to `src/physics/mutators/*.{cpp,hpp}`
  with `CONFIGURE_DEPENDS` so CMake re-detects new files on incremental builds.
  Without `CONFIGURE_DEPENDS`, a new file in a new subdirectory silently fails to
  compile and the linker happily produces a `.so` with an unresolved symbol that
  segfaults at runtime — this trap was hit twice while bulk-moving 9+ functions and
  is the reason for the safe-slice protocol below.

**Safe-slice protocol (mandatory).** Two prior bulk-move attempts (13 functions and
9 functions) both broke runtime in subtle ways and had to be reverted. The protocol
that worked:

1. Pick **one or three** functions with no still-inline water-cluster callees (or
   whose callees are already moved). Avoid mixing functions that have inter-call
   dependencies in the same batch unless all calls resolve through the
   header-declared symbol set.
2. Append the body verbatim to `WaterPhysicsMutators.cpp` — only `inline ` stripped
   from the signature line. Indentation, comments, parameter formatting all stay
   byte-identical.
3. Replace the body in `PhysicsMutators.hpp` with a forward declaration that
   matches the moved signature byte-for-byte (preserve the original two-line
   `inline void` / `function-name(...)` style — keep it for diff cleanliness, or
   normalize to a single line, but don't change argument types or order).
4. Run `make build-install-test`. If GREEN, commit. If not, **revert** and
   investigate the specific function before retrying — don't pile on more changes.

**Files:**
- `src/physics/PhysicsMutators.hpp` (header — bodies replaced with forward decls)
- `src/physics/mutators/WaterPhysicsMutators.cpp` (new — bodies appended one batch
  at a time)
- `src/physics/CMakeLists.txt` (already extended, no further changes needed)

**Acceptance:** `make build-install-test` GREEN at every commit boundary; no other
tests fail; live game starts and water flows in the mountain-side scenario.

**Status (2026-05-02):** **4 of 11 done** in commit `0bf0b69` (two of the original 13
functions were deleted entirely under C4/C4.3). Remaining 7 to migrate one-at-a-time
per the protocol above. Coordinate with C3 / C4 / D — once those tasks ship, the
affected functions can be moved in the same commit since the body already needs
editing.

---

#### Task R Phase 2 — Unified state-mutation audit

**Architectural goals.** The audit exists to verify and drive four
architectural rules that govern every state-mutating code path. Every
candidate flagged in the table below is a violation of one or more of
these. Reviewers should treat the rules as load-bearing — not aspirational
— because the live-game `Entity has both WaterMatter and WaterVapor`
crash we have been chasing is a direct consequence of breaches.

1. **Actor / executor write rule — writes execute on exactly one of two
   paths.** Either the synchronous physics main loop (`processPhysics` and
   the helpers it directly invokes), or as the body of a dispatcher event
   handler (`PhysicsEngine::on*Event` → `_handle*` mutator). Any other
   write origin — Python tick callbacks, world pre/post-update hooks,
   raw repository writes from places that are neither in the loop nor in
   a handler — is out of pattern and must be migrated. This is the
   actor-executor model: writes happen only in known, serialised
   contexts so the system as a whole has a small set of points where
   state changes can be reasoned about.

2. **Mutators are the only write surface.** Regardless of which path
   triggers the write (event-driven or inside the physics loop), the
   *actual* mutation lives inside a function declared in
   `PhysicsMutators.hpp` (with the body either inline in the header or
   in `physics/mutators/*.cpp`). No `terrainGridRepository->set*` /
   `setVelocity` / `setMovingComponent` / `moveTerrain` call should
   appear outside a mutator function. This rule is what makes the entire
   write surface searchable from one file: a reviewer can enumerate every
   guard once, not 22 times across handlers and helpers.

3. **Mutators validate their own preconditions.** Each mutator is
   responsible for verifying — within its locked region — the cell-state
   invariants its operation depends on, in light of its specific
   context. *Do not assume the caller already checked.* Read fresh,
   validate, then act. The cost of a defensive read inside a single
   `withSharedLock` region is negligible; the cost of an unguarded
   propagation downstream is a runtime crash that we have spent days
   diagnosing. The audit annotates every row's `Guard` column with what
   the mutator currently checks; an empty `Guard` cell on a matter-write
   row is a defect.

4. **Inconsistency is never accepted.** When a mutator detects that its
   operation would produce an invalid state, it must do one of these
   three things — never silently let the violation through:
   - **Event-driven path** — re-enqueue the same event with an
     incremented `retryCount` so transient races (concurrent vapor /
     liquid traffic, cascading writes) get a chance to resolve before
     the next attempt. Abort with a `warn`-level log after a small
     bounded retry limit (`WATER_VAPOR_CONFLICT_RETRY_LIMIT = 3`).
     `createWaterTerrainFromFall` (C3.6) and `createWaterTerrainBelowVapor`
     (C3.9) are the canonical implementations of this pattern.
   - **Synchronous physics-loop path** — refuse the operation, clear the
     source's velocity / scheduling state so the source stops
     re-attempting on the next tick, and log the reason.
     `_attemptVelocityDrivenMove` (row 19b) is the canonical
     implementation of this pattern.
   - **When the source itself is already inconsistent** — refuse to
     *propagate* the violation downstream. The source will be caught by
     the per-tick water-invariant check on a later iteration; the
     mutator's job at this point is to not make things worse by spreading
     the bad state to a fresh cell.

**Audit goal.** Enumerate every code path that mutates terrain-cell state (matter,
velocity, moving component, terrain id, structural integrity, physics stats),
annotated with the event handler that triggers it (if any), the centralised
mutator it lives in (if any), the invariant guard it has (if any), and how it
is invoked. Empty cells (`null` / `n/a`) are intentional — they mark the
columns that don't apply to that row, which is exactly the signal we use to
prioritise migration work *against the four architectural goals above*.

This is the single source of truth replacing two earlier sub-audits. Use it
to pick refactoring targets: rows where the mutator column is empty are
candidates for centralisation; rows where the event-handler column is empty
are paths that bypass the event-sink architecture entirely; rows where the
guard column is empty (or `n/a` because the row doesn't write matter) need
extra scrutiny if matter-write rows nearby depend on them being safe.

**`EcosystemEngine.cpp` baseline (no work needed for terrain).**
Verified clean: zero direct terrain-state mutations. All terrain interaction is
read-only (`get*`, `checkIf*`, `hasMovingComponent`) plus `TerrainGridLock`
constructions. The two `registry.emplace` / `registry.create` sites it does have
target non-terrain domains (`PlantResources` at line 515; `raspberryFruit` plus
`ItemTypeComponent` / `FoodItem` at lines 1614–1620). Out of scope for this
refactor.

**Unified write-site audit.**

`State` column abbreviations: `matter` = `MatterContainer`; `vel` = `Velocity`;
`mc` = `MovingComponent`; `tid` = terrain id; `pos`/`type`/`SIC`/`stats` are
the corresponding repository fields; `delete` = `voxelGrid.deleteTerrain`.

Rows are ordered by priority: dangerous unguarded paths first, then
newly-centralised reference patterns (the model to copy when migrating
remaining rows), then already-centralised event-driven rows, then Phase 2
inline-handler candidates, then non-event helpers, then setup-only
writers.

| # | Write site | State | Event handler | Mutator (`PhysicsMutators` / `WaterPhysicsMutators`) | Guard | Invocation | Notes / migration recommendation |
|---|---|---|---|---|---|---|---|
| 1 | Python binding `TerrainStorage.set_terrain_water_matter` / `.set_terrain_vapor_matter` / `.set_terrain_matter` (`aetherion.cpp:322,324,326`) | matter (per-field, **raw VDB grid write, NO LOCK**) | _none_ | _none — **bypasses `TerrainGridRepository` entirely**_ | ❌ **none** | currently only used by `tests/integration/test_terrain_storage.py` and `test_world_terrain.py` | ☢️ **Foot-gun binding**. Two consecutive Python calls (one to each setter) would produce both-fields-positive with no detection. **Migration target**: remove the binding from the Python surface — tests can use the locked, container-level setter instead. |
| 2 | `TerrainGridRepository::moveTerrain` (`TerrainGridRepository.cpp:67–232`) — copies source's `MatterContainer` to destination via `setTerrainMatterContainer` (line 162) | matter, tid, type, SIC, stats, vel, pos | _none — repository method_ | _none — **lives in storage layer**_ | ❌ **no guard inside `moveTerrain` itself** — but the only physics-loop caller (row 4) now validates source + destination phase compatibility before invoking. Other callers (`applyTerrainMovement` helper) still call directly. | physics worker thread, **non-event** | ⚠️ The repository method itself stays unguarded. Defence-in-depth via row 4's guard covers the velocity-driven path; remaining direct callers (`applyTerrainMovement` from event-handler-driven move paths) inherit the guards from their event handlers. |
| 3 | `TerrainGridRepository::setTerrainFromEntt` (`TerrainGridRepository.cpp:847+`) — copies entity's `MatterContainer` via `setTerrainMatterContainer` (line 871) | matter, type, SIC, stats | _none — repository method_ | _none — **lives in storage layer**_ | ❌ **no guard** — propagates entity's matter as-is | one-time entity → VDB migration (`createEntityFromPython`, `_seed_liquid_water_at`) | ⚠️ Edge case. Only fires during entity migration. Migrate to a guarded mutator if `createEntityFromPython` path is kept; else delete after Task G. |
| 4 | `_attemptVelocityDrivenMove` (`PhysicsMutators.hpp:1742+`) | matter (via `moveTerrain`), tid, type, SIC, stats, vel, mc, pos | _none — physics-loop mutator invoked from row 21_ | `_attemptVelocityDrivenMove` | ✅ refuses if source has both fields; refuses on phase mismatch; refuses if destination already populated; clears source velocity on every refusal so it does not retry | physics worker thread, **non-event** | ✅ Centralised. Defends against the unguarded `moveTerrain` matter copy from row 2 by validating before the move. **Reference pattern** for migrating other physics-loop write sites. |
| 5 | `SpringWaterSystem.update` (`spring_water.py`) — dispatches `WaterCreationEvent` per pace-tick | matter (via row 6) | `onWaterCreationEvent` | `_handleWaterCreationEvent` (`PhysicsMutators.hpp`) | ✅ guards delegated to the mutator (row 6) | event-driven from the main thread (Python `update()` call) | ✅ Centralised. No reads, no inline writes, no entity creation. The mutator owns all state validation. **Reference pattern** for migrating other Python-side writers. |
| 6 | `_handleWaterCreationEvent` (`PhysicsMutators.hpp`) — three-branch handler for the `WaterCreationEvent` | matter, tid, type, SIC, stats, vel | `onWaterCreationEvent` | `_handleWaterCreationEvent` | ✅ NONE → fresh terrain scaffolding; liquid → additive merge; vapor → retry-then-abort up to `WATER_VAPOR_CONFLICT_RETRY_LIMIT`; non-water terrain → refuse + warn | event-driven worker | ✅ Centralised. Used by `SpringWaterSystem` today; reusable for scripted weather and future rain. **Reference pattern** for centralising other coord-only sources. |
| 7 | `_handleWaterSpreadEvent` (`WaterPhysicsMutators.cpp:11+`) | matter | `onWaterSpreadEvent` | `_handleWaterSpreadEvent` | ✅ aborts on `currentTarget.WaterVapor > 0` | event-driven worker | Centralised — no work |
| 8 | `_handleWaterGravityFlowEvent` (`PhysicsMutators.hpp:1676+`) | matter, tid, type, SIC, stats, vel | `onWaterGravityFlowEvent` | `_handleWaterGravityFlowEvent` | ✅ aborts on vapor target | event-driven worker | Centralised — no work |
| 9 | `_handleTerrainPhaseConversionEvent` (`WaterPhysicsMutators.cpp:103+`) | matter, type, SIC | `onTerrainPhaseConversionEvent` | `_handleTerrainPhaseConversionEvent` | ✅ field-mutex check both directions | event-driven worker | Centralised — no work |
| 10 | `_handleVaporMergeSidewaysEvent` (`PhysicsMutators.hpp:1775+`) | matter | `onVaporMergeSidewaysEvent` | `_handleVaporMergeSidewaysEvent` | ✅ branches (water-only / vapor-only / empty); throws on pre-existing both-fields | event-driven worker | Centralised — no work |
| 11 | `createWaterTerrainFromFall` (`PhysicsMutators.hpp:1162+`) | matter, tid, type, SIC, pos, stats, vel | `onWaterFallEntityEvent` (delegates) | `createWaterTerrainFromFall` | ✅ retry-then-abort on vapor-only destination | event-driven worker | Centralised helper — no work |
| 12 | `createWaterTerrainFromGravityFlow` (`PhysicsMutators.hpp:1320+`) | matter, tid, type, SIC, pos, stats, vel | `onWaterGravityFlowEvent` (via `_handleWaterGravityFlowEvent`) | `createWaterTerrainFromGravityFlow` | ✅ caller-side guard | event-driven worker | Centralised helper — no work |
| 13 | `createWaterTerrainBelowVapor` (`PhysicsMutators.hpp:1494+`) | matter, tid, type, SIC, pos, stats, vel | `onCondenseWaterEntityEvent` (Path 2 only) | `createWaterTerrainBelowVapor` | ✅ retry-then-abort on vapor-at-destination | event-driven worker | Centralised helper — no work |
| 14 | `addOrCreateVaporAbove` (`PhysicsMutators.hpp:1404+`) | matter (via merge or via row 15) | `onVaporCreationEvent` (delegate) and `onEvaporateWaterEntityEvent` (inline call from row 19) | `addOrCreateVaporAbove` | ✅ guards on `matterContainerAbove.WaterMatter == 0` (vapor branch); fresh `MatterContainer{}` (create branch via row 15) | event-driven worker | Centralised helper — no work |
| 15 | `createVaporTerrainEntity` (`PhysicsMutators.hpp:109+`) | matter, tid, type, SIC, pos, stats | called from `addOrCreateVaporAbove` (row 14) | `createVaporTerrainEntity` | ✅ writes fresh `MatterContainer{}` (only `WaterVapor` set) | event-driven worker | Centralised helper — no work |
| 16 | `_recoverStaleTerrainCellIfTransitory` (`PhysicsMutators.hpp:1652+`) | delete | `onWaterFallEntityEvent` (delegates) | `_recoverStaleTerrainCellIfTransitory` | n/a (only deletes) | event-driven worker | Centralised helper — no work |
| 17 | `onVaporMergeUpEvent` body (`PhysicsEngine.cpp:185–247`) | matter | `onVaporMergeUpEvent` | _none — **inline in handler**_ | ✅ guards on `targetMatter.WaterMatter == 0` | event-driven worker | **Phase 2 candidate**: extract to `_handleVaporMergeUpEvent` to mirror sideways |
| 18 | `onAddVaporToTileAboveEvent` body (`PhysicsEngine.cpp:256–305`) | matter | `onAddVaporToTileAboveEvent` | _none — **inline**_ | ✅ guards on `matterContainerAbove.WaterMatter == 0` | event-driven worker | **Phase 2 candidate**: extract to `_handleAddVaporToTileAboveEvent` |
| 19 | `onCondenseWaterEntityEvent` body, Path 1 (`PhysicsEngine.cpp:2160–2179`) | matter (× 2 cells: matterBelow + vaporMatter) | `onCondenseWaterEntityEvent` | _none — **inline** (Path 2 already delegates to row 13)_ | ✅ guards on `matterBelow.WaterVapor == 0` | event-driven worker | **Phase 2 candidate**: extract Path 1 into `_handleCondenseWaterEntityEvent`; Path 2 already in row 13 |
| 20 | `onEvaporateWaterEntityEvent` body (`PhysicsEngine.cpp:2031–2095`) | matter, stats; vapor goes to z+1 via row 14 | `onEvaporateWaterEntityEvent` | _none — **inline**_ | ✅ source decrement on liquid cell only; vapor placed in a *different* cell via guarded row 14 | event-driven worker | **Phase 2 candidate (highest priority)**: also uses raw `lockTerrainGrid()` / `unlockTerrainGrid()` (lines 2042/2094) instead of `TerrainGridLock` RAII — any throw between the two leaks the lock |
| 21 | `onMoveGasEntityEvent` body (`PhysicsEngine.cpp:1537–1655`) | vel (mc via repository for direction check) | `onMoveGasEntityEvent` | _none — **inline**_ | n/a (no matter write) | event-driven worker | **Phase 2 candidate**: extract to `_handleMoveGasEntityEvent`; lowest priority because it's the largest body and the path most under active development |
| 22 | `onMoveSolidLiquidTerrainEvent` body (`PhysicsEngine.cpp:1735+`) | vel, mc, pos, etc. | `onMoveSolidLiquidTerrainEvent` | _none — **inline**_ | n/a (no matter write) | event-driven worker | **Phase 2 candidate**: extract to `_handleMoveSolidLiquidTerrainEvent` |
| 23 | `onDeleteOrConvertTerrainEvent` body | varies (delete/convert) | `onDeleteOrConvertTerrainEvent` | _partial — wraps `_handle*` calls but does extra inline work_ | n/a | event-driven worker | **Phase 2 candidate (low priority)**: tighten so the body becomes a pure delegator |
| 24 | `_nudgeSettledWaterAfterDrain` (`PhysicsMutators.hpp:1682+`) | vel | _none — free helper invoked from row 4's mutator_ | `_nudgeSettledWaterAfterDrain` | n/a (only seeds gravity velocity if `WaterMatter > 0` and current velocity is zero) | physics inner loop, **non-event** | ✅ Centralised — moved out of `PhysicsEngine` member function into `PhysicsMutators.hpp` |
| 25 | `processVelocityForVoxel` body (`PhysicsEngine.cpp:1481, 1504`) | vel | _none — `PhysicsEngine` velocity-loop body_ | move-trigger sub-block delegates to `_attemptVelocityDrivenMove` (row 4); two remaining `setVelocity` writes are inline | n/a (no matter write; the velocity persists / clears are local results of friction/gravity computation, not read-modify-write) | physics worker thread, **non-event** | ⚠️ Two `setVelocity` writes still inline — Phase 2 candidate to centralise as `_persistComputedVelocity` / `_clearVelocityOnCollision` if symmetry matters. The bigger move sub-block is now centralised. |
| 26 | `make_mountain_side_pre_update`'s `_seed_liquid_water_at` (`factory.py:84`) — `set_terrain_matter_container` | matter, type, SIC | _none_ | _none — **Python-side inline write**_ | n/a (one-time, hard-coded `(water_matter=1000, water_vapor=0)`) | one-time on first pre-update tick | Non-event-driven; pattern is unguarded. **Migration target**: route through `WaterCreationEvent` (row 6) once pre-update hooks become a long-term feature. |
| 27 | Reference primitives `place_water` / `place_vapor` / `place_stone` / `place_empty` (`primitives.py`) — `set_terrain_matter_container` | matter, type, SIC | _none_ | _none — **Python-side inline write**_ | n/a (each writes terrain-type-appropriate fixed values) | scenario / test setup only | Non-event-driven; safe for setup. **Not a migration target** unless these primitives ever get called at runtime. |

**Acceptance for Phase 2 / Phase 3 work driven from this table:**
- Every `event-driven` row has a non-empty `Mutator` cell (centralised, not inline).
- Every `non-event` row that mutates matter either gets a centralised mutator or an explicit reason-to-stay note.
- Every `❌ no guard` row that mutates matter either gains an invariant guard or has its caller documented as a known-safe context.
- The dangerous Python bindings in row 1 are removed from the surface.

#### Task R Phase 2 — Second-pass: invariant-risk audit

The first-pass table tells us *who* writes. The second pass asks a sharper
question against rule 4 ("never accept inconsistency"): **can this write
produce a `MatterContainer` with both `WaterMatter > 0` and
`WaterVapor > 0`, even transiently?** We have spent days chasing the
`Entity has both WaterMatter and WaterVapor` crash. Each previous fix
(C3.6 / C3.7 / C3.9 / `_attemptVelocityDrivenMove` / `_handleWaterCreationEvent`)
guarded a *specific* code path against a *specific* failure mode and
each one was a real bug — yet the crash keeps coming back. The simplest
explanation: our guards are *sub-condition* checks, not the *full*
invariant check. Every one of them asks "is one specific input clean?"
None of them asks the simpler, sufficient question: "does the
`MatterContainer` I am about to write satisfy
`!(WaterMatter > 0 && WaterVapor > 0)`?"

**The corollary of rule 4 we have been missing — Rule 4b: outgoing-write
invariant check.** Every mutator that writes a `MatterContainer` must,
just before the call to `setTerrainMatterContainer`, verify that the
container it is about to write does not violate the invariant. If the
check trips, the mutator must do one of:
- Apply rule 4 retry-then-abort if event-driven (re-enqueue with
  retry counter, warn-log on exhaust).
- Apply rule 4 refuse-and-clear-velocity if physics-loop.
- Throw a structured exception with full context (coord, before, after,
  mutator name) if the violation looks like an active bug to track
  down — surfacing the offender directly through the worker-thread
  catcher rather than letting some downstream reader trip the
  per-tick invariant check.

A previous attempt to add this check inside `setTerrainMatterContainer`
itself segfaulted because it did *unsynchronised* reads. The lesson is
that the check belongs **inside the mutator** (under the mutator's
existing lock), not inside the storage layer. Each mutator already
holds the unique lock when it writes; adding the check there is free.

**Risk-by-row for the matter-writing rows.** Pass / fail per Rule 4b:

| # | Write site | Sub-condition guard today | Rule 4b (outgoing-write invariant) | Reasoning |
|---|---|---|---|---|
| 2 | `moveTerrain` | ❌ none | ❌ **fails** | Copies source's matter as-is. If source has both fields > 0 (violated), destination receives the violation. The velocity-driven caller (row 4) now guards source-side, but `applyTerrainMovement` (called from event-handler-driven moves) does *not* — it inherits its caller's guard, which never explicitly checks "source has neither field doubled". |
| 3 | `setTerrainFromEntt` | ❌ none | ❌ **fails** | Same as row 2 for the entity-migration path. |
| 4 | `_attemptVelocityDrivenMove` | ✅ refuses if source has both, refuses on phase mismatch | ✅ **passes by construction** | The "refuses if source has both fields" branch is exactly the Rule 4b check applied at the source side; the phase-mismatch branch covers the destination side. |
| 6 | `_handleWaterCreationEvent` | ✅ vapor-only retry, non-water refuse, additive merge into water/grass | ⚠️ **partial** | The merge branch does `destMatter.WaterMatter += amount` and writes back `destMatter`. If `destMatter` *was already* in violation (`WaterVapor > 0` AND `WaterMatter > 0` at read time, an upstream-introduced violation), the existing guard `destinationIsVaporOnly` (`WaterVapor > 0 && WaterMatter <= 0`) is **false**, so we fall through to the additive-merge path and write a still-violated container. Need to add a Rule 4b check on the outgoing `destMatter`. |
| 7 | `_handleWaterSpreadEvent` | ✅ aborts on `currentTarget.WaterVapor > 0` | ⚠️ **partial** | Same shape as row 6. Aborts when target is vapor-only, but if target already has both fields > 0, the guard is false (vapor IS > 0, but not vapor-only), and the additive merge writes a still-violated container. |
| 8 | `_handleWaterGravityFlowEvent` | ✅ aborts on `currentTarget.WaterVapor > 0` | ⚠️ **partial** | Same shape as row 7. |
| 9 | `_handleTerrainPhaseConversionEvent` | ✅ field-mutex check both directions | ✅ **passes** | The check covers both fields explicitly (`event.newMatter.WaterMatter > 0 && currentMatter.WaterVapor > 0`, and reverse). |
| 10 | `_handleVaporMergeSidewaysEvent` | ✅ branches; throws on pre-existing both-fields | ✅ **passes** | The else-branch explicitly throws when target has both fields > 0; merge result is constructed to land in exactly one phase. |
| 11 | `createWaterTerrainFromFall` | ✅ retry-then-abort on vapor-only destination | ⚠️ **partial** | Same as row 6: vapor-only branch covered by retry, but pre-existing both-fields-positive at destination is not detected. |
| 13 | `createWaterTerrainBelowVapor` | ✅ retry-then-abort on vapor-at-destination | ⚠️ **partial** | Same as row 6. |
| 14 | `addOrCreateVaporAbove` (vapor-add branch) | ✅ guards on `matterContainerAbove.WaterMatter == 0` | ✅ **passes** | Adds vapor only when WaterMatter is exactly 0, so output has WaterMatter=0 and WaterVapor>0 — invariant holds. |
| 15 | `createVaporTerrainEntity` | ✅ writes fresh `MatterContainer{}` | ✅ **passes** | Fresh container, only WaterVapor set. |
| 17 | `onVaporMergeUpEvent` body (inline) | ✅ guards on `targetMatter.WaterMatter == 0` | ✅ **passes** | Same shape as row 14. |
| 18 | `onAddVaporToTileAboveEvent` body (inline) | ✅ guards on `matterContainerAbove.WaterMatter == 0` | ✅ **passes** | Same shape as row 14. |
| 19 | `onCondenseWaterEntityEvent` Path 1 (inline) | ✅ guards on `matterBelow.WaterVapor == 0` | ⚠️ **partial** | Same shape as row 6. Aborts when matterBelow has vapor, but pre-existing both-fields not detected; the additive merge into `matterBelow.WaterMatter` writes back whatever vapor was there. |
| 20 | `onEvaporateWaterEntityEvent` body (inline) | ✅ source decrement on liquid, vapor goes to z+1 | ⚠️ **partial** | Source-cell write only modifies WaterMatter — but if source had pre-existing vapor > 0, the write preserves the violation. |

**Suspects ranked.** The ⚠️ "partial" rows are the candidates for the
live-game crash. Each one *preserves* the violation if a cell is
already in violation when the mutator runs. Combined with row 2's
unguarded `moveTerrain` matter copy, a single upstream violation
introduced anywhere can propagate through every centralised handler
without ever being detected — until `processTileWater` happens to
iterate the cell and throws. That matches the observed symptom (crash
fires *after* a long burst of activity, not at a specific
identifiable write site).

**Why row 2 is the most likely first-introducer.** `moveTerrain` is
the *only* matter-write surface that has **no guard at all** and runs
on a hot worker-thread loop. The defence in `_attemptVelocityDrivenMove`
covers the velocity-driven physics-loop caller; the gas-/solid-move
event-handler callers (rows 21, 22) inherit no equivalent guard. If
*any* of those handlers ever calls `applyTerrainMovement` on a cell
that briefly held both fields, `moveTerrain` propagates the violation
to a fresh cell where `processTileWater` will eventually find it.

**Migration targets driven by Rule 4b.**

1. **Add a small inline helper** `_assertMatterContainerInvariant(mc, x, y, z, mutatorName)` in `PhysicsMutators.hpp`: throws `std::runtime_error` with full context if `mc.WaterMatter > 0 && mc.WaterVapor > 0`. Cheap, no allocations on the success path.
2. **Insert the helper before every `setTerrainMatterContainer` call inside a mutator.** This is what the four ⚠️-partial rows need (6, 7, 8, 11, 13, 19, 20). The helper fires in the offender's stack frame, exposing exactly which mutator is propagating bad state.
3. **For row 2 (`moveTerrain`):** add the same check just before line 162 (the `setTerrainMatterContainer` that copies source's matter to destination) — but treated specially since `moveTerrain` is a storage method, not a mutator. Either (a) lift the check into `_attemptVelocityDrivenMove` and the equivalent guards on the event-handler-driven move callers, or (b) add the check inside `moveTerrain` itself with an explicit comment that this is the one storage-layer guard we accept (with the recursion-safe lock pattern from Rule 3 — read source matter via a method that respects the thread-local recursion guard).
4. **For row 3 (`setTerrainFromEntt`):** same as row 2.

The result is that Rule 4b becomes mechanically enforced: *no* mutator
can write a violated `MatterContainer`, regardless of what state the
cell was in when the mutator started. The first writer that *would*
introduce the violation crashes with full context, in its own stack
frame, where we can read who its caller was. That's strictly better
than the current behaviour (some unrelated reader trips the per-tick
check after the trail is cold).

#### Task R Phase 3 — Instrumentation findings (2026-05-03)

Phase 2 said "throw on violation"; Phase 3 said "**log** on violation"
because two prior attempts at throwing — both inside
`TerrainGridRepository::setTerrainMatterContainer`, the storage method —
produced segfaults instead of clean stack traces. The lesson: any
diagnostic at the storage layer is poisoned, regardless of whether it
reads storage.

**Failed approaches (do not repeat):**

1. _Storage-read snapshot inside the setter._ Read `before` / `after` via raw `storage_.getTerrainMatter(x,y,z)` etc. Caused unsynchronised OpenVDB tree traversal racing with concurrent writers → segfault during VDB tree-node dereference.

2. _Parameter-only check with `std::source_location` + throw._ Even with zero storage reads, the throw + unwinding through openvdb-adjacent code segfaulted instead of producing the diagnostic. Suspected cause: exception unwinding through code paths that aren't exception-safe, or a worker-pool thread catcher mishandling the throw type. The probe site itself is the problem; the check semantics are not.

**Successful approach.** A single inline helper at the top of `PhysicsMutators.hpp`:

```cpp
inline void
_logIfViolatingMatterWrite(const char *mutatorName, int x, int y, int z,
                           const MatterContainer &outgoing) {
  if (outgoing.WaterMatter > 0 && outgoing.WaterVapor > 0) {
    auto logger = spdlog::get("console");
    if (logger) {
      logger->error("[INVARIANT-WRITE][{}] coord=({}, {}, {}) "
                    "WaterMatter={} WaterVapor={} TerrainMatter={} "
                    "BioMassMatter={}",
                    mutatorName, x, y, z, outgoing.WaterMatter,
                    outgoing.WaterVapor, outgoing.TerrainMatter,
                    outgoing.BioMassMatter);
    }
  }
}
```

Called immediately before every `setTerrainMatterContainer` inside each mutator (parameter-only check, no storage reads, no throw, nullptr-guarded logger). 14 call sites instrumented across `PhysicsMutators.hpp` + `WaterPhysicsMutators.cpp`. Sites that can't violate by construction (`createVaporTerrainEntity` hardcodes `WaterMatter=0`; `setEmptyWaterComponentsStorage`, `convertTerrainTileToEmpty`, `setGravityFlowEmptySourceDefaults` write all-zero matter) skipped to avoid log noise.

Companion change: silenced three high-volume logs that were crowding the trace (`handleMovement -> applying Gravity` in `PhysicsEngine.cpp`, `deleteEntityOrConvertInEmpty: processing entity ...` in `PhysicsMutators.hpp`, both `No active terrain entity to delete at ...` branches in `TerrainGridRepository.cpp`). All three commented out, not deleted, with a one-line note for re-enabling.

**First catch (2026-05-03 02:22:50).**

```
[INVARIANT-WRITE][createWaterTerrainFromFall:dest] coord=(49, 49, 2) WaterMatter=1 WaterVapor=1 TerrainMatter=0 BioMassMatter=0
onWaterFallEntityEvent -> Created ON_GRID_STORAGE water at (49, 49, 2).
```

Next-line "Created ON_GRID_STORAGE water" confirmed the destination was treated as `destinationIsEmpty == true` (i.e., `terrainId == NONE`). Yet the outgoing container had `WaterVapor=1`. The only way that can happen: the cell's WaterVapor grid entry held a stale `1` from a prior write that cleared `terrainId` without zeroing matter — an **orphan vapor** in a `NONE` cell.

**Why the function let it through.** `createWaterTerrainFromFall` read `destMatter` unconditionally at the top of the function, regardless of whether the destination was NONE. The vapor-only safeguard at line 1243-1247 only fires for `!destinationIsEmpty`, so a `NONE` cell with orphan WaterVapor sails past every guard, and the additive `destMatter.WaterMatter += fallingAmount` produces `{WaterMatter=1, WaterVapor=1}` — exactly the violation.

**Band-aid (R.1 — landed).** Adopt `_handleWaterCreationEvent`'s conditional-read pattern:

```cpp
MatterContainer destMatter{};
if (!destinationIsEmpty) {
  destMatter =
      voxelGrid.terrainGridRepository->getTerrainMatterContainer(x, y, z);
}
```

For NONE cells, `destMatter` stays zero-init'd. The additive write produces `{WaterMatter=fallingAmount, WaterVapor=0, ...}`, and the subsequent `setTerrainMatterContainer` also overwrites the orphan WaterVapor in storage as a side effect — band-aid both prevents the violation *and* cleans the residue in one step.

**Verified parallels.** `createWaterTerrainBelowVapor` and `_handleWaterCreationEvent` already use the conditional-read pattern, so no further changes were needed in those mutators. The audit in Phase 2 of which mutators *preserve* vs *create* the violation is consistent with this finding: `createWaterTerrainFromFall` was the only "creator" of a fresh violation; the other 7 ⚠️-partial mutators only preserve pre-existing ones.

**Open follow-ups.**

- **R.2:** find the upstream path that produces orphan WaterVapor in `NONE` cells. Same diagnostic pattern as Phase 3, just on the deletion side: `_logIfOrphanMatterAfterDelete(callerName, x, y, z)` at every place that sets terrainId to NONE or soft-deactivates a cell with non-zero matter.
- **R.3:** once R.2 closes the source, upgrade the 7 ⚠️ rows from log to throw (Rule 4b proper). Until then, the `_logIfViolatingMatterWrite` calls keep the diagnostic surface visible without the segfault risk of a throw.

**Memory recorded** (user-level): `feedback_invariant_check_segfault_risk.md` — never propose a probe inside `TerrainGridRepository::setTerrainMatterContainer` again, even with parameter-only checks. Two attempts both segfaulted. Instrumentation lives inside mutators only.

---

### Task D — Refactor `WaterFallEntityEvent` to carry `VoxelCoord` only

**Goal:** `WaterFallEntityEvent` no longer carries an `entt::entity` field. It carries
`{int x, int y, int z}` as primary keys (already carries `sourcePos` and `position` —
confirmed in `EcosystemEvents.hpp:35-45`). `onWaterFallEntityEvent` in
`PhysicsEngine.cpp` resolves all terrain state from the repository using these
coordinates, never from the entity handle.

This supersedes crash-fix plan Task 5c ("switch WaterFallEntityEvent to carry VoxelCoord
only") and bundles **merge-fix tasks E2.1, E2.2, E2.3, E1.3, E1.4** from
`2026-05-02-water-fall-event-merge-fix.md`. After today's E1.1 work, the handler is
already calling an additive-safe `createWaterTerrainFromFall`, so the only remaining
shape changes are:

1. Remove the `entity` field from the event struct (merge-fix E2.2 weakens it to
   advisory; Task D drops it).
2. Replace the `if (terrainToCreateWaterId == NONE)` short-circuit with the three-way
   merge / re-emit / re-emit-with-defer switch (merge-fix E1.3 + E1.4).
3. Replace the `registry.create()` recovery block with an `ON_GRID_STORAGE` coord
   write (merge-fix E2.1) — closes the version-overflow loop documented in
   `analysis/2026-05-02-floating-water-collision-loss.md` E2.

**Steps:**
1. Update `WaterFallEntityEvent` in `src/ecosystem/EcosystemEvents.hpp` — if it already
   has `position` field, confirm it carries x/y/z. Remove `entity` field.
2. Update all dispatch sites of `WaterFallEntityEvent` to pass coordinates, not an entity.
3. Rewrite `onWaterFallEntityEvent` in `src/PhysicsEngine.cpp` (lines 2076–2205):
   - Replace `registry.valid(entity)` checks with `repo->checkIfTerrainExists(x, y, z)`
   - Replace entity-based state reads with `repo->getTerrainEntityType(x, y, z)` etc.
   - Replace recovery `registry.create()` with VDB-only water creation (same as C1 pattern)
   - Remove the `entity handle` recovery path entirely once entities no longer exist for water

**Files:**
- `src/ecosystem/EcosystemEvents.hpp`
- `src/PhysicsEngine.cpp` (lines 2076–2205)
- Any other dispatch site for `WaterFallEntityEvent` (search project)

**Test:**

> The file `tests/reference/test_water_fall_event.py` **already exists** (created by
> merge-fix E1.1 — first test:
> `test_fall_into_existing_water_merges_additively`). Task D adds new test functions
> to the same file rather than creating a new one. The merge-fix epic owns the
> full test catalog for this file (E1.1, E1.2, E1.3, E1.4, E2.1, E2.2, E2.3, F1) —
> Task D is GREEN once those tests are GREEN. The single Task-D-specific addition
> is the assertion that the handler runs without a valid `event.entity` field.

```python
# tests/reference/test_water_fall_event.py — Task D addition

from helpers import (
    build_minimal_test_manager, fall_event_position,
    place_empty, place_water, water_matter,
)


def test_water_fall_lands_at_correct_voxel_with_no_entity_field():
    """Task D: dispatch a fall event whose source/destination are coord-only and
    confirm the handler resolves terrain state without consulting any entity
    handle. RED today because the handler still reads `event.entity` for the
    Position lookup; GREEN after Task D drops the field and the handler is
    fully coord-driven."""
    manager = build_minimal_test_manager(3, 3, 3)
    voxel_grid = manager.current.world.get_voxel_grid()
    place_empty(voxel_grid, 1, 1, 0)             # destination NONE
    place_water(voxel_grid, 1, 1, 1, water_matter=3)

    # `dispatch_water_fall_event` defaults `entity` to TerrainIdTypeEnum::NONE (-2),
    # i.e. an explicitly invalid handle. Post-D this is the only mode callers use.
    manager.current.world.dispatch_water_fall_event(
        source_pos=fall_event_position(1, 1, 1),
        dest_pos=fall_event_position(1, 1, 0),
        falling_amount=3,
    )
    manager.update()

    assert water_matter(voxel_grid, 1, 1, 0) == 3
    assert water_matter(voxel_grid, 1, 1, 1) == 0
    assert voxel_grid.get_terrain(1, 1, 0) == -1   # ON_GRID_STORAGE
    assert voxel_grid.terrain_grid_repository.sum_total_water() == 3
```

`place_empty` is a new helper; add it to `tests/reference/helpers.py` alongside
`place_water` and `place_stone`. It zeroes the matter container at `(x, y, z)` and
calls `set_terrain_id_raw(x, y, z, NONE)`.

**Acceptance:** all 170+ tests pass; no `"entity not valid"` lines in `water_debug.jsonl` during
a 200-step mountain-side run; the four per-task tests (`test_c1_water_fall.py`,
`test_c2_water_gravity_flow.py`, `test_c3_water_spread.py`, `test_c4_vapor_creation.py`)
still GREEN.

---

### Task E — Remove `registry.destroy()` from terrain deletion paths

**Goal:** `deleteEntityOrConvertInEmpty` and `deleteTerrain` no longer enqueue
`KillEntityEvent` for plain terrain (water, vapor, grass). Only terrain with a real ECS
entity (i.e., `terrainId > 0` — `TileEffectsList` case) fires `KillEntityEvent`. Plain
terrain deletion is a pure VDB + tracking-map operation.

> **Overlap with merge-fix E2.1.** Merge-fix E2.1 removes the `registry.create()` call
> in `onWaterFallEntityEvent`'s recovery branch (`PhysicsEngine.cpp:~2200`) — that's
> creation, not destruction, but it lives in the same handler that Task D rewrites.
> Land merge-fix E2.1 inside Task D's scope (it's a strict prerequisite to remove the
> entity field), then Task E follows independently for the deletion path.

**Steps:**
1. In `PhysicsMutators.hpp:deleteEntityOrConvertInEmpty` (line 393), the deletion path:
   - Old: `voxelGrid.deleteTerrain(dispatcher, x, y, z, false)` — calls
     `KillEntityEvent{terrainId}` internally
   - New: `voxelGrid.deleteTerrain(dispatcher, x, y, z, false)` still works, but
     `KillEntityEvent` is only enqueued when `terrainId > 0` (entity exists)
2. In `TerrainGridRepository::deleteTerrain` (lines 912–961): wrap the `KillEntityEvent`
   dispatch in `if (terrainId > 0)` guard — non-entity terrain does not need the event.
3. In `WorldProcessEntityDeletion`: confirm it handles `entt::null` or missing entity gracefully
   (should already, but verify).

**Files:**
- `src/terrain/TerrainGridRepository.cpp` (lines 912–961)
- `src/physics/PhysicsMutators.hpp` (lines 354–402)

**Test:**

> Prerequisite: expose `World.get_alive_entity_count()` (see Required Nanobind Additions §3).
> Write `tests/reference/test_no_registry_growth.py` BEFORE implementing Task E.
> Run `make test` → RED (entity count grows with water cycles). Implement. GREEN.

```python
# tests/reference/test_no_registry_growth.py
"""Task E: registry must not grow as water voxels are created and deleted."""

def test_alive_entity_count_stable_during_water_cycling():
    """After C1–C4+D+E, registry.alive() must not increase as water flows and is deleted."""
    manager = _build_manager()
    world = manager.current.world

    # Record baseline after world init (beasts, spring entity, etc.)
    baseline = world.get_alive_entity_count()

    for _ in range(200):
        manager.update()

    after = world.get_alive_entity_count()

    # Count must not grow — water creation/deletion must be ECS-free
    # Allow a small constant slack for any legitimate non-terrain entities that spawn
    SLACK = 10
    assert after <= baseline + SLACK, (
        f"Registry grew by {after - baseline} during water simulation "
        f"(expected ≤ {SLACK}). Water entities are still being created in ECS."
    )
```

---

### Tasks F1–F3 — Cleanup (parallel batch, after C1–C4 + E complete)

#### F1 — Remove ECS hooks for terrain-only usage

**Goal:** `onConstructVelocity`, `onDestroyVelocity`, `onConstructMoving`, `onDestroyMoving`
in `TerrainGridRepository.cpp` (lines 275–400) no longer trigger `markActive` / `clearActive`
for terrain voxels. Since terrain has no entities, the hooks fire only for non-terrain
entities that happen to share the same registry (beasts, etc.) — and these can be unregistered
entirely if no non-terrain entity uses `Velocity` or `MovingComponent` via the terrain repo
hooks.

**Approach:** If beasts/plants still use these hooks: gate the hook body with a check
`if (!isTerrainEntity(e)) return;` or disconnect the hooks entirely if no non-terrain entity
needs them.

**Files:** `src/terrain/TerrainGridRepository.cpp` (lines 275–400)

**Test:**

> No new test file needed here — F1 is structural cleanup. Write the test assertions as
> inline checks inside the **existing** `test_no_registry_growth.py` (Task E test) by adding a
> beast entity to the world and verifying its velocity hook still fires correctly:

```python
def test_beast_velocity_hook_still_fires_after_f1():
    """Non-terrain entities must still get activated when Velocity is emplaced."""
    # This is covered by existing entity-movement tests — run make test and confirm
    # beast/beast movement tests still pass. No additional test file needed.
    # Acceptance: `make build-install-test` with all existing tests GREEN.
    pass
```

**Acceptance:** `make build-install-test` all tests pass; no `markActive` log entries for
water voxels; beast movement tests (in `tests/entities/`) are GREEN.

---

#### F2 — Remove `byCoord_`/`byEntity_` tracking maps for terrain

**Goal:** After C1–C4, no terrain voxel is added to `byCoord_`/`byEntity_`. Confirm the
maps are empty during a full simulation run (assert or log count == 0 for terrain entries).
Then remove `addToTrackingMaps` / `removeFromTrackingMaps` calls in the terrain creation /
deletion paths.

If non-terrain entities still use `byCoord_`/`byEntity_` (e.g., for `getPositionOfEntt`), the
maps stay but are scoped to non-terrain. If no non-terrain entity uses them, remove the maps
entirely.

**Files:** `src/terrain/TerrainGridRepository.hpp` (member declarations),
`src/terrain/TerrainGridRepository.cpp` (all `byCoord_`/`byEntity_` usage sites)

**Test:**

> Expose `TerrainGridRepository.get_tracking_map_size() → int` as a temporary debug binding
> (remove after F2 is confirmed and deleted). Write a quick assertion in any existing reference
> test that after 200 steps the size is 0 (or equal to non-terrain entity count only).

```python
# Add to test_no_registry_growth.py as a new test function:
def test_terrain_tracking_maps_empty_after_decoupling():
    """byCoord_/byEntity_ must hold no terrain entries during simulation."""
    manager = _build_manager()
    world = manager.current.world
    repo = world.get_voxel_grid().terrainGridRepository

    for _ in range(200):
        manager.update()

    # If no non-terrain entity uses the tracking maps, size should be 0
    # If beasts use getPositionOfEntt, allow that count instead
    size = repo.get_tracking_map_size()
    assert size == 0, (
        f"Tracking maps still have {size} entries — terrain entities not fully removed")
```

> **Performance note:** `byCoord_` and `byEntity_` are `std::unordered_map` — removing them
> eliminates two hash-map lookups per water voxel creation and deletion. For high-traffic
> corridors (700+ recycles) this is measurable. Confirm with a before/after `make build-install-test`
> timing comparison if desired.

---

#### F3 — Remove ECS `Position` from `moveTerrain`

**Goal:** `moveTerrain` at lines 174–177 currently does `registry_.emplace<Position>(newEntity, newPos)`. Since entities no longer exist for terrain, this call is a no-op at best, a crash at worst. Remove it. Also remove `registry_.remove<Position>` / `registry_.remove<Velocity>` lines 82–84 (entity no longer exists).

**Files:** `src/terrain/TerrainGridRepository.cpp` (lines 66–189)

**Test:**

> F3 is covered by Task A/B tests and the existing mountain-side regression tests. No new
> file needed. Confirm with `make build-install-test` that `test_velocity_vdb.py` and
> `test_moving_component_vdb.py` (Tasks A + B) remain GREEN — those tests exercise `moveTerrain`
> indirectly via the gravity-flow path.

---

### Task G — `createEntityFromPython` terrain fast-path (VDB write, no entity)

**Goal:** When Python calls `world.create_entity(grass)` for a terrain object, skip
`registry.create()` entirely. Detect `grid_type == TERRAIN` before any ECS allocation and
write all static attributes directly to `TerrainGridRepository` setters.

**Steps:**
1. Add `void World::createTerrainFromPython(nb::object pyEntity)` (private method in
   `World.hpp` + `World.cpp`) — reads position, entity_type, matter_container,
   physics_stats, structural_integrity from the Python object; calls `terrainGridRepository`
   setters; no `registry.create()`.
2. At the top of `World::createEntityFromPython` (line 159), add a fast-path guard:
   ```cpp
   if (nb::hasattr(pyEntity, "grid_type")) {
     if (nb::cast<GridType>(pyEntity.attr("grid_type")) == GridType::TERRAIN) {
       createTerrainFromPython(pyEntity);
       return entt::null;
     }
   }
   ```
3. Audit Python callers: confirm no caller stores/uses the returned entity handle for terrain.

**Files:** `src/World.hpp`, `src/World.cpp` (lines 159–324)

**Test:**

> Prerequisite: expose `World.get_alive_entity_count()` (see Required Nanobind Additions §3).
> Write `tests/reference/test_create_terrain_from_python.py` BEFORE implementing.
> Run `make test` → RED (entity count increments on terrain create). Implement. GREEN.

```python
# tests/reference/test_create_terrain_from_python.py
"""Task G: world.create_entity for terrain must not call registry.create()."""
import aetherion

def test_create_terrain_entity_does_not_grow_registry():
    """Creating a grass terrain voxel from Python must not increment registry.alive()."""
    world = aetherion.World(10, 10, 10)
    world.initialize_voxel_grid()

    before = world.get_alive_entity_count()

    # Create a terrain entity the same way world factories do
    grass = aetherion.EntityInterface()  # or the Python-side entity object used in factories
    grass.grid_type = aetherion.GridType.TERRAIN
    grass.position = aetherion.Position(x=5, y=5, z=0)
    # (set other required fields as the factory does)
    world.create_entity(grass)

    after = world.get_alive_entity_count()
    assert after == before, (
        f"registry.alive() went from {before} to {after} — "
        f"terrain fast-path in createEntityFromPython not taken")

def test_create_terrain_entity_visible_in_vdb():
    """The terrain voxel created from Python must be readable via get_terrain(x,y,z)."""
    world = aetherion.World(10, 10, 10)
    world.initialize_voxel_grid()

    grass = aetherion.EntityInterface()
    grass.grid_type = aetherion.GridType.TERRAIN
    grass.position = aetherion.Position(x=5, y=5, z=0)
    world.create_entity(grass)

    terrain_id = world.get_voxel_grid().get_terrain(5, 5, 0)
    assert terrain_id == -1, (
        f"Expected ON_GRID_STORAGE (-1) for Python-created terrain, got {terrain_id}")
```

**Acceptance:** both tests GREEN; existing 200-step corridor test still passes (world factory
creates the same terrain layout as before, just via VDB instead of ECS).

---

## Parallelism Map

```
  [A: Velocity → VDB]  ───┐
                           │
  [B: Moving → Map]  ──────┤
                           │
                           ├──→ [C-Pre.1: iterateVelocityVoxels] ──→ [C-Pre.2: dual-loop processPhysics]
                           │                                                   │
                           │                                                   ▼
                           ├──────────────────────────────────────────→ [C1] ──┐
                           │                                            [C2] ──┤
                           │                                            [C3] ──┤  ──→ [D] ──→ [E] ──→ [F1, F2, F3]
                           │                                            [C4] ──┘
                           │
  [G: Python terrain fast-path] ──────────────── after B ──────────────────────────────────────
```

**Session 1:** A + B in parallel (different files, no conflicts) ✓ DONE
**Session 2:** C-Pre.1 (TerrainStorage/Repository iteration method)
**Session 3:** C-Pre.2 (PhysicsEngine dual loop — depends on C-Pre.1)
**Session 4:** C1 + C2 + C3 + C4 can be assigned to separate worktrees or done sequentially
in one session (each is ≤80 lines of change in the same file; sequential is safer to avoid
merge conflicts in `PhysicsMutators.hpp`)
**Session 5:** D + G in parallel (different files)
**Session 6:** E alone (needs D stable first)
**Session 7:** F1 + F2 + F3 in parallel (cleanup, different parts of same file)

---

## Test Acceptance Criteria

| Test | Baseline (now) | After A+B | After C1–C4 | After D | After E+F | Final |
|------|---------------|-----------|------------|---------|-----------|-------|
| `test_..._200_steps` | PASS | PASS | PASS | PASS | PASS | PASS |
| `test_..._2000_steps` | PASS | PASS | PASS | PASS | PASS | PASS |
| `test_watch_corridor_no_corrupted_ids_at_1000_steps` | FAIL | FAIL | **PASS** | PASS | PASS | PASS |
| `test_specific_crash_voxels_stay_valid_to_1000_steps` | FAIL | FAIL | **PASS** | PASS | PASS | PASS |
| `test_no_engine_exception_in_safe_budget` | FAIL | FAIL | **PASS** | PASS | PASS | PASS |
| `test_full_grid_no_corrupted_ids_after_safe_budget` | FAIL | FAIL | **PASS** | PASS | PASS | PASS |
| `test_no_water_sim_errors_after_safe_budget` | FAIL | FAIL | **PASS** | PASS | PASS | PASS |
| `test_water_falls_to_on_grid_storage::*` (was test_c1_water_fall) | n/a | n/a | **PASS** | PASS | PASS | PASS |
| `test_water_gravity_flow::*` (was test_c2_water_gravity_flow) | n/a | n/a | **PASS** | PASS | PASS | PASS |
| `test_water_spread::*` (C3 successor) | n/a | n/a | **PASS** | PASS | PASS | PASS |
| `test_vapor_creation::*` (C4 successor) | n/a | n/a | **PASS** | PASS | PASS | PASS |
| `test_velocity_readable_from_vdb_after_gravity_step` | FAIL | **PASS** | PASS | PASS | PASS | PASS |
| `test_moving_component_cleared_after_water_settles` | FAIL | **PASS** | PASS | PASS | PASS | PASS |
| `test_water_fall_event::test_fall_into_existing_water_merges_additively` (merge-fix E1.1+E1.3) | FAIL | FAIL | FAIL | **PASS** | PASS | PASS |
| `test_water_fall_event::test_water_fall_lands_at_correct_voxel_with_no_entity_field` (D) | FAIL | FAIL | FAIL | **PASS** | PASS | PASS |
| `test_alive_entity_count_stable_during_water_cycling` | FAIL | FAIL | FAIL | FAIL | **PASS** | PASS |
| `test_create_terrain_entity_does_not_grow_registry` | FAIL | FAIL | FAIL | FAIL | FAIL | **PASS** |
| All 165 currently passing tests | PASS | PASS | PASS | PASS | PASS | PASS |

The 5 crash-fix tests flip at Phase C. The new TDD tests flip one phase at a time as designed.

---

## Known Blockers

| Blocker | Blocks | Status |
|---------|--------|--------|
| `storage_.copyVoxel(from, to)` may not exist yet | Task C2 (gravity flow entity-reuse path) | Needs confirmation — check `TerrainStorage.hpp`; if missing, inline the field copies |
| Non-terrain entities using `Velocity` via terrain hooks | Task F1 | Audit required before removing hooks; confirm beasts/plants do not rely on `onConstructVelocity` firing in `TerrainGridRepository` |
| Python callers storing terrain entity handles | Task G | Audit `world_factories.py` and any other `create_entity` callers that capture return value |
| `WaterFallEntityEvent` struct fields | Task D | ✅ Confirmed (`EcosystemEvents.hpp:35-45`): `entt::entity entity; Position sourcePos; Position position; int fallingAmount`. Task D drops the `entity` field after merge-fix E2.2 makes the handler ignore it. |
| `World::getRegistry()` access for `get_alive_entity_count` | Task E, G | Add getter or expose via `VoxelGrid` — needed before writing Task E/G tests |
| `get_tracking_map_size()` binding | Task F2 | Temporary debug binding — add, use for F2 verification, then remove |

---

## Relationship to `2026-04-30-water-sim-crash-fix-plan.md`

| Crash fix task | Disposition in this plan |
|---------------|--------------------------|
| 5a (ghost entity after overflow guard) | **Superseded** by C1–C4 — no entities created, no ghosts |
| 5b (recovery creates zero-water entities) | **Superseded** by D — recovery path rewritten |
| 5c (coord-based WaterFallEntityEvent) | **Directly addressed** in Task D |
| 6a (createTerrainFromPython) | **Covered** by Task G |
| 6b (route terrain in createEntityFromPython) | **Covered** by Task G |
| 4 (remove recovery masking) | **Superseded** by D — handler rewritten without recovery |

The locking fixes from crash-fix tasks 2 and 3 (lock scope, thread-local ownership,
`withUniqueLock` race) are still valid and should remain. This epic does not touch them.

---

## Relationship to `2026-05-02-water-fall-event-merge-fix.md`

The merge-fix epic is a **focused subset** of this decoupling plan, scoped to the
event-based water-fall path. The two plans share files (`PhysicsMutators.hpp`,
`PhysicsEngine.cpp`, `EcosystemEvents.hpp`, `tests/reference/test_water_fall_event.py`,
`tests/reference/helpers.py`) and overlap on tasks. Coordination map:

| Decoupling task | Merge-fix task | Relationship |
|----------------|---------------|--------------|
| C1 (done) | E1.1 (done 2026-05-02) | Merge-fix retro-hardened C1's `createWaterTerrainFromFall` to be additive + vapor-safe. C1 alone removed `registry.create()`; E1.1 closed the latent overwrite. |
| C2 (done 2026-05-02) | — | Adopted scaffolding-gated contract from E1.1; matter additive merge owned by `_handleWaterGravityFlowEvent`. Vapor safeguard not needed (caller already guards). |
| C3 (pending — retargeted to `createWaterTerrainBelowVapor`) | E1.2 (pending) | C3 is now the third water-creation site (water from vapor condensation). E1.2 (additive matter contract on the same function) lands as part of C3. |
| C4 (pending — expanded to 3 vapor sites) | — | Pure vapor-creation cluster: `createVaporTerrainEntity`, `createEmptyActiveTerrain`, `createAndRegisterVaporEntity`. The previously-listed E1.2 overlap moved to C3. |
| R (in-progress, 4/13) | — | Refactor: extract water-cluster mutator implementations to `src/physics/mutators/WaterPhysicsMutators.cpp`. Orthogonal to merge-fix; no behavior change. |
| D (pending) | E1.3, E1.4, E2.1, E2.2, E2.3 (pending) | Task D *bundles* all five merge-fix handler tasks. E2.2 weakens `event.entity` to advisory; D drops it. E1.3+E1.4 replace the short-circuit with merge + re-emit branches. E2.1 removes the recovery-branch `registry.create()` (8th site). E2.3 funnels every silent bail-out through the re-emission primitive. |
| E (pending) | — | Orthogonal — destruction path, not creation. No merge-fix overlap. |
| F1, F2, F3 (pending) | — | Orthogonal cleanup. |
| G (pending) | — | Orthogonal — Python factory path. |
| — | F1 (merge-fix end-to-end test) | Owned by the merge-fix epic; verifies the `(89..97, 50, 1)` repro after the full chain ships. |

**Sequencing recommendation:**

1. Finish merge-fix E1.3 first (handler short-circuit → three-way switch). It flips the
   already-RED `test_fall_into_existing_water_merges_additively` to GREEN and validates
   today's E1.1 work end-to-end.
2. Then merge-fix E1.4 + E2.3 (re-emission primitive). This permanently closes C1
   follow-up #2 (`processTileWater` throw mitigation can be removed).
3. Then merge-fix E2.1 + E2.2 (recovery branch + entity-field-as-advisory). At this
   point Task D becomes a small struct-rename change (drop `entity` field).
4. Then C2, C3, C4 (+ merge-fix E1.2 with C4) in parallel. They no longer block each
   other and can be assigned to separate worktrees.
5. Then E, F1, F2, F3, G — independent cleanup.
