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

from helpers import build_scenario_manager, water_matter

from aetherion import Position
from aetherion.reference.world.scenarios import (
    GRAVITY_FLOW_AMOUNT,
    GRAVITY_FLOW_INITIAL_WATER_MATTER,
    GRAVITY_FLOW_SOURCE_POS,
    GRAVITY_FLOW_TARGET_POS,
    water_gravity_flow_world_factory,
)


def _coord_to_position(coord: tuple[int, int, int]) -> Position:
    position = Position()
    position.x, position.y, position.z = coord
    return position


def _setup_scenario():
    manager = build_scenario_manager(
        water_gravity_flow_world_factory,
        "Water Gravity Flow Into Empty Cell",
    )
    voxel_grid = manager.current.world.get_voxel_grid()
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
