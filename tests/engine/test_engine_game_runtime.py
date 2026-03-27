from __future__ import annotations

from types import SimpleNamespace

import pytest

from aetherion.engine.game import GameEngine
from aetherion.engine_config import EngineConfig
from aetherion.events.action_event import InputEventActionType
from aetherion.world.constants import WorldInstanceTypes


class _FakeController:
    def __init__(self, _pubsub, _event_bus) -> None:
        self.started = False

    def start(self):
        self.started = True


def _build_engine(monkeypatch) -> GameEngine:
    monkeypatch.setattr("aetherion.engine.game.init_sdl2", lambda: None)
    monkeypatch.setattr(
        GameEngine, "init_window", lambda self: setattr(self, "game_window", SimpleNamespace(window=object()))
    )
    monkeypatch.setattr(
        GameEngine,
        "set_window_ptr",
        lambda self: setattr(self, "window_ptr", 1234),
    )
    monkeypatch.setattr(
        GameEngine, "init_renderer", lambda self: setattr(self, "renderer", SimpleNamespace(renderer_ptr=99))
    )
    monkeypatch.setattr(
        GameEngine,
        "init_scheduler",
        lambda self: setattr(self, "scheduler", SimpleNamespace(execute_scheduled_funcs=lambda state: state)),
    )

    config = EngineConfig(
        user_input_controller_class=_FakeController,
        player_input_map_factory=lambda _conn: {},
    )
    return GameEngine(config)


def test_game_engine_requires_input_controller_class():
    config = EngineConfig(user_input_controller_class=None, player_input_map_factory=lambda _conn: {})
    with pytest.raises(ValueError, match="user_input_controller_class"):
        GameEngine(config)


def test_game_engine_requires_input_map_factory():
    config = EngineConfig(user_input_controller_class=_FakeController, player_input_map_factory=None)
    with pytest.raises(ValueError, match="player_input_map_factory"):
        GameEngine(config)


def test_set_input_action_processor_skips_when_no_player_connection(monkeypatch):
    engine = _build_engine(monkeypatch)
    engine.player_connection = None

    engine.set_input_action_processor()

    assert engine.input_action_processor is None


def test_set_input_action_processor_uses_factory_when_connection_set(monkeypatch):
    created = {"processor": None}
    engine = _build_engine(monkeypatch)
    engine.player_connection = SimpleNamespace(name="player-1")
    engine.player_input_map_factory = lambda _conn: {InputEventActionType.JUMP: object()}

    class _FakeProcessor:
        def __init__(self, broker, command_specs):
            created["processor"] = (broker, command_specs)

        def process_inputs(self, _shared_state):
            return None

    monkeypatch.setattr("aetherion.engine.game.InputActionProcessor", _FakeProcessor)
    engine.set_input_action_processor()

    assert created["processor"] is not None
    assert InputEventActionType.JUMP in created["processor"][1]


def test_process_input_queue_is_guarded(monkeypatch):
    engine = _build_engine(monkeypatch)
    shared_state = SimpleNamespace()
    engine.input_action_processor = None
    engine.process_input_queue(shared_state)  # no-op

    calls = {"count": 0}
    engine.input_action_processor = SimpleNamespace(process_inputs=lambda _state: calls.__setitem__("count", 1))
    engine.process_input_queue(shared_state)
    assert calls["count"] == 1


def test_should_poll_world_only_for_non_streaming_server_connection(monkeypatch):
    engine = _build_engine(monkeypatch)
    engine.player_connection = None
    assert engine._should_poll_world() is False

    engine.player_connection = SimpleNamespace(server_online=True)
    assert engine._should_poll_world() is False

    from aetherion.networking.connection import ServerBeastConnection

    server_conn = object.__new__(ServerBeastConnection)
    server_conn._server_online = True
    server_conn.streaming_enabled = False
    engine.player_connection = server_conn
    assert engine._should_poll_world() is True

    server_conn.streaming_enabled = True
    assert engine._should_poll_world() is False


def test_check_if_imgui_wants_capture_keyboard(monkeypatch):
    engine = _build_engine(monkeypatch)
    engine.world_type = WorldInstanceTypes.SYNCHRONOUS
    monkeypatch.setattr("aetherion.engine.game.aetherion.wants_capture_keyboard", lambda: True)
    assert engine.check_if_imgui_wants_capture_keyboard(True) is True
    assert engine.check_if_imgui_wants_capture_keyboard(False) is False
