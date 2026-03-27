from __future__ import annotations

from types import SimpleNamespace

from conftest import FakeEventBus, FakeMetadata, FakeWorldBridge

from aetherion.world.manager import WorldManager


def _make_manager() -> WorldManager:
    return WorldManager(event_bus=FakeEventBus(), default_event_handlers={})


def test_validate_target_key_none_without_current_returns_none():
    manager = _make_manager()
    assert manager._validate_target_key(None) is None


def test_pause_and_play_world_update_status():
    manager = _make_manager()
    manager.current_key = "new_world"
    manager.worlds_metadata["new_world"] = FakeMetadata(key="new_world", name="New World", status="running")

    manager._pause_world()
    assert manager.worlds_metadata["new_world"].status == "paused"

    manager._play_world()
    assert manager.worlds_metadata["new_world"].status == "running"


def test_step_simulation_runs_n_ticks_and_restores_status():
    manager = _make_manager()
    bridge = FakeWorldBridge()
    manager.current_key = "new_world"
    manager.worlds["new_world"] = bridge
    manager.worlds_metadata["new_world"] = FakeMetadata(key="new_world", name="New World", status="paused")

    manager._step_simulation(steps=3)
    assert bridge.update_calls == 3
    assert manager.worlds_metadata["new_world"].status == "paused"


def test_update_only_runs_when_status_running():
    manager = _make_manager()
    bridge = FakeWorldBridge()
    manager.current = bridge
    manager.current_key = "new_world"
    manager.current_metadata = FakeMetadata(key="new_world", name="New World", status="paused")

    manager.update()
    assert bridge.update_calls == 0

    manager.current_metadata.status = "running"
    manager.update()
    assert bridge.update_calls == 1

    manager.current_metadata.status = "stepping"
    manager.update()
    assert bridge.update_calls == 1


def test_update_without_metadata_keeps_backward_compatibility():
    manager = _make_manager()
    bridge = FakeWorldBridge()
    manager.current = bridge
    manager.current_metadata = None

    manager.update()
    assert bridge.update_calls == 1


def test_register_and_get_world_by_normalized_name():
    manager = _make_manager()
    world_interface = SimpleNamespace()
    # Bypass strict type check while still testing key path behavior.
    manager.worlds["new_world"] = world_interface

    fetched = manager.get_world("New World")
    assert fetched is world_interface
