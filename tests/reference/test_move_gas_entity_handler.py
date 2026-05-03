"""onMoveGasEntityEvent must process ON_GRID_STORAGE vapor cells end-to-end.

Two prior bugs combined to keep these tests honest:

1. The handler had a sanity gate `terrainId != static_cast<int>(event.entity)`
   that silently dropped events when the entity field carried the
   ON_GRID_STORAGE sentinel — `static_cast<int>(entt::entity)` is not a
   reliable round-trip for negative sentinel values. Fix: bypass the gate
   for ON_GRID_STORAGE cells.
2. The shared velocity-translation function picked a single dominant axis
   when the global `allow_multi_direction` flag was off (a player/NPC config
   that should not gate fluid simulation). Fix: dedicated multi-axis variant
   for fluid handlers (vapor / liquid water).

After both fixes (and the `deleteTerrain` orphan-matter fix), a successful
gas event causes the cell to MIGRATE from its source coord to a destination
in the same tick. So these tests assert at the destination, not the source.
"""

from helpers import (
    build_minimal_test_manager,
    make_position,
    place_empty,
    place_vapor,
)


def _repo(manager):
    return manager.current.world.get_voxel_grid().terrain_grid_repository


def test_move_gas_event_lifts_on_grid_storage_vapor():
    """Buoyancy alone (no horizontal force) — vapor at (2,2,2) should migrate
    to (2,2,3) within one update. Pre-fix the handler silently returned and
    nothing happened; post-fix the cell moves up cleanly with no orphan
    matter or velocity at the source."""
    manager = build_minimal_test_manager(5, 5, 5)
    voxel_grid = manager.current.world.get_voxel_grid()
    place_vapor(voxel_grid, 2, 2, 2, water_vapor=200)

    repo = _repo(manager)
    assert repo.count_active_vapor_matter_voxels() == 1
    assert voxel_grid.get_terrain(2, 2, 2) == -1  # ON_GRID_STORAGE
    assert voxel_grid.get_terrain(2, 2, 3) == -2  # NONE

    manager.current.world.dispatch_move_gas_entity_event(
        position=make_position(2, 2, 2),
        # entity defaults to ON_GRID_STORAGE (-1) — the path that was broken.
    )
    manager.update()

    # Source must be fully cleared — terrain id, matter, and velocity. The
    # `deleteTerrain` orphan fix guarantees this; a regression here means
    # `setValueOff(coord)` slipped back in somewhere on the move path.
    assert voxel_grid.get_terrain(2, 2, 2) == -2, "Source cell should be empty after the buoyancy-driven move"
    src_matter = repo.get_terrain_matter_container(2, 2, 2)
    assert src_matter.water_vapor == 0, (
        f"Source vapor must be cleared (orphan-matter regression check); got {src_matter.water_vapor}"
    )
    src_vel = repo.get_terrain_velocity(2, 2, 2)
    assert src_vel == (0.0, 0.0, 0.0), f"Source velocity must be cleared post-move; got {src_vel}"

    # Destination at (2,2,3) holds the migrated vapor with the buoyancy
    # velocity carried over by `moveTerrain`.
    assert voxel_grid.get_terrain(2, 2, 3) == -1, "Vapor should have migrated up to (2,2,3) as ON_GRID_STORAGE"
    dst_matter = repo.get_terrain_matter_container(2, 2, 3)
    assert dst_matter.water_vapor == 200, (
        f"Vapor amount must be preserved across the move; got {dst_matter.water_vapor}"
    )
    _, _, dst_vz = repo.get_terrain_velocity(2, 2, 3)
    assert dst_vz > 0.0, f"Destination should retain the upward buoyancy velocity; got vz={dst_vz}"

    # No vapor was lost or duplicated.
    assert repo.count_active_vapor_matter_voxels() == 1, "Total vapor cell count must be conserved (1 cell, just moved)"


def test_move_gas_event_applies_horizontal_force_to_on_grid_storage_vapor():
    """Multi-axis fluid handling — a buoyancy event with non-zero forceX
    must produce velocity on BOTH axes (not pick a single dominant axis).
    The cell migrates diagonally to (3,2,3); destination velocity is
    (5,0,5) (multi-axis newVel clamped to maxSpeed/2 because two axes are
    nonzero)."""
    manager = build_minimal_test_manager(5, 5, 5)
    voxel_grid = manager.current.world.get_voxel_grid()
    place_vapor(voxel_grid, 2, 2, 2, water_vapor=200)

    repo = _repo(manager)
    assert voxel_grid.get_terrain(3, 2, 3) == -2, "destination must start empty"

    manager.current.world.dispatch_move_gas_entity_event(
        position=make_position(2, 2, 2),
        force_x=500.0,
        force_y=0.0,
    )
    manager.update()

    # Cell migrated diagonally up-and-right. Source empty.
    assert voxel_grid.get_terrain(2, 2, 2) == -2, "Source cell should be empty after the multi-axis move"
    assert voxel_grid.get_terrain(3, 2, 3) == -1, (
        "Vapor should have migrated diagonally to (3,2,3) — proves both "
        "horizontal force AND vertical buoyancy contributed (W1 multi-axis)"
    )
    dst_matter = repo.get_terrain_matter_container(3, 2, 3)
    assert dst_matter.water_vapor == 200, (
        f"Vapor amount must be preserved across the diagonal move; got {dst_matter.water_vapor}"
    )

    # The velocity carried to the destination must have BOTH x and z
    # components — that's the W1 multi-axis fix at work. With the
    # single-dominant-axis behaviour, vx would have stayed 0.
    dst_vx, _, dst_vz = repo.get_terrain_velocity(3, 2, 3)
    assert dst_vx > 0.0, f"Horizontal force must contribute to velocity; got vx={dst_vx}"
    assert dst_vz > 0.0, f"Vertical buoyancy must coexist with horizontal force; got vz={dst_vz}"

    assert repo.count_active_vapor_matter_voxels() == 1, "Vapor cell count must be conserved across the move"


def test_move_gas_event_is_a_noop_at_none_cell():
    """Step 3 of the handler exits early when terrainId == NONE. That branch
    was already correct — verify the V3.1 fix did not regress it."""
    manager = build_minimal_test_manager(5, 5, 5)
    voxel_grid = manager.current.world.get_voxel_grid()
    place_empty(voxel_grid, 2, 2, 2)

    manager.current.world.dispatch_move_gas_entity_event(
        position=make_position(2, 2, 2),
    )
    manager.update()

    vx, vy, vz = _repo(manager).get_terrain_velocity(2, 2, 2)
    assert (vx, vy, vz) == (0.0, 0.0, 0.0), f"NONE cell must not receive velocity; got ({vx}, {vy}, {vz})"
