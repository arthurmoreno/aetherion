from __future__ import annotations

from types import SimpleNamespace
from typing import Any

import pytest
from conftest import make_event

from aetherion import WorldInterfaceMetadata
from aetherion.events.handlers import game_engine as game_engine_handlers
from aetherion.world.constants import WorldInstanceTypes


class _PhysicsSettings:
    def __init__(self):
        self.values: dict[str, float] = {}

    def set_gravity(self, value: float):
        self.values["gravity"] = value

    def set_friction(self, value: float):
        self.values["friction"] = value

    def set_metabolism_cost_to_apply_force(self, value: float):
        self.values["metabolism"] = value

    def set_evaporation_coefficient(self, value: float):
        self.values["evaporation"] = value

    def set_heat_to_water_evaporation(self, value: float):
        self.values["heat"] = value

    def set_water_minimum_units(self, value: float):
        self.values["water_minimum_units"] = value


class _ServerAdminConnection:
    def __init__(self):
        self.connected: list[dict[str, Any]] = []

    def connect(self, connection_type, world_host: str = "localhost", world_port: str = "8765", **_kwargs):
        self.connected.append(
            {
                "connection_type": connection_type,
                "world_host": world_host,
                "world_port": world_port,
            }
        )


def _build_engine() -> SimpleNamespace:
    world_manager = SimpleNamespace(worlds_metadata={}, pause_world=lambda _: None, play_world=lambda _: None)
    return SimpleNamespace(
        world_manager=world_manager,
        shared_state=SimpleNamespace(connected_world=None, connected_beast=None, desired_fps=30),
        audio_manager=SimpleNamespace(
            load_music=lambda *_: None,
            set_music_volume=lambda *_: None,
            fade_in_music=lambda **_: None,
            play_music=lambda **_: None,
            fade_out_music=lambda **_: None,
            stop_music=lambda: None,
            pause_music=lambda: None,
            resume_music=lambda: None,
        ),
        sound_effect_manager=SimpleNamespace(
            load_sound=lambda *_: None,
            play_sound=lambda *_1, **_2: None,
        ),
        player_connection=object(),
        world_interface=object(),
        server_admin_connection=None,
    )


def test_on_world_connected_missing_metadata_raises():
    engine = _build_engine()
    event = make_event({"world_name": "Missing", "world_key": "missing"})
    with pytest.raises(RuntimeError, match="not found"):
        game_engine_handlers.on_world_connected(engine, event)


def test_on_world_connected_sync_updates_shared_state(monkeypatch):
    engine = _build_engine()
    engine.world_manager.worlds_metadata["new_world"] = WorldInterfaceMetadata(
        key="new_world",
        name="New World",
        description="Test world metadata",
        type=WorldInstanceTypes.SYNCHRONOUS,
    )
    monkeypatch.setattr(game_engine_handlers, "PhysicsSettings", _PhysicsSettings)

    event = make_event({"world_name": "New World", "world_key": "new_world"})
    game_engine_handlers.on_world_connected(engine, event)
    assert engine.shared_state.connected_world == "new_world"


def test_on_world_connected_server_connects_admin(monkeypatch):
    engine = _build_engine()
    engine.world_manager.worlds_metadata["srv"] = WorldInterfaceMetadata(
        key="srv",
        name="Srv",
        description="Test server world metadata",
        type=WorldInstanceTypes.SERVER,
    )
    monkeypatch.setattr(game_engine_handlers, "ServerAdminConnection", _ServerAdminConnection)
    event = make_event({"world_name": "Srv", "world_key": "srv"})

    game_engine_handlers.on_world_connected(engine, event)

    assert isinstance(engine.server_admin_connection, _ServerAdminConnection)
    assert engine.server_admin_connection.connected == [
        {"connection_type": WorldInstanceTypes.SERVER, "world_host": "localhost", "world_port": "8765"}
    ]
    assert engine.shared_state.connected_world == "srv"


def test_on_world_disconnect_requested_clears_connections():
    paused: list[str] = []
    engine = _build_engine()
    engine.shared_state.connected_world = "new_world"
    engine.world_manager.pause_world = lambda world_key: paused.append(world_key)
    game_engine_handlers.on_world_disconnect_requested(engine, make_event({}))

    assert paused == ["new_world"]
    assert engine.shared_state.connected_world is None
    assert engine.shared_state.connected_beast is None
    assert engine.player_connection is None
    assert engine.world_interface is None


def test_on_world_stop_requested_requires_connected_world():
    engine = _build_engine()
    engine.shared_state.connected_world = None
    with pytest.raises(RuntimeError, match="No world is connected"):
        game_engine_handlers.on_world_stop_requested(engine, make_event({}))


def test_on_world_play_requested_calls_world_manager():
    played: list[str] = []
    engine = _build_engine()
    engine.shared_state.connected_world = "new_world"
    engine.world_manager.play_world = lambda world_key: played.append(world_key)

    game_engine_handlers.on_world_play_requested(engine, make_event({}))
    assert played == ["new_world"]


def test_audio_sound_effect_missing_data_returns_early():
    called = {"load": 0}
    engine = _build_engine()
    engine.sound_effect_manager.load_sound = lambda *_: called.__setitem__("load", called["load"] + 1)

    game_engine_handlers.on_audio_sound_effect_play(engine, make_event({"name": "x"}))
    assert called["load"] == 0


def test_audio_music_play_uses_fade_or_regular_paths(monkeypatch):
    engine = _build_engine()
    calls: list[str] = []
    monkeypatch.setattr(game_engine_handlers, "resolve_path", lambda p: p)
    engine.audio_manager.load_music = lambda *_: calls.append("load")
    engine.audio_manager.set_music_volume = lambda *_: calls.append("volume")
    engine.audio_manager.fade_in_music = lambda **_: calls.append("fade")
    engine.audio_manager.play_music = lambda **_: calls.append("play")

    game_engine_handlers.on_audio_music_play(
        engine, make_event({"file_path": "res://x.ogg", "loops": 2, "fade_ms": 500, "volume": 11})
    )
    game_engine_handlers.on_audio_music_play(
        engine, make_event({"file_path": "res://x.ogg", "loops": 2, "fade_ms": 0, "volume": 11})
    )

    assert "fade" in calls
    assert "play" in calls
