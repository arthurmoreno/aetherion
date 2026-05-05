"""C-Pre.1: iterateVelocityVoxels must yield exactly the voxels written via setVelocity.

These tests verify the VDB iteration infrastructure in isolation. They do NOT rely
on the physics loop writing velocity to VDB (that is C-Pre.2's concern). Instead,
they write directly via set_terrain_velocity and assert the iteration count.
"""

from helpers import build_mountain_side_manager


def test_velocity_count_zero_on_fresh_world():
    """No velocity has been written to VDB — count must be 0."""
    manager = build_mountain_side_manager("Vel Iter Zero Test")
    repo = manager.current.world.get_voxel_grid().terrain_grid_repository
    assert repo.count_active_velocity_voxels() == 0


def test_set_velocity_activates_voxel():
    """Writing a non-zero velocity via set_terrain_velocity must register in VDB."""
    manager = build_mountain_side_manager("Vel Iter Set Test")
    repo = manager.current.world.get_voxel_grid().terrain_grid_repository

    repo.set_terrain_velocity(10, 10, 0, 1.0, 0.0, 0.0)
    assert repo.count_active_velocity_voxels() == 1


def test_clear_velocity_deactivates_voxel():
    """Setting velocity to (0,0,0) must deactivate the voxel (not show up in iteration)."""
    manager = build_mountain_side_manager("Vel Iter Clear Test")
    repo = manager.current.world.get_voxel_grid().terrain_grid_repository

    repo.set_terrain_velocity(10, 10, 0, 1.0, 0.0, 0.0)
    assert repo.count_active_velocity_voxels() == 1

    repo.set_terrain_velocity(10, 10, 0, 0.0, 0.0, 0.0)
    assert repo.count_active_velocity_voxels() == 0


def test_multiple_velocity_voxels_counted():
    """Each distinct voxel with non-zero velocity adds one to the count."""
    manager = build_mountain_side_manager("Vel Iter Multi Test")
    repo = manager.current.world.get_voxel_grid().terrain_grid_repository

    repo.set_terrain_velocity(5, 5, 0, 1.0, 0.0, 0.0)
    repo.set_terrain_velocity(6, 5, 0, 0.0, 1.0, 0.0)
    repo.set_terrain_velocity(7, 5, 0, 0.0, 0.0, -1.0)
    assert repo.count_active_velocity_voxels() == 3

    # Clearing one reduces the count
    repo.set_terrain_velocity(6, 5, 0, 0.0, 0.0, 0.0)
    assert repo.count_active_velocity_voxels() == 2


def test_get_terrain_velocity_matches_set():
    """After set_terrain_velocity, get_terrain_velocity must return the same values."""
    manager = build_mountain_side_manager("Vel Iter Roundtrip Test")
    repo = manager.current.world.get_voxel_grid().terrain_grid_repository

    repo.set_terrain_velocity(20, 30, 2, 1.5, -0.5, 2.0)
    vx, vy, vz = repo.get_terrain_velocity(20, 30, 2)
    assert abs(vx - 1.5) < 1e-5, f"vx mismatch: {vx}"
    assert abs(vy - (-0.5)) < 1e-5, f"vy mismatch: {vy}"
    assert abs(vz - 2.0) < 1e-5, f"vz mismatch: {vz}"
