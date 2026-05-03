"""Investigation: where does ON_GRID_STORAGE vapor go after a buoyancy event?

The user reports vapor stalling immediately after evaporation in the live game.
The lift assertion in test_move_gas_entity_handler shows vz>0 at the source
cell — but does that mean the cell moved, or that it's still at the source?
This file probes the full state after the dispatch + update so we can tell.
"""

from __future__ import annotations

from helpers import (
    build_minimal_test_manager,
    make_position,
    place_stone,
    place_vapor,
)


def _dump(label, voxel_grid, repo, x, y, z):
    """Pretty-print everything that matters about a cell."""
    tid = voxel_grid.get_terrain(x, y, z)
    matter = repo.get_terrain_matter_container(x, y, z)
    vx, vy, vz = repo.get_terrain_velocity(x, y, z)
    print(
        f"  {label} ({x},{y},{z}): "
        f"tid={tid} water={matter.water_matter} vapor={matter.water_vapor} "
        f"vel=({vx:.3f},{vy:.3f},{vz:.3f})"
    )


def test_investigate_where_vapor_ends_up():
    """Create vapor at (2,2,2) on a stone floor, dispatch a buoyancy event,
    update once, dump every cell in the column. Diagnostic only — does not
    fail."""
    manager = build_minimal_test_manager(5, 5, 5)
    voxel_grid = manager.current.world.get_voxel_grid()
    repo = voxel_grid.terrain_grid_repository

    # Stone floor + vapor sitting at z=2
    for x in range(5):
        for y in range(5):
            place_stone(voxel_grid, x, y, 0)
    place_vapor(voxel_grid, 2, 2, 2, water_vapor=200)

    print("\n=== BEFORE dispatch ===")
    for z in range(5):
        _dump(f"z={z}", voxel_grid, repo, 2, 2, z)
    print(f"  active velocity voxels: {repo.count_active_velocity_voxels()}")
    print(f"  active vapor voxels:    {repo.count_active_vapor_matter_voxels()}")

    manager.current.world.dispatch_move_gas_entity_event(
        position=make_position(2, 2, 2),
    )
    manager.update()

    print("\n=== AFTER 1 update ===")
    for z in range(5):
        _dump(f"z={z}", voxel_grid, repo, 2, 2, z)
    print(f"  active velocity voxels: {repo.count_active_velocity_voxels()}")
    print(f"  active vapor voxels:    {repo.count_active_vapor_matter_voxels()}")

    # Don't fail — this is exploratory.
    assert True


def test_investigate_horizontal_force_destination():
    """Where does a vapor cell at (2,2,2) end up after a buoyancy + horizontal
    force event? Multi-axis fluid handling produces velocity on both x and z;
    the move-destination algorithm picks one."""
    manager = build_minimal_test_manager(5, 5, 5)
    voxel_grid = manager.current.world.get_voxel_grid()
    repo = voxel_grid.terrain_grid_repository

    place_vapor(voxel_grid, 2, 2, 2, water_vapor=200)

    print("\n=== BEFORE dispatch (horizontal+buoyancy) ===")
    for x in range(5):
        for z in range(5):
            tid = voxel_grid.get_terrain(x, 2, z)
            if tid != -2:
                _dump(f"x={x} z={z}", voxel_grid, repo, x, 2, z)

    manager.current.world.dispatch_move_gas_entity_event(
        position=make_position(2, 2, 2),
        force_x=500.0,
        force_y=0.0,
    )
    manager.update()

    print("\n=== AFTER 1 update (horizontal+buoyancy) ===")
    for x in range(5):
        for z in range(5):
            tid = voxel_grid.get_terrain(x, 2, z)
            if tid != -2:
                _dump(f"x={x} z={z}", voxel_grid, repo, x, 2, z)

    assert True


def test_investigate_vapor_stack_above_water_evaporation_pattern():
    """Reproduce the live-game scenario: water at z=1, vapor evaporated at z=2,
    then dispatch a buoyancy event and watch what happens. Mirrors what the
    user reports: vapor created above water, then immediately stalls."""
    manager = build_minimal_test_manager(5, 5, 5)
    voxel_grid = manager.current.world.get_voxel_grid()
    repo = voxel_grid.terrain_grid_repository

    # Water surface; vapor just above it
    for x in range(5):
        for y in range(5):
            place_stone(voxel_grid, x, y, 0)
    # Use place_vapor to put a vapor cell at z=2 (mimicking post-evaporation)
    place_vapor(voxel_grid, 2, 2, 2, water_vapor=20)

    print("\n=== BEFORE 5 updates ===")
    for z in range(5):
        _dump(f"z={z}", voxel_grid, repo, 2, 2, z)

    for tick in range(5):
        manager.current.world.dispatch_move_gas_entity_event(
            position=make_position(2, 2, 2),
        )
        manager.update()
        print(f"\n=== AFTER tick {tick + 1} ===")
        for z in range(5):
            _dump(f"z={z}", voxel_grid, repo, 2, 2, z)

    assert True
