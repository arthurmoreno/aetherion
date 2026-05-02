"""Vapor → liquid condensation lands water as ON_GRID_STORAGE.

Drives `_handleCondenseWaterEntityEvent` → `createWaterTerrainBelowVapor`
through the `water_condense_below_vapor_world_factory` reference scenario:
vapor at ``CONDENSATION_VAPOR_POS`` condenses into the empty cell at
``CONDENSATION_TARGET_POS``. Tests assert (1) the destination cell ends as
``ON_GRID_STORAGE`` (terrain id ``-1``), no positive entity handle, and
(2) total water (liquid + vapor) is conserved across the phase change.
"""

from __future__ import annotations

from helpers import build_scenario_manager, water_matter, water_vapor

from aetherion import Position
from aetherion.reference.world.scenarios import (
    CONDENSATION_AMOUNT,
    CONDENSATION_INITIAL_VAPOR,
    CONDENSATION_TARGET_POS,
    CONDENSATION_VAPOR_POS,
    water_condense_below_vapor_world_factory,
)


def _coord_to_position(coord: tuple[int, int, int]) -> Position:
    position = Position()
    position.x, position.y, position.z = coord
    return position


def _setup_scenario():
    manager = build_scenario_manager(
        water_condense_below_vapor_world_factory,
        "Water Condensation Below Vapor",
    )
    voxel_grid = manager.current.world.get_voxel_grid()
    return manager, voxel_grid


def test_condensation_lands_water_as_on_grid_storage():
    """Vapor at VAPOR_POS condenses into an empty cell at TARGET_POS. After
    dispatching a `CondenseWaterEntityEvent` and ticking once, the destination
    must be populated water with terrain id ``-1`` (ON_GRID_STORAGE), not a
    positive EnTT entity handle.

    RED today because `createWaterTerrainBelowVapor` calls `registry.create()`
    and writes that entity handle into the terrain id grid.
    """
    manager, voxel_grid = _setup_scenario()

    assert water_vapor(voxel_grid, *CONDENSATION_VAPOR_POS) == CONDENSATION_INITIAL_VAPOR
    assert voxel_grid.get_terrain(*CONDENSATION_TARGET_POS) == -2  # NONE

    manager.current.world.dispatch_condense_water_event(
        vapor_pos=_coord_to_position(CONDENSATION_VAPOR_POS),
        condensation_amount=CONDENSATION_AMOUNT,
    )
    manager.update()

    target_terrain_id = voxel_grid.get_terrain(*CONDENSATION_TARGET_POS)
    assert target_terrain_id == -1, (
        f"Destination at {CONDENSATION_TARGET_POS} has tid={target_terrain_id}, "
        f"expected -1 (ON_GRID_STORAGE)"
    )


def test_condensation_conserves_total_water():
    """Total water (liquid + vapor) across the world must be exactly preserved
    as vapor condenses into the destination — no overwrite, no double-count."""
    manager, voxel_grid = _setup_scenario()
    repo = voxel_grid.terrain_grid_repository
    initial_total = repo.sum_total_water()
    assert initial_total == CONDENSATION_INITIAL_VAPOR, "Setup invariant"

    manager.current.world.dispatch_condense_water_event(
        vapor_pos=_coord_to_position(CONDENSATION_VAPOR_POS),
        condensation_amount=CONDENSATION_AMOUNT,
    )
    manager.update()

    final_total = repo.sum_total_water()
    assert final_total == initial_total, (
        f"Total water (liquid + vapor) drifted from {initial_total} to {final_total} "
        f"during condensation"
    )
    assert water_matter(voxel_grid, *CONDENSATION_TARGET_POS) == CONDENSATION_AMOUNT
    assert (
        water_vapor(voxel_grid, *CONDENSATION_VAPOR_POS)
        == CONDENSATION_INITIAL_VAPOR - CONDENSATION_AMOUNT
    )
