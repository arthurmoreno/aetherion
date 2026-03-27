from __future__ import annotations

from dataclasses import dataclass
from types import SimpleNamespace
from typing import Any

import pytest
from conftest import FakeEventBus, make_event

from aetherion import GameEventType
from aetherion.events.handlers import world_manager as world_manager_handlers
from aetherion.world.constants import WorldInstanceTypes


@dataclass
class _Metadata:
    key: str
    name: str
    description: str = ""
    type: Any = WorldInstanceTypes.SYNCHRONOUS
    status: str = "ready"
    host: str | None = None
    port: str | None = None


class _FakeThread:
    def __init__(self, target=None, args=(), kwargs=None):
        self.target = target
        self.args = args
        self.kwargs = kwargs or {}
        self.started = False

    def start(self):
        self.started = True


def _make_world_manager() -> SimpleNamespace:
    return SimpleNamespace(
        worlds_metadata={},
        worlds={},
        current=None,
        current_key=None,
        current_metadata=None,
        event_bus=FakeEventBus(),
        world_thread=None,
        start_ai_manager_calls=[],
        load_world=lambda **_: None,
    )


def test_on_world_create_requested_sync_starts_loader_and_emits_success(monkeypatch):
    manager = _make_world_manager()
    monkeypatch.setattr(world_manager_handlers, "WorldInterfaceMetadata", _Metadata)
    monkeypatch.setattr(world_manager_handlers, "Thread", _FakeThread)

    event = make_event(
        {
            "world_name": "New World",
            "world_factory_name": "dungeon",
            "world_description": "desc",
            "world_config": {"type": WorldInstanceTypes.SYNCHRONOUS},
        }
    )

    world_manager_handlers.on_world_create_requested(manager, event)

    assert "new_world" in manager.worlds_metadata
    assert manager.world_thread is not None
    assert manager.world_thread.started is True
    assert manager.event_bus.emitted[-1]["event_type"] == GameEventType.WORLD_CREATED
    assert manager.event_bus.emitted[-1]["data"]["success"] is True


def test_on_world_create_requested_server_sets_ready_and_emits_success(monkeypatch):
    manager = _make_world_manager()
    monkeypatch.setattr(world_manager_handlers, "WorldInterfaceMetadata", _Metadata)

    event = make_event(
        {
            "world_name": "Server World",
            "world_description": "desc",
            "world_host": "127.0.0.1",
            "world_port": "9999",
            "world_config": {"type": WorldInstanceTypes.SERVER},
        }
    )

    world_manager_handlers.on_world_create_requested(manager, event)

    md = manager.worlds_metadata["server_world"]
    assert md.status == "ready"
    assert md.type == WorldInstanceTypes.SERVER
    assert md.host == "127.0.0.1"
    assert md.port == "9999"
    assert manager.event_bus.emitted[-1]["data"]["success"] is True


def test_on_world_create_requested_invalid_type_emits_failure(monkeypatch):
    manager = _make_world_manager()
    monkeypatch.setattr(world_manager_handlers, "WorldInterfaceMetadata", _Metadata)

    event = make_event({"world_name": "Bad", "world_config": {"type": 999}})
    world_manager_handlers.on_world_create_requested(manager, event)

    assert manager.event_bus.emitted[-1]["event_type"] == GameEventType.WORLD_CREATED
    assert manager.event_bus.emitted[-1]["data"]["success"] is False
    assert "Unsupported world instance type" in manager.event_bus.emitted[-1]["data"]["error"]


def test_on_world_create_requested_missing_type_emits_failure(monkeypatch):
    manager = _make_world_manager()
    monkeypatch.setattr(world_manager_handlers, "WorldInterfaceMetadata", _Metadata)

    event = make_event({"world_name": "Bad", "world_config": {}})
    world_manager_handlers.on_world_create_requested(manager, event)

    assert manager.event_bus.emitted[-1]["data"]["success"] is False
    assert "type" in manager.event_bus.emitted[-1]["data"]["error"]


def test_on_world_connect_requested_missing_world_raises():
    manager = _make_world_manager()
    event = make_event({"world_name": "Ghost", "world_key": "ghost"})

    with pytest.raises(ValueError, match="not registered"):
        world_manager_handlers.on_world_connect_requested(manager, event)


def test_on_world_connect_requested_non_ready_world_raises():
    manager = _make_world_manager()
    manager.worlds_metadata["new_world"] = _Metadata(
        key="new_world", name="New World", status="creating", type=WorldInstanceTypes.SYNCHRONOUS
    )
    event = make_event({"world_name": "New World", "world_key": "new_world"})

    with pytest.raises(RuntimeError, match="not ready for connection"):
        world_manager_handlers.on_world_connect_requested(manager, event)


def test_on_world_connect_requested_virtual_server_emits_connected():
    manager = _make_world_manager()
    manager.worlds_metadata["srv"] = _Metadata(key="srv", name="Srv", status="ready", type=WorldInstanceTypes.SERVER)
    event = make_event({"world_name": "Srv", "world_key": "srv"})

    world_manager_handlers.on_world_connect_requested(manager, event)

    assert manager.event_bus.emitted[-1]["event_type"] == GameEventType.WORLD_CONNECTED
    assert manager.event_bus.emitted[-1]["data"]["success"] is True


def test_on_world_connect_requested_sync_world_starts_ai_and_marks_running():
    manager = _make_world_manager()
    manager.worlds_metadata["new_world"] = _Metadata(
        key="new_world",
        name="New World",
        status="ready",
        type=WorldInstanceTypes.SYNCHRONOUS,
    )
    manager.worlds["new_world"] = object()

    def _start_ai(world_instance, connection_type):
        manager.start_ai_manager_calls.append((world_instance, connection_type))

    manager.start_ai_manager = _start_ai
    event = make_event({"world_name": "New World", "world_key": "new_world"})

    world_manager_handlers.on_world_connect_requested(manager, event)

    assert len(manager.start_ai_manager_calls) == 1
    assert manager.worlds_metadata["new_world"].status == "running"
    assert manager.event_bus.emitted[-1]["event_type"] == GameEventType.WORLD_CONNECTED
