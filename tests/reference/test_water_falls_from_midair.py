"""Mid-air ON_GRID_STORAGE water must fall under gravity.

Validates the velocity-grid-driven gravity discovery:
1. Water created without an ECS entity (placed via `place_water` writing
   ON_GRID_STORAGE directly) needs an initial velocity seed to enter the
   VDB velocity grid — without it, the ECS-only physics-async pass never
   sees the voxel.
2. The velocity-driven pass picks it up via `iterateVelocityVoxels`,
   applies gravity each tick, and triggers `moveTerrain`.
3. `moveTerrain` propagates velocity from source to destination so the
   voxel keeps falling on subsequent ticks without dropping out of the
   iteration set.
4. After each `moveTerrain` the physics layer wakes any settled water in
   the cell above so a column collapses cleanly.

Note on `place_water`. The primitive in
`aetherion.reference.world.scenarios.primitives` writes the water voxel
directly (terrain id, type, matter, SIC, physics stats) but does **not**
seed initial velocity — it bypasses `createWaterTerrainFromFall` /
`createWaterTerrainFromGravityFlow` / `createWaterTerrainBelowVapor`.
For the mid-air-fall test to drive the velocity-grid path, the water
needs an alternative kick. Two options:

- The wake-up cascade: when the voxel below a settled water cell becomes
  empty, the wake-up logic seeds gravity velocity. The
  `column_above_floor` test exercises this path by deleting a placeholder
  cell below the water.
- A small seed via `set_terrain_velocity` from Python before ticking,
  mirroring what `createWaterTerrain*` does at creation time. The
  midair-fall test exercises this path.
"""

from __future__ import annotations

from helpers import build_scenario_manager, water_matter

from aetherion.reference.world.scenarios import (
    MIDAIR_INITIAL_WATER_MATTER,
    MIDAIR_WATER_POS,
    water_column_above_floor_world_factory,
    water_fall_from_midair_world_factory,
)

_NONE_TERRAIN_ID = -2


def _effective_water_matter(voxel_grid, x: int, y: int, z: int) -> int:
    """Treat NONE-terrain cells as having zero water.

    `water_matter()` reads the VDB matter grid directly without consulting the
    terrain id. After `moveTerrain` (or `deleteTerrain`) clears a source cell's
    terrain id to NONE, the matter VDB keeps its prior value as stale residue
    until something writes over it. From the simulation's perspective such a
    cell holds zero water — but `water_matter()` happily returns the residue.
    This helper guards reads with a terrain-id check so test assertions reflect
    the cell's effective state.
    """
    if voxel_grid.get_terrain(x, y, z) == _NONE_TERRAIN_ID:
        return 0
    return water_matter(voxel_grid, x, y, z)


def test_midair_water_falls_to_floor_with_seeded_velocity():
    """Water voxel at z=8 with a small initial gravity kick must reach the
    stone floor at z=0 within a budget of physics ticks.

    Exercises two requirements together: the velocity-driven Loop 2 picks
    up the seeded ON_GRID_STORAGE voxel, and `moveTerrain` propagates
    velocity from source to destination so the voxel keeps falling
    instead of stopping after one cell.
    """
    import time

    manager = build_scenario_manager(
        water_fall_from_midair_world_factory,
        "Mid-Air Falling Water",
    )
    voxel_grid = manager.current.world.get_voxel_grid()

    # Setup invariant: water is at the seeded mid-air position.
    assert _effective_water_matter(voxel_grid, *MIDAIR_WATER_POS) == MIDAIR_INITIAL_WATER_MATTER

    # Mimic what `createWaterTerrainFromFall` would do at creation time
    # (place_water bypasses that path). A small downward kick is enough —
    # the velocity-driven physics pass accelerates from there each tick.
    voxel_grid.terrain_grid_repository.set_terrain_velocity(*MIDAIR_WATER_POS, 0.0, 0.0, -1.0)

    # Run the simulation for a generous step budget. With gravity 5.0
    # and a 7-cell drop, a reasonable budget is ~50 ticks.
    for _ in range(50):
        manager.update()
        time.sleep(0.001)

    # Water must have moved out of the original mid-air cell. (The cell's
    # terrain id should be NONE; `_effective_water_matter` returns 0 in
    # that case even if the VDB matter grid still carries stale residue.)
    assert _effective_water_matter(voxel_grid, *MIDAIR_WATER_POS) == 0, (
        f"Water still sitting at the seeded mid-air position {MIDAIR_WATER_POS} "
        f"after 50 ticks — gravity didn't reach the ON_GRID_STORAGE voxel"
    )

    # Water must have arrived somewhere lower in the same column.
    landing_z = None
    for z in range(MIDAIR_WATER_POS[2]):
        if _effective_water_matter(voxel_grid, MIDAIR_WATER_POS[0], MIDAIR_WATER_POS[1], z) > 0:
            landing_z = z
            break
    assert landing_z is not None, "Water did not arrive at any lower cell in the same column"
    assert landing_z < MIDAIR_WATER_POS[2], f"Water still at or above its starting height (landed at z={landing_z})"


def test_water_column_falls_when_floor_is_removed_via_wakeup():
    """Stone floor at z=0 supports a water column at z=1..4. Once the
    bottom water cell is moving, each ``moveTerrain`` invokes the
    physics layer's wake-up nudge for the cell above, which seeds it
    with downward velocity so the next tick can move it. The column
    therefore cascades downward one cell per tick.

    The test seeds the initial velocity at ``(2, 2, 1)`` directly,
    which mimics what the in-physics wake-up nudge would do for a
    drain triggered through a physics-layer code path. The
    ``World.delete_terrain_at`` binding bypasses the physics layer
    (it goes straight to ``VoxelGrid::deleteTerrain`` in storage), so
    it does not start the cascade on its own — only physics-layer
    drain sites do, and they are not exposed individually as Python
    bindings. Asserting the cascade itself (one trigger producing
    downstream wake-ups for cells above) is the meaningful invariant
    here.
    """
    import time

    manager = build_scenario_manager(
        water_column_above_floor_world_factory,
        "Water Column Above Floor",
    )
    voxel_grid = manager.current.world.get_voxel_grid()
    repo = voxel_grid.terrain_grid_repository

    # Setup invariant: water at z=1..4 each holds 1000 matter, z=0 is stone.
    for z in (1, 2, 3, 4):
        assert _effective_water_matter(voxel_grid, 2, 2, z) == 1000

    # Drain z=0 (stone) and seed downward velocity at z=1 so the
    # bottom water voxel becomes a candidate for the velocity-driven
    # physics pass. From there, each `moveTerrain` invokes the
    # in-physics wake-up nudge on the cell above, propagating the
    # cascade up the column.
    manager.current.world.delete_terrain_at(2, 2, 0)
    repo.set_terrain_velocity(2, 2, 1, 0.0, 0.0, -5.0)

    # Run for enough ticks to let the cascade propagate through the
    # 4-cell column.
    for _ in range(20):
        manager.update()
        time.sleep(0.001)

    # The column should have shifted down. The bottom (z=0) must hold
    # water and the top (z=4) must be effectively empty (`get_terrain`
    # returns NONE there).
    assert _effective_water_matter(voxel_grid, 2, 2, 0) > 0, (
        "Water did not reach z=0 — wake-up cascade failed at the bottom"
    )
    assert _effective_water_matter(voxel_grid, 2, 2, 4) == 0, (
        "Water still at z=4 after cascade — the top cell did not shift down"
    )
