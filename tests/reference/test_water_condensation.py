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
        f"Destination at {CONDENSATION_TARGET_POS} has tid={target_terrain_id}, expected -1 (ON_GRID_STORAGE)"
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
        f"Total water (liquid + vapor) drifted from {initial_total} to {final_total} during condensation"
    )
    assert water_matter(voxel_grid, *CONDENSATION_TARGET_POS) == CONDENSATION_AMOUNT
    assert water_vapor(voxel_grid, *CONDENSATION_VAPOR_POS) == CONDENSATION_INITIAL_VAPOR - CONDENSATION_AMOUNT


def test_condense_into_vapor_destination_aborts_without_invariant_violation():
    """Condensation aimed at a cell that *currently* holds vapor — even
    though the event's snapshot said the cell was NONE — must not write
    `WaterMatter` into it. The handler's branch decision is taken from
    the event payload (`terrain_below_id == NONE`), but the actual cell
    state can change between event enqueue and dispatch (cascading
    vapor merges, parallel condensation events, etc.). Writing liquid
    on top of pure vapor would leave the cell with both fields > 0,
    which the per-tick water-invariant check forbids.

    The retry-then-abort guard preserves both the source vapor and the
    pre-existing destination vapor, and after enough ticks for the
    retry counter to exhaust, no invariant violation is observed and
    matter is conserved.
    """
    manager, voxel_grid = _setup_scenario()

    # Reach into the world and place vapor at the would-be destination
    # *now*, after the scenario factory has run. This mimics what
    # happens in the live game when an unrelated handler populates the
    # cell with vapor while a pending `CondenseWaterEntityEvent` (whose
    # `terrain_below_id` was captured as NONE) is still queued.
    from aetherion import EntityEnum, EntityTypeComponent, MatterContainer, MatterState, TerrainEnum

    repo = voxel_grid.terrain_grid_repository
    target_x, target_y, target_z = CONDENSATION_TARGET_POS
    voxel_grid.set_terrain_id_raw(target_x, target_y, target_z, -1)  # ON_GRID_STORAGE

    target_type = EntityTypeComponent()
    target_type.main_type = EntityEnum.TERRAIN.value
    target_type.sub_type0 = TerrainEnum.WATER.value
    target_type.sub_type1 = 0
    repo.set_terrain_entity_type(target_x, target_y, target_z, target_type, True)

    target_matter = MatterContainer()
    target_matter.terrain_matter = 0
    target_matter.water_matter = 0
    target_matter.water_vapor = 7
    target_matter.bio_mass_matter = 0
    repo.set_terrain_matter_container(target_x, target_y, target_z, target_matter)
    repo.set_matter_state(target_x, target_y, target_z, MatterState.GAS)

    initial_total = repo.sum_total_water()

    # ACT — dispatch with terrain_below_id = -2 (NONE), forcing the
    # handler into the createWaterTerrainBelowVapor branch even though
    # actual state is now vapor. Tick enough times for the retry cap
    # to fire.
    manager.current.world.dispatch_condense_water_event(
        vapor_pos=_coord_to_position(CONDENSATION_VAPOR_POS),
        condensation_amount=CONDENSATION_AMOUNT,
        terrain_below_id=-2,
    )
    for _ in range(8):
        manager.update()

    # ASSERT — destination vapor untouched, source vapor untouched, no
    # invariant violation, total water conserved.
    assert water_matter(voxel_grid, *CONDENSATION_TARGET_POS) == 0, (
        "Destination vapor cell must not have liquid water written into it"
    )
    assert water_vapor(voxel_grid, *CONDENSATION_TARGET_POS) == 7, "Destination vapor amount must be untouched"
    assert water_vapor(voxel_grid, *CONDENSATION_VAPOR_POS) == CONDENSATION_INITIAL_VAPOR, (
        "Source vapor must keep its full amount when condensation aborts"
    )
    assert repo.sum_total_water() == initial_total
