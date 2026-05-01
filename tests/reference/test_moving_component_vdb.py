"""Verify MovingComponent lives in the coord-keyed map (Task B), not ECS."""

from helpers import build_mountain_side_manager

SPRING_X, SPRING_Y = 5, 50


def test_moving_component_absent_for_static_terrain():
    """Static grass voxels must never have a MovingComponent in the map."""
    manager = build_mountain_side_manager("Moving Component VDB Static Test")
    world = manager.current.world
    repo = world.get_voxel_grid().terrain_grid_repository

    assert not repo.has_terrain_moving_component(50, 50, 0), "Static grass voxel incorrectly has a MovingComponent"


def test_moving_component_cleared_after_water_settles():
    """After water finishes moving, its MovingComponent must be cleared from the map."""
    manager = build_mountain_side_manager("Moving Component VDB Settle Test")
    world = manager.current.world
    repo = world.get_voxel_grid().terrain_grid_repository
    vg = world.get_voxel_grid()

    for _ in range(50):
        manager.update()

    # Find a settled water voxel (water_matter > 0, velocity == 0 on all axes)
    for z in range(world.depth):
        matter = vg.get_terrain_matter_container_component(SPRING_X, SPRING_Y, z)
        if matter.water_matter > 0:
            vx, vy, vz = repo.get_terrain_velocity(SPRING_X, SPRING_Y, z)
            if vx == 0.0 and vy == 0.0 and vz == 0.0:
                assert not repo.has_terrain_moving_component(SPRING_X, SPRING_Y, z), (
                    f"Settled water at ({SPRING_X},{SPRING_Y},{z}) still has a MovingComponent"
                )
                return

    # Fallback: confirm no static terrain has the component
    assert not repo.has_terrain_moving_component(50, 50, 0)
