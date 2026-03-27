from __future__ import annotations

from types import SimpleNamespace

import sdl2

from aetherion.events.action_event import InputEventActionType
from aetherion.input.user_input_controller import UserInputController


class _FakeBroker:
    def __init__(self) -> None:
        self.published: list[tuple[str, object]] = []
        self.consumers: list[object] = []

    def register_consumer(self, consumer) -> None:
        self.consumers.append(consumer)

    def unregister_consumer(self, consumer) -> None:
        if consumer in self.consumers:
            self.consumers.remove(consumer)

    def publish_event(self, topic: str, event: object) -> None:
        self.published.append((topic, event))


class _FakeEventBus:
    def __init__(self) -> None:
        self.emitted: list[dict[str, object]] = []

    def emit(self, event_type, data=None, source=None):
        self.emitted.append({"event_type": event_type, "data": data or {}, "source": source})
        return None


def _make_key_event(event_type: int, key: int) -> SimpleNamespace:
    return SimpleNamespace(type=event_type, key=SimpleNamespace(keysym=SimpleNamespace(sym=key)))


def _make_mouse_event(event_type: int, button: int | None = None) -> SimpleNamespace:
    return SimpleNamespace(
        type=event_type,
        button=SimpleNamespace(button=button),
        motion=SimpleNamespace(x=33, y=44),
    )


def test_check_key_state_publishes_jump_and_take_item():
    broker = _FakeBroker()
    controller = UserInputController(broker, _FakeEventBus())

    controller.check_key_state(_make_key_event(sdl2.SDL_KEYDOWN, sdl2.SDLK_SPACE))
    controller.check_key_state(_make_key_event(sdl2.SDL_KEYDOWN, sdl2.SDLK_f))

    events = [evt.event_type for _, evt in broker.published]
    assert InputEventActionType.JUMP in events
    assert InputEventActionType.TAKE_ITEM in events


def test_process_key_state_emits_last_pressed_direction_single_axis():
    broker = _FakeBroker()
    controller = UserInputController(broker, _FakeEventBus())
    controller.last_key_pressed[sdl2.SDLK_LEFT] = 12.0
    controller.last_key_pressed[sdl2.SDLK_UP] = 0.0

    controller.process_key_state({"allowMultiDirection": False})

    topic, event = broker.published[-1]
    assert topic == "input_action_queue"
    assert event.event_type == InputEventActionType.WALK_LEFT


def test_check_mouse_state_tracks_motion_and_clicks():
    broker = _FakeBroker()
    event_bus = _FakeEventBus()
    controller = UserInputController(broker, event_bus)

    controller.check_mouse_state(_make_mouse_event(sdl2.SDL_MOUSEMOTION))
    controller.check_mouse_state(_make_mouse_event(sdl2.SDL_MOUSEBUTTONDOWN, sdl2.SDL_BUTTON_LEFT))
    controller.check_mouse_state(_make_mouse_event(sdl2.SDL_MOUSEBUTTONUP, sdl2.SDL_BUTTON_RIGHT))

    assert controller.get_mouse_state()["x"] == 33
    assert controller.get_mouse_state()["y"] == 44
    published_types = [evt.event_type for _, evt in broker.published]
    assert InputEventActionType.MOUSE_LEFT_BUTTON_DOWN in published_types
    assert InputEventActionType.MOUSE_RIGHT_BUTTON_UP in published_types
    assert len(event_bus.emitted) == 1


def test_set_steps_to_advance_resets_counter():
    controller = UserInputController(_FakeBroker(), _FakeEventBus())
    controller.steps_to_advance = 55
    controller.set_steps_to_advance()
    assert controller.steps_to_advance == 0
