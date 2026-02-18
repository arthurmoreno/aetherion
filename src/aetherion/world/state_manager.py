from aetherion import EntityEnum, EntityInterface, World
from aetherion.entities.beasts import BeastEntity, BeastEnum
from aetherion.logger import logger
from aetherion.world.exceptions import CreatePerceptionResponseException


def look_for_player(world, player) -> BeastEntity:
    entity_id = player.entity_id
    player = BeastEntity.from_entity_interface(world.get_entity_by_id(entity_id))
    player.entity_id = entity_id

    return player


def create_perceptions_linear(entity_id, optional_queries, world: World, player: BeastEntity):
    response = {}

    squirrels = world.get_entities_by_type(EntityEnum.BEAST.value, BeastEnum.SQUIRREL.value)
    for entity_id, squirrel in squirrels.items():
        entity_interface: EntityInterface = world.get_entity_by_id(entity_id)
        if entity_interface.get_entity_type().main_type != 2:
            import ipdb

            ipdb.set_trace()
            print("stop")
        perception_response = world.create_perception_response(entity_id, [])
        response_key = f"beast_brain_{entity_id}"
        response[response_key] = perception_response

    connection_name = "player"
    _optional_queries = optional_queries.get(connection_name, [])
    perception_response = world.create_perception_response(player.entity_id, _optional_queries)

    response[connection_name] = perception_response

    return response


def create_perception_multithread(world: World, entities_ids_with_queries, entities_ids_connection_names):
    # Create queries for all squirrels as empty lists
    #    and set the player's queries from optional_queries.
    #    Build a map from entity ID -> connection name
    # Call into C++ to create all perception responses at once
    try:
        perception_responses = world.create_perception_responses(entities_ids_with_queries)
    except Exception as e:
        message: str = f"Error creating perception responses: {e}"
        return CreatePerceptionResponseException(message)

    # Build the final response in one pass
    response = {}
    for entity_id, resp_value in perception_responses.items():
        if len(resp_value) == 0:
            logger.error(f"Empty perception response for entity {entity_id}")
        entity_connection_name = None
        for key in entities_ids_connection_names.keys():
            if entity_id == key:
                entity_connection_name = entities_ids_connection_names[key]

        if entity_connection_name:
            response[entity_connection_name] = resp_value
        else:
            logger.error(f"Entity {entity_id} has no connection name.")
            # if "player" in response_key:
            #     print("Player perception response created.")
            # response[response_key] = resp_value

    return response


def update_world(world_interface, player: BeastEntity, optional_queries):
    """
    To update the world and get creatures perceptions.
    """
    # if not world_interface.world:
    #     sleep(1)
    #     return {}

    world_interface.world.update()
    # world_interface.player = look_for_player(world_interface.world, player)

    # return create_perception_multithread(optional_queries, world_interface.world, player)
