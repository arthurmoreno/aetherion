from aetherion.events.action_event import InputEventActionType
from aetherion.input.player_input_map import create_player_input_map


class DummyPlayerConnection:
    def walk_left(self):
        return None

    def walk_right(self):
        return None

    def walk_up(self):
        return None

    def walk_down(self):
        return None

    def walk_left_up(self):
        return None

    def walk_left_down(self):
        return None

    def walk_right_up(self):
        return None

    def walk_right_down(self):
        return None

    def walk_left_jump(self):
        return None

    def walk_right_jump(self):
        return None

    def walk_up_jump(self):
        return None

    def walk_down_jump(self):
        return None

    def jump(self):
        return None

    def make_entity_take_item(self):
        return None

    def make_entity_use_item(self, item_slot: int):
        return item_slot


def test_create_player_input_map_contains_core_movement_actions():
    input_map = create_player_input_map(DummyPlayerConnection())

    required_actions = {
        InputEventActionType.WALK_LEFT,
        InputEventActionType.WALK_RIGHT,
        InputEventActionType.WALK_UP,
        InputEventActionType.WALK_DOWN,
        InputEventActionType.WALK_LEFT_UP,
        InputEventActionType.WALK_LEFT_DOWN,
        InputEventActionType.WALK_RIGHT_UP,
        InputEventActionType.WALK_RIGHT_DOWN,
        InputEventActionType.WALK_LEFT_JUMP,
        InputEventActionType.WALK_RIGHT_JUMP,
        InputEventActionType.WALK_UP_JUMP,
        InputEventActionType.WALK_DOWN_JUMP,
        InputEventActionType.JUMP,
        InputEventActionType.TAKE_ITEM,
    }

    assert required_actions.issubset(set(input_map.keys()))


def test_create_player_input_map_contains_use_item_actions_0_to_9():
    input_map = create_player_input_map(DummyPlayerConnection())

    for slot in range(10):
        action = InputEventActionType[f"USE_ITEM_{slot}"]
        assert action in input_map
        assert input_map[action].kwargs["item_slot"] == slot
        assert input_map[action].processor_kwargs == {"shared_state"}
