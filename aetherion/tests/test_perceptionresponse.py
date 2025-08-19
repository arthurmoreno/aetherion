from aetherion import (
    DirectionEnum,
    EntityInterface,
    PerceptionResponse,
    PerceptionResponseFlatB,
    Position,
    WorldView,
)


def test_perception_response_serialization():
    # Create an EntityInterface and a WorldView for PerceptionResponse
    entity = EntityInterface()
    entity.set_entity_id(2)
    world_view = WorldView()
    world_view.voxelGridView.initVoxelGridView(3, 3, 3, 0, 0, 0)

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

    # Create the perception response
    perception_response_serialized = PerceptionResponse(entity, world_view).serialize_flatbuffer()
    response_flatb = PerceptionResponseFlatB(perception_response_serialized)

    response_entity = response_flatb.getEntity()
    response_world_view = WorldView.deserialize_flatbuffer(response_flatb.getWorldView())

    # Verify the deserialized data matches the original
    assert response_entity.get_entity_id() == entity.get_entity_id()
    assert response_world_view.voxelGridView.width == world_view.voxelGridView.width

    assert len(response_world_view.entities) == len(world_view.entities)
    for entity_id in world_view.entities:
        original_entity = world_view.entities[entity_id]
        deserialized_entity = response_world_view.entities[entity_id]

        assert deserialized_entity.get_entity_id() == original_entity.get_entity_id()


def test_perception_response_flatbuffer_access():
    # Create an EntityInterface and a WorldView for PerceptionResponse
    entity = EntityInterface()
    entity.set_entity_id(2)
    world_view = WorldView()
    world_view.voxelGridView.initVoxelGridView(3, 3, 3, 0, 0, 0)

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
    world_view.voxelGridView.set_entity(1, 1, 1, 1)

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
    world_view.voxelGridView.set_entity(2, 2, 2, 2)

    # Create the perception response
    perception_response = PerceptionResponse(entity, world_view)
    serialized_data = perception_response.serialize_flatbuffer()

    flatb_accessor = PerceptionResponseFlatB(serialized_data)
    assert flatb_accessor.getWorldView().get_entity(1, 1, 1).get_position().x == entity1.get_position().x
