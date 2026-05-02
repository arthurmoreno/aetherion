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


def test_midair_water_falls_to_floor_with_seeded_velocity():
    """Water voxel at z=8 with a small initial gravity kick must reach the
    stone floor at z=0 within a budget of physics ticks.

    Exercises two requirements together: the velocity-driven Loop 2 picks
    up the seeded ON_GRID_STORAGE voxel, and `moveTerrain` propagates
    velocity from source to destination so the voxel keeps falling
    instead of stopping after one cell.
    """
    manager = build_scenario_manager(
        water_fall_from_midair_world_factory,
        "Mid-Air Falling Water",
    )
    voxel_grid = manager.current.world.get_voxel_grid()

    # Setup invariant: water is at the seeded mid-air position.
    assert water_matter(voxel_grid, *MIDAIR_WATER_POS) == MIDAIR_INITIAL_WATER_MATTER

    # Mimic what `createWaterTerrainFromFall` would do at creation time
    # (place_water bypasses that path). A small downward kick is enough —
    # processPhysics Loop 2 will accelerate from there each tick.
    voxel_grid.terrain_grid_repository.set_terrain_velocity(
        *MIDAIR_WATER_POS, 0.0, 0.0, -1.0
    )

    # Run the simulation for a generous step budget. With gravity 5.0
    # and a 7-cell drop, a reasonable budget is ~50 ticks.
    for _ in range(50):
        manager.update()

    # Water must have moved out of the original mid-air cell.
    assert water_matter(voxel_grid, *MIDAIR_WATER_POS) == 0, (
        f"Water still sitting at the seeded mid-air position {MIDAIR_WATER_POS} "
        f"after 50 ticks — gravity didn't reach the ON_GRID_STORAGE voxel"
    )

    # Water must have arrived somewhere lower in the same column.
    landing_z = None
    for z in range(MIDAIR_WATER_POS[2]):
        if water_matter(voxel_grid, MIDAIR_WATER_POS[0], MIDAIR_WATER_POS[1], z) > 0:
            landing_z = z
            break
    assert landing_z is not None, (
        "Water did not arrive at any lower cell in the same column"
    )
    assert landing_z < MIDAIR_WATER_POS[2], (
        f"Water still at or above its starting height (landed at z={landing_z})"
    )


def test_water_column_falls_when_floor_is_removed_via_wakeup():
    """Stone floor at z=0 supports a water column at z=1..4. Deleting the
    stone via `World.delete_terrain_at` must wake up the water at z=1,
    which then cascades the column down by one cell as each `moveTerrain`
    triggers another wake-up on the cell above.

    Exercises the wake-up cascade: settled water has zero velocity and is
    therefore invisible to the ECS-only physics-async pass; the wake-up
    nudge invoked after a drain is what gets the column moving.
    """
    manager = build_scenario_manager(
        water_column_above_floor_world_factory,
        "Water Column Above Floor",
    )
    voxel_grid = manager.current.world.get_voxel_grid()

    # Setup invariant: water at z=1..4 each holds 1000 matter, z=0 is stone.
    for z in (1, 2, 3, 4):
        assert water_matter(voxel_grid, 2, 2, z) == 1000

    # Delete the stone floor under the column. This calls
    # `VoxelGrid::deleteTerrain`; the physics layer's wake-up nudge
    # then seeds velocity on (2, 2, 1) so the cascade starts.
    manager.current.world.delete_terrain_at(2, 2, 0)

    # Run for enough ticks to let the cascade propagate through the
    # 4-cell column.
    for _ in range(20):
        manager.update()

    # The column should have shifted down by exactly one cell. Bottom
    # cells (z=0..3) hold water, top cell (z=4) is now empty.
    assert water_matter(voxel_grid, 2, 2, 0) > 0, (
        "Water did not reach z=0 — wake-up cascade failed at the bottom"
    )
    assert water_matter(voxel_grid, 2, 2, 4) == 0, (
        "Water still at z=4 after cascade — the top cell did not shift down"
    )
