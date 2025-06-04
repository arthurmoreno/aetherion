from lifesimcore import DirectionEnum, EntityInterface, Position, WorldView, WorldViewFlatB


def test_world_view_structpack_serialization():
    # Create a WorldView instance with test data
    world_view = WorldView()

    # Initialize the voxel grid
    world_view.voxelGridView.initVoxelGridView(5, 5, 5, 0, 0, 0)
    # Set some voxel data
    world_view.voxelGridView.set_entity(1, 1, 1, 7)

    # Add some entities to the world view
    entity1 = EntityInterface()
    entity1.set_entity_id(1)
    # Add components to entity1
    position1 = Position()
    position1.x = 10
    position1.y = 20
    position1.z = 30
    position1.direction = DirectionEnum.DOWN
    entity1.set_position(position1)
    world_view.entities[1] = entity1

    entity2 = EntityInterface()
    entity2.set_entity_id(2)
    # Add components to entity2
    position2 = Position()
    position2.x = 15
    position2.y = 25
    position2.z = 35
    position2.direction = DirectionEnum.UP
    entity2.set_position(position2)
    world_view.entities[2] = entity2

    # Verify the deserialized data matches the original
    # Check voxel grid dimensions
    assert world_view.voxelGridView.width == 5
    assert world_view.voxelGridView.height == 5
    assert world_view.voxelGridView.depth == 5


def test_world_view_flatbuffer_access():
    # Create a WorldView instance with test data
    world_view = WorldView()

    # Initialize the voxel grid
    world_view.voxelGridView.initVoxelGridView(5, 5, 5, 0, 0, 0)
    # Set some voxel data
    world_view.voxelGridView.set_entity(1, 1, 1, 7)

    # Add some entities to the world view
    entity1 = EntityInterface()
    entity1.set_entity_id(1)
    # Add components to entity1
    position1 = Position()
    position1.x = 10
    position1.y = 20
    position1.z = 30
    position1.direction = DirectionEnum.DOWN
    entity1.set_position(position1)
    world_view.addEntity(1, entity1)
    world_view.voxelGridView.set_entity(3, 3, 2, 1)

    entity2 = EntityInterface()
    entity2.set_entity_id(2)
    # Add components to entity2
    position2 = Position()
    position2.x = 15
    position2.y = 25
    position2.z = 35
    position2.direction = DirectionEnum.UP
    entity2.set_position(position2)
    world_view.addEntity(2, entity2)
    world_view.voxelGridView.set_entity(3, 2, 2, 2)

    # Serialize the world view
    serialized_data = world_view.serialize_flatbuffer()
    flatb_accessor = WorldViewFlatB(serialized_data)

    accessed_entity = flatb_accessor.get_entity(3, 3, 2)
    assert accessed_entity.get_position().x == entity1.get_position().x
    assert accessed_entity.get_position().y == entity1.get_position().y
