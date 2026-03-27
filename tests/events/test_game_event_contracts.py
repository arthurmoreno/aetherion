from aetherion.events import GameEventType
from aetherion.world.constants import WorldInstanceTypes


def test_required_game_event_types_exist_for_minimal_loop():
    required_events = {
        "CHANGE_DESIRED_FPS",
        "WORLD_CREATE_REQUESTED",
        "WORLD_CONNECTED",
        "CREATE_BEAST_REQUESTED",
        "BEAST_CONNECT_REQUESTED",
        "WORLD_DISCONNECT_REQUESTED",
        "WORLD_PLAY_REQUESTED",
        "WORLD_STOP_REQUESTED",
    }

    available_events = {event.name for event in GameEventType}
    assert required_events.issubset(available_events)


def test_required_world_instance_type_exists_for_sync_world():
    assert WorldInstanceTypes.SYNCHRONOUS.value == 1
