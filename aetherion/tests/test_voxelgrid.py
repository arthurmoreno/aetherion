from aetherion import GridData, VoxelGrid, VoxelGridCoordinates


def test_set_and_get_voxel():
    """
    Test setting and getting voxel data.
    """
    grid = VoxelGrid()
    grid.initialize_grids()

    # Create a GridData instance
    data = GridData(terrainID=1, entityID=100, eventID=200, lightingLevel=0.75)

    # Set voxel at (10, 20, 30)
    grid.set_voxel(10, 20, 30, data)

    # Retrieve voxel data
    retrieved_data = grid.get_voxel(10, 20, 30)

    assert retrieved_data.terrainID == 1
    assert retrieved_data.entityID == 100
    assert retrieved_data.eventID == 200
    assert retrieved_data.lightingLevel == 0.75


def test_search_terrain_voxels_in_region():
    """
    Test searching for terrain voxels within a specified region.
    """
    grid = VoxelGrid()
    grid.initialize_grids()

    # Insert terrain voxels
    grid.set_terrain(5, 5, 5, 1)
    grid.set_terrain(10, 10, 10, 2)
    grid.set_terrain(15, 15, 15, 3)

    # Define search region
    x_min, y_min, z_min = 0, 0, 0
    x_max, y_max, z_max = 12, 12, 12

    # Search for terrain voxels within the region
    terrain_voxels = grid.get_all_terrain_in_region(x_min, y_min, z_min, x_max, y_max, z_max)

    # Expected voxels: (5,5,5) and (10,10,10)
    expected_voxels = [VoxelGridCoordinates(x=5, y=5, z=5), VoxelGridCoordinates(x=10, y=10, z=10)]

    assert len(terrain_voxels) == len(expected_voxels)
    for voxel in expected_voxels:
        assert voxel in terrain_voxels


def test_search_entity_and_lighting_voxels_in_region():
    """
    Test searching for entity and lighting voxels within a specified region.
    """
    grid = VoxelGrid()
    grid.initialize_grids()

    # Insert entity voxels
    grid.set_entity(20, 20, 20, 300)
    grid.set_entity(25, 25, 25, 400)

    # Insert lighting voxels
    grid.set_lighting_level(20, 20, 20, 0.85)
    grid.set_lighting_level(30, 30, 30, 0.95)

    # Define search region
    x_min, y_min, z_min = 15, 15, 15
    x_max, y_max, z_max = 25, 25, 25

    # Search for entity voxels within the region
    entity_voxels = grid.get_all_entity_in_region(x_min, y_min, z_min, x_max, y_max, z_max)

    # Expected entity voxel: (20,20,20) and (25,25,25) is on the boundary
    expected_entity_voxels = [VoxelGridCoordinates(x=20, y=20, z=20), VoxelGridCoordinates(x=25, y=25, z=25)]

    assert len(entity_voxels) == len(expected_entity_voxels)
    for voxel in expected_entity_voxels:
        assert voxel in entity_voxels

    # Search for lighting voxels within the region
    lighting_voxels = grid.get_all_lighting_in_region(x_min, y_min, z_min, x_max, y_max, z_max)

    # Expected lighting voxel: (20,20,20)
    expected_lighting_voxels = [VoxelGridCoordinates(x=20, y=20, z=20)]

    assert len(lighting_voxels) == len(expected_lighting_voxels)
    for voxel in expected_lighting_voxels:
        assert voxel in lighting_voxels
