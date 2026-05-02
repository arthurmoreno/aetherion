"""Gravity-flow into an empty cell lands water as ON_GRID_STORAGE.

Drives `_handleWaterGravityFlowEvent` through the
`water_gravity_flow_world_factory` reference scenario: water at
``GRAVITY_FLOW_SOURCE_POS`` flows into the empty cell at
``GRAVITY_FLOW_TARGET_POS``. Tests assert (1) the destination cell ends as
``ON_GRID_STORAGE`` (terrain id ``-1``), no positive entity handle, and
(2) total water is conserved across the flow.
"""

from __future__ import annotations

from time import sleep

from helpers import build_minimal_test_manager, place_empty, place_water, water_matter

from aetherion import Position
from aetherion.reference.world.scenarios import (
    GRAVITY_FLOW_AMOUNT,
    GRAVITY_FLOW_INITIAL_WATER_MATTER,
    GRAVITY_FLOW_SOURCE_POS,
    GRAVITY_FLOW_TARGET_POS,
)

# Setup is built inline rather than via a scenario factory because:
# 1. The package-level `water_gravity_flow_world_factory` was rewired
#    to a spring-driven layout (no water at t=0, water injected at z=2
#    every 5 ticks) that doesn't match this test's contract.
# 2. The original `_3x3x3` factory still does the right `place_water`
#    call but enables `process_ecosystem_async = True`, whose worker
#    pool races on small grids and intermittently hangs `manager.update()`.
# `build_minimal_test_manager` defaults to a synchronous world via
# `empty_square_world_factory`, mirroring `test_water_fall_event.py`'s
# already-stable setup.


def _coord_to_position(coord: tuple[int, int, int]) -> Position:
    position = Position()
    position.x, position.y, position.z = coord
    return position


def _setup_scenario():
    manager = build_minimal_test_manager(3, 3, 3)
    voxel_grid = manager.current.world.get_voxel_grid()
    # `empty_square_world_factory` (the factory behind
    # `build_minimal_test_manager`) fills layer z=0 with stone. The
    # gravity-flow scenario expects the target cell to be NONE so the
    # handler routes through `createWaterTerrainFromGravityFlow`'s
    # empty-target branch — clear it explicitly here.
    place_empty(voxel_grid, *GRAVITY_FLOW_TARGET_POS)
    place_water(
        voxel_grid,
        *GRAVITY_FLOW_SOURCE_POS,
        water_matter=GRAVITY_FLOW_INITIAL_WATER_MATTER,
    )
    return manager, voxel_grid


def test_gravity_flow_into_empty_cell_lands_as_on_grid_storage():
    """Source at SOURCE_POS, empty cell at TARGET_POS. After dispatching a
    `WaterGravityFlowEvent` and ticking once, the destination must be
    populated water with terrain id ``-1`` (ON_GRID_STORAGE), not a positive
    EnTT entity handle.

    RED today because `createWaterTerrainFromGravityFlow` calls
    `registry.create()` on the NONE-target branch and writes that entity
    handle into the terrain id grid.
    """
    manager, voxel_grid = _setup_scenario()

    assert water_matter(voxel_grid, *GRAVITY_FLOW_SOURCE_POS) == GRAVITY_FLOW_INITIAL_WATER_MATTER
    assert voxel_grid.get_terrain(*GRAVITY_FLOW_TARGET_POS) == -2  # NONE

    manager.current.world.dispatch_water_gravity_flow_event(
        source_pos=_coord_to_position(GRAVITY_FLOW_SOURCE_POS),
        target_pos=_coord_to_position(GRAVITY_FLOW_TARGET_POS),
        amount=GRAVITY_FLOW_AMOUNT,
    )
    manager.update()
    sleep(0.001)

    target_terrain_id = voxel_grid.get_terrain(*GRAVITY_FLOW_TARGET_POS)
    assert target_terrain_id == -1, (
        f"Destination at {GRAVITY_FLOW_TARGET_POS} has tid={target_terrain_id}, expected -1 (ON_GRID_STORAGE)"
    )


def test_gravity_flow_conserves_total_water():
    """Total water across the world must be exactly preserved as water flows
    from source to destination — no overwrite, no double-add."""
    manager, voxel_grid = _setup_scenario()
    initial_total = voxel_grid.terrain_grid_repository.sum_total_water()
    assert initial_total == GRAVITY_FLOW_INITIAL_WATER_MATTER, "Setup invariant"

    manager.current.world.dispatch_water_gravity_flow_event(
        source_pos=_coord_to_position(GRAVITY_FLOW_SOURCE_POS),
        target_pos=_coord_to_position(GRAVITY_FLOW_TARGET_POS),
        amount=GRAVITY_FLOW_AMOUNT,
    )
    manager.update()
    sleep(0.005)

    final_total = voxel_grid.terrain_grid_repository.sum_total_water()
    assert final_total == initial_total, (
        f"Total water drifted from {initial_total} to {final_total} during gravity flow"
    )
    assert water_matter(voxel_grid, *GRAVITY_FLOW_TARGET_POS) == GRAVITY_FLOW_AMOUNT
    assert water_matter(voxel_grid, *GRAVITY_FLOW_SOURCE_POS) == GRAVITY_FLOW_INITIAL_WATER_MATTER - GRAVITY_FLOW_AMOUNT
