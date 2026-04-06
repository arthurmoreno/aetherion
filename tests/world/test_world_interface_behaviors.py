from __future__ import annotations

from types import SimpleNamespace

import msgpack
import pytest
from conftest import FakeEntity

from aetherion import DirectionEnum
from aetherion.world.constants import WorldInstanceTypes
from aetherion.world.interface import WorldInterface


class _FakeWorld:
    def __init__(self):
        self.game_clock = SimpleNamespace(get_ticks=lambda: 0)
        self.move_calls = []
        self.take_calls = []
        self.use_calls = []
        self.debug_calls = []
        self.update_calls = 0

    def update(self):
        self.update_calls += 1

    def get_py_registry(self):
        return "fake_registry"

    def get_voxel_grid(self):
        return "fake_voxel_grid"

    def dispatch_move_entity_event_by_id(self, entity_id, directions):
        self.move_calls.append((entity_id, directions))

    def dispatch_take_item_event_by_id(self, entity_id, hovered_entity, selected_entity):
        self.take_calls.append((entity_id, hovered_entity, selected_entity))

    def dispatch_use_item_event_by_id(self, entity_id, item_slot, hovered_entity, selected_entity):
        self.use_calls.append((entity_id, item_slot, hovered_entity, selected_entity))

    def dispatch_set_entity_to_debug(self, entity_id):
        self.debug_calls.append(entity_id)


def test_walk_and_jump_dispatch_expected_direction_vectors():
    world = _FakeWorld()
    iface = WorldInterface(WorldInstanceTypes.SYNCHRONOUS, world)
    entity = FakeEntity(entity_id=12)

    iface.walk_left_entity(entity)
    iface.walk_up_entity(entity)
    iface.jump_entity(entity)
    iface.close()

    assert world.move_calls[0][1] == [DirectionEnum.LEFT]
    assert world.move_calls[1][1] == [DirectionEnum.UP]
    assert world.move_calls[2][1] == [DirectionEnum.UPWARD]


def test_make_entity_take_and_use_item_convert_none_to_minus_one():
    world = _FakeWorld()
    iface = WorldInterface(WorldInstanceTypes.SYNCHRONOUS, world)
    entity = FakeEntity(entity_id=9)

    iface.make_entity_take_item(entity, None, None)
    iface.make_entity_use_item(entity, 2, None, None)
    iface.close()

    assert world.take_calls == [(9, -1, -1)]
    assert world.use_calls == [(9, 2, -1, -1)]


def test_check_world_raises_when_world_is_none():
    world = _FakeWorld()
    iface = WorldInterface(WorldInstanceTypes.SYNCHRONOUS, world)
    iface.world = None

    with pytest.raises(RuntimeError, match="not initialized"):
        iface.check_world()
    iface.close()


def test_get_perception_responses_sync_stores_last_state(monkeypatch):
    world = _FakeWorld()
    iface = WorldInterface(WorldInstanceTypes.SYNCHRONOUS, world)
    monkeypatch.setattr(
        "aetherion.world.interface.create_perception_multithread",
        lambda *_: {1: [{"k": "v"}]},
    )

    encoded = iface.get_perception_responses({1: []}, {1: "player_1"})
    iface.close()

    assert iface.last_state_response == encoded
    assert msgpack.unpackb(encoded, raw=False, strict_map_key=False) == {1: [{"k": "v"}]}


def test_get_perception_responses_server_compresses(monkeypatch):
    world = _FakeWorld()
    iface = WorldInterface(WorldInstanceTypes.SERVER, world)
    monkeypatch.setattr(
        "aetherion.world.interface.create_perception_multithread",
        lambda *_: {1: [{"k": "v"}]},
    )
    monkeypatch.setattr("aetherion.world.interface.lz4.frame.compress", lambda b: b"compressed-" + b)

    encoded = iface.get_perception_responses({1: []}, {1: "player_1"})
    iface.close()

    assert encoded.startswith(b"compressed-")


def test_update_world_calls_world_update():
    world = _FakeWorld()
    iface = WorldInterface(WorldInstanceTypes.SYNCHRONOUS, world)
    iface.update_world()
    iface.close()
    assert world.update_calls == 1
