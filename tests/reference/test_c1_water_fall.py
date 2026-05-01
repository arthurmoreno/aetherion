"""C1: createWaterTerrainFromFall must produce ON_GRID_STORAGE water."""

from __future__ import annotations

from helpers import build_minimal_test_manager, place_stone, place_water


def test_water_falls_and_lands_as_on_grid_storage():
    manager = build_minimal_test_manager(5, 5, 5)
    world = manager.current.world
    voxel_grid = world.get_voxel_grid()

    for x in range(5):
        for y in range(5):
            place_stone(voxel_grid, x, y, 0)

    place_water(voxel_grid, 2, 2, 3, water_matter=1000)
    voxel_grid.terrain_grid_repository.set_terrain_velocity(2, 2, 3, 0.0, 0.0, -1.0)

    for _ in range(5):
        manager.update()

    landed = None
    for z in range(1, 4):
        matter = voxel_grid.get_terrain_matter_container_component(2, 2, z)
        if matter.water_matter > 0:
            landed = z
            break

    assert landed is not None, "Water did not fall"

    terrain_id = voxel_grid.get_terrain(2, 2, landed)
    assert terrain_id == -1, (
        f"Fallen water at z={landed} has tid={terrain_id}, expected -1 (ON_GRID_STORAGE)"
    )
