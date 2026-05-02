"""Event-based water-fall path: world-state contracts.

These tests drive `WaterFallEntityEvent` directly via
`World.dispatch_water_fall_event` (no velocity, no movement) and assert on
observable world state after a single `manager.update()` tick.

See `.claude/docs/epics-plans/2026-05-02-water-fall-event-merge-fix.md` for
the full task list. Tests are written RED-first against the desired post-fix
behavior — see each test docstring for the bug it pins down.
"""

from __future__ import annotations

from helpers import (
    build_minimal_test_manager,
    fall_event_position,
    place_water,
    water_matter,
)


def test_fall_into_existing_water_merges_additively():
    """E1.1 + E1.3: a fall event into a populated water cell must merge
    additively. Source matter is decremented by exactly `falling_amount`,
    destination matter grows by exactly `falling_amount`, and total water
    is conserved.

    RED today because:
      - `onWaterFallEntityEvent` short-circuits on populated destinations
        (silent drop), so the merge never runs.
      - Even if the short-circuit were relaxed, `createWaterTerrainFromFall`
        overwrites destination water (`= falling_amount`) instead of adding.
    """
    # ARRANGE — 3x3x3 world, two water voxels stacked
    manager = build_minimal_test_manager(3, 3, 3)
    voxel_grid = manager.current.world.get_voxel_grid()
    place_water(voxel_grid, 1, 1, 0, water_matter=5)  # destination already holds water
    place_water(voxel_grid, 1, 1, 1, water_matter=3)  # source

    initial_total = voxel_grid.terrain_grid_repository.sum_total_water()
    assert initial_total == 8, "Setup invariant"

    # ACT — direct dispatch of the fall event, no velocity, single tick
    manager.current.world.dispatch_water_fall_event(
        source_pos=fall_event_position(1, 1, 1),
        dest_pos=fall_event_position(1, 1, 0),
        falling_amount=3,
    )
    manager.update()

    # ASSERT — world state matches the additive contract
    assert water_matter(voxel_grid, 1, 1, 0) == 8  # 5 + 3
    assert water_matter(voxel_grid, 1, 1, 1) == 0  # source drained exactly
    assert voxel_grid.terrain_grid_repository.sum_total_water() == 8  # conservation
