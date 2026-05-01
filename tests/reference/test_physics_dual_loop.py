"""C-Pre.2: Dual-loop processPhysics — ECS view + VDB terrain loop.

Pre-C1–C4 the production physics loop never writes to VDB velocity, so
Loop 2 yields nothing during normal simulation. To exercise Loop 2's code
path we seed VDB velocity directly on an ON_GRID_STORAGE voxel.

Note: the dedup path (Loop 2 skips voxels that have an ECS entity) cannot
be tested cleanly pre-C1–C4 because the mountain_side world has no
ECS-backed terrain (only ON_GRID_STORAGE rocks, terrain_id == -1).
That assertion is deferred to post-C1–C4 tests once water becomes
ON_GRID_STORAGE and the dual-state can be constructed naturally.
"""

from __future__ import annotations

from helpers import build_mountain_side_manager


def test_loop2_does_not_crash_during_normal_simulation():
    """Pre-C1–C4 Loop 2 yields nothing; the simulation must complete cleanly."""
    manager = build_mountain_side_manager("Dual Loop No Crash Test")

    for _ in range(50):
        manager.update()

    repo = manager.current.world.get_voxel_grid().terrain_grid_repository
    # Pre-C1–C4: physics loop writes ECS Velocity, not VDB; VDB count stays 0.
    assert repo.count_active_velocity_voxels() == 0, (
        "Pre-C1–C4 the physics loop must not populate VDB velocity grids"
    )


def test_loop2_processes_vdb_velocity_on_on_grid_storage_voxel():
    """Loop 2 must pick up VDB-only velocity (no ECS entity) without crashing.

    We seed VDB velocity on an ON_GRID_STORAGE voxel (terrain_id == -1).
    Loop 2's contract: iterate VDB velocity voxels, skip any that have an ECS
    entity, process the rest. With no ECS entity present, Loop 2 must run its
    full processing path (gravity, friction, optional move) without throwing.
    """
    manager = build_mountain_side_manager("Dual Loop OGS Test")
    manager.update()

    world = manager.current.world
    voxel_grid = world.get_voxel_grid()
    repo = voxel_grid.terrain_grid_repository

    # Find an ON_GRID_STORAGE voxel (terrain_id == -1) — mountain rock.
    found = None
    for x in range(world.width):
        for y in range(world.height):
            for z in range(world.depth):
                if voxel_grid.get_terrain(x, y, z) == -1:
                    found = (x, y, z)
                    break
            if found:
                break
        if found:
            break

    assert found is not None, "Expected at least one ON_GRID_STORAGE voxel"
    x, y, z = found

    # Seed VDB velocity directly. Pre-C1–C4 the physics loop won't touch this
    # via the ECS path, so any change comes from Loop 2.
    repo.set_terrain_velocity(x, y, z, 0.0, 0.0, 0.5)
    assert repo.count_active_velocity_voxels() == 1

    # Run a physics step. Loop 2 iterates VDB velocity, finds this voxel,
    # confirms it has no ECS entity, and processes it. Must not crash.
    manager.update()
