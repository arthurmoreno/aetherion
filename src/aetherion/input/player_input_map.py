from aetherion.events.action_event import InputEventActionType
from aetherion.input.event_action_handler import InputEventActionHandler
from aetherion.networking.connection import BeastConnection


def create_player_input_map(player_connection: BeastConnection) -> dict[InputEventActionType, InputEventActionHandler]:
    return {
        InputEventActionType.WALK_LEFT: InputEventActionHandler(handler=player_connection.walk_left, kwargs={}),
        InputEventActionType.WALK_RIGHT: InputEventActionHandler(handler=player_connection.walk_right, kwargs={}),
        InputEventActionType.WALK_UP: InputEventActionHandler(handler=player_connection.walk_up, kwargs={}),
        InputEventActionType.WALK_DOWN: InputEventActionHandler(handler=player_connection.walk_down, kwargs={}),
        InputEventActionType.WALK_LEFT_UP: InputEventActionHandler(handler=player_connection.walk_left_up, kwargs={}),
        InputEventActionType.WALK_LEFT_DOWN: InputEventActionHandler(
            handler=player_connection.walk_left_down, kwargs={}
        ),
        InputEventActionType.WALK_RIGHT_UP: InputEventActionHandler(handler=player_connection.walk_right_up, kwargs={}),
        InputEventActionType.WALK_RIGHT_DOWN: InputEventActionHandler(
            handler=player_connection.walk_right_down, kwargs={}
        ),
        InputEventActionType.WALK_LEFT_JUMP: InputEventActionHandler(
            handler=player_connection.walk_left_jump, kwargs={}
        ),
        InputEventActionType.WALK_RIGHT_JUMP: InputEventActionHandler(
            handler=player_connection.walk_right_jump, kwargs={}
        ),
        InputEventActionType.WALK_UP_JUMP: InputEventActionHandler(handler=player_connection.walk_up_jump, kwargs={}),
        InputEventActionType.WALK_DOWN_JUMP: InputEventActionHandler(
            handler=player_connection.walk_down_jump, kwargs={}
        ),
        InputEventActionType.JUMP: InputEventActionHandler(handler=player_connection.jump, kwargs={}),
        InputEventActionType.TAKE_ITEM: InputEventActionHandler(
            handler=player_connection.make_entity_take_item, kwargs={}
        ),
        InputEventActionType.USE_ITEM_0: InputEventActionHandler(
            handler=player_connection.make_entity_use_item,
            kwargs={"item_slot": 0},
            processor_kwargs={"shared_state"},
        ),
        InputEventActionType.USE_ITEM_1: InputEventActionHandler(
            handler=player_connection.make_entity_use_item,
            kwargs={"item_slot": 1},
            processor_kwargs={"shared_state"},
        ),
        InputEventActionType.USE_ITEM_2: InputEventActionHandler(
            handler=player_connection.make_entity_use_item,
            kwargs={"item_slot": 2},
            processor_kwargs={"shared_state"},
        ),
        InputEventActionType.USE_ITEM_3: InputEventActionHandler(
            handler=player_connection.make_entity_use_item,
            kwargs={"item_slot": 3},
            processor_kwargs={"shared_state"},
        ),
        InputEventActionType.USE_ITEM_4: InputEventActionHandler(
            handler=player_connection.make_entity_use_item,
            kwargs={"item_slot": 4},
            processor_kwargs={"shared_state"},
        ),
        InputEventActionType.USE_ITEM_5: InputEventActionHandler(
            handler=player_connection.make_entity_use_item,
            kwargs={"item_slot": 5},
            processor_kwargs={"shared_state"},
        ),
        InputEventActionType.USE_ITEM_6: InputEventActionHandler(
            handler=player_connection.make_entity_use_item,
            kwargs={"item_slot": 6},
            processor_kwargs={"shared_state"},
        ),
        InputEventActionType.USE_ITEM_7: InputEventActionHandler(
            handler=player_connection.make_entity_use_item,
            kwargs={"item_slot": 7},
            processor_kwargs={"shared_state"},
        ),
        InputEventActionType.USE_ITEM_8: InputEventActionHandler(
            handler=player_connection.make_entity_use_item,
            kwargs={"item_slot": 8},
            processor_kwargs={"shared_state"},
        ),
        InputEventActionType.USE_ITEM_9: InputEventActionHandler(
            handler=player_connection.make_entity_use_item,
            kwargs={"item_slot": 9},
            processor_kwargs={"shared_state"},
        ),
    }
