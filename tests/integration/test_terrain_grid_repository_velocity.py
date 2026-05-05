"""
Tests for TerrainGridRepository velocity storage (Task A: velocity to VDB).

Verifies that get/set velocity on TerrainGridRepository reads and writes
to OpenVDB FloatGrids, not to ECS. Tests go through the Python-exposed
public surface: world.get_voxel_grid().terrain_grid_repository.
"""

import pytest

import aetherion


@pytest.fixture()
def repo():
    """Small world with initialised voxel grid; yields the TerrainGridRepository."""
    world = aetherion.World(10, 10, 10)
    world.initialize_voxel_grid()
    vg = world.get_voxel_grid()
    yield vg.terrain_grid_repository


class TestGetSetVelocityRoundTrip:
    def test_default_velocity_is_zero(self, repo):
        vx, vy, vz = repo.get_terrain_velocity(5, 5, 5)
        assert vx == 0.0 and vy == 0.0 and vz == 0.0

    def test_set_then_get_returns_same_values(self, repo):
        repo.set_terrain_velocity(3, 4, 5, 1.5, -0.5, 2.0)
        vx, vy, vz = repo.get_terrain_velocity(3, 4, 5)
        assert pytest.approx(vx) == 1.5
        assert pytest.approx(vy) == -0.5
        assert pytest.approx(vz) == 2.0

    def test_velocity_isolated_to_voxel(self, repo):
        repo.set_terrain_velocity(0, 0, 0, 3.0, 3.0, 3.0)
        vx, vy, vz = repo.get_terrain_velocity(1, 0, 0)
        assert vx == 0.0 and vy == 0.0 and vz == 0.0

    def test_overwrite_velocity(self, repo):
        repo.set_terrain_velocity(2, 2, 2, 10.0, 10.0, 10.0)
        repo.set_terrain_velocity(2, 2, 2, 0.1, 0.2, 0.3)
        vx, vy, vz = repo.get_terrain_velocity(2, 2, 2)
        assert pytest.approx(vx) == 0.1
        assert pytest.approx(vy) == 0.2
        assert pytest.approx(vz) == 0.3

    def test_set_zero_velocity_reads_back_zero(self, repo):
        repo.set_terrain_velocity(1, 1, 1, 5.0, 0.0, 0.0)
        repo.set_terrain_velocity(1, 1, 1, 0.0, 0.0, 0.0)
        vx, vy, vz = repo.get_terrain_velocity(1, 1, 1)
        assert vx == 0.0 and vy == 0.0 and vz == 0.0

    def test_multiple_voxels_independent(self, repo):
        repo.set_terrain_velocity(0, 0, 0, 1.0, 0.0, 0.0)
        repo.set_terrain_velocity(0, 0, 1, 0.0, 2.0, 0.0)
        repo.set_terrain_velocity(0, 0, 2, 0.0, 0.0, 3.0)
        vx0, _, _ = repo.get_terrain_velocity(0, 0, 0)
        _, vy1, _ = repo.get_terrain_velocity(0, 0, 1)
        _, _, vz2 = repo.get_terrain_velocity(0, 0, 2)
        assert pytest.approx(vx0) == 1.0
        assert pytest.approx(vy1) == 2.0
        assert pytest.approx(vz2) == 3.0
