from lifesimcore import DirectionEnum, EntityInterface, HealthComponent, Position, Velocity


# Sample test data for your components
def test_entity_interface_serialization():
    # Create an EntityInterface instance with some test data
    entity = EntityInterface()
    entity.set_entity_id(1)
    # Add components

    position = Position()
    position.x = 10
    position.y = 20
    position.z = 30
    position.direction = DirectionEnum.DOWN
    entity.set_position(position)

    velocity = Velocity()
    velocity.vx = 0
    velocity.vy = 0
    velocity.vz = 0
    entity.set_velocity(velocity)

    health = HealthComponent()
    health.health_level = 100
    health.max_health = 120
    entity.set_health(health)

    # Serialize the entity
    serialized_data = entity.serialize()

    # Deserialize the entity back
    deserialized_entity = EntityInterface.deserialize(serialized_data)

    # Verify the deserialized entity matches the original
    assert deserialized_entity.get_entity_id() == entity.get_entity_id()
    assert deserialized_entity.get_position().x == entity.get_position().x
    assert deserialized_entity.get_velocity().vz == entity.get_velocity().vz
