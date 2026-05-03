"""V2: count_active_water_matter_voxels / count_active_vapor_matter_voxels must
yield exactly the voxels written via set_terrain_matter_container.

Mirrors test_velocity_vdb_iteration.py — verifies the VDB activation contract
for the water and vapor matter grids: writing non-zero activates, writing 0
deactivates. Without this contract, drained cells stay in the active set
forever and pollute future sparse iteration over `waterMatterGrid` /
`vaporMatterGrid`.
"""

from helpers import (
    build_minimal_test_manager,
    place_empty,
    place_stone,
    place_vapor,
    place_water,
)


def _repo(manager):
    return manager.current.world.get_voxel_grid().terrain_grid_repository


def test_water_matter_count_zero_on_fresh_world():
    manager = build_minimal_test_manager(5, 5, 5)
    repo = _repo(manager)
    assert repo.count_active_water_matter_voxels() == 0
    assert repo.count_active_vapor_matter_voxels() == 0


def test_place_water_activates_water_matter_voxel():
    manager = build_minimal_test_manager(5, 5, 5)
    voxel_grid = manager.current.world.get_voxel_grid()
    place_water(voxel_grid, 2, 2, 1, water_matter=500)
    repo = _repo(manager)
    assert repo.count_active_water_matter_voxels() == 1
    assert repo.count_active_vapor_matter_voxels() == 0


def test_place_vapor_activates_vapor_matter_voxel():
    manager = build_minimal_test_manager(5, 5, 5)
    voxel_grid = manager.current.world.get_voxel_grid()
    place_vapor(voxel_grid, 2, 2, 3, water_vapor=200)
    repo = _repo(manager)
    assert repo.count_active_vapor_matter_voxels() == 1
    assert repo.count_active_water_matter_voxels() == 0


def test_zero_water_matter_deactivates_voxel():
    """Writing water_matter=0 via set_terrain_matter_container must deactivate
    the voxel — otherwise drained cells stay in the active set forever."""
    manager = build_minimal_test_manager(5, 5, 5)
    voxel_grid = manager.current.world.get_voxel_grid()
    place_water(voxel_grid, 2, 2, 1, water_matter=500)
    assert _repo(manager).count_active_water_matter_voxels() == 1

    # Drain via place_empty (writes a zeroed MatterContainer).
    place_empty(voxel_grid, 2, 2, 1)
    assert _repo(manager).count_active_water_matter_voxels() == 0


def test_zero_vapor_matter_deactivates_voxel():
    manager = build_minimal_test_manager(5, 5, 5)
    voxel_grid = manager.current.world.get_voxel_grid()
    place_vapor(voxel_grid, 2, 2, 3, water_vapor=200)
    assert _repo(manager).count_active_vapor_matter_voxels() == 1

    place_empty(voxel_grid, 2, 2, 3)
    assert _repo(manager).count_active_vapor_matter_voxels() == 0


def test_multiple_matter_voxels_counted_independently():
    manager = build_minimal_test_manager(8, 8, 5)
    voxel_grid = manager.current.world.get_voxel_grid()
    place_water(voxel_grid, 1, 1, 1, water_matter=100)
    place_water(voxel_grid, 2, 2, 1, water_matter=200)
    place_water(voxel_grid, 3, 3, 1, water_matter=300)
    place_vapor(voxel_grid, 4, 4, 3, water_vapor=50)
    place_vapor(voxel_grid, 5, 5, 3, water_vapor=75)

    repo = _repo(manager)
    assert repo.count_active_water_matter_voxels() == 3
    assert repo.count_active_vapor_matter_voxels() == 2

    place_empty(voxel_grid, 2, 2, 1)
    assert repo.count_active_water_matter_voxels() == 2
    assert repo.count_active_vapor_matter_voxels() == 2


def test_stone_does_not_activate_matter_grids():
    """place_stone writes terrain_matter only — water/vapor grids must stay clean."""
    manager = build_minimal_test_manager(5, 5, 5)
    voxel_grid = manager.current.world.get_voxel_grid()
    for x in range(5):
        for y in range(5):
            place_stone(voxel_grid, x, y, 0)

    repo = _repo(manager)
    assert repo.count_active_water_matter_voxels() == 0
    assert repo.count_active_vapor_matter_voxels() == 0
