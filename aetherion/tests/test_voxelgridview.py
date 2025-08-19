from aetherion import VoxelGridView


def test_voxelgridview_serialization():
    # Create a VoxelGridView instance with test data
    voxel_grid = VoxelGridView()
    voxel_grid.initVoxelGridView(5, 5, 5, 0, 0, 0)  # Set up the grid
    voxel_grid.set_entity(1, 1, 1, 7)  # Add some voxel data

    # Verify the deserialized data matches the original
    assert voxel_grid.get_entity(1, 1, 1) == 7
