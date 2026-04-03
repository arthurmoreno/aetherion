from __future__ import annotations

import asyncio
from unittest.mock import AsyncMock, MagicMock

import msgpack

from aetherion.networking.websocket_server import WebSocketGameServer


class _StubEntity:
    pass


class _StubWorld:
    def __init__(self) -> None:
        self.entity = _StubEntity()

    def get_entity_by_id(self, entity_id: int) -> _StubEntity:
        _ = entity_id
        return self.entity


class _StubWorldInterface:
    def __init__(self) -> None:
        self.world = _StubWorld()
        self.walk_left_entity = MagicMock()
        self.walk_up_entity = MagicMock()
        self.walk_right_entity = MagicMock()
        self.walk_down_entity = MagicMock()
        self.jump_entity = MagicMock()
        self.make_entity_eat = MagicMock()
        self.set_entity_action_map = MagicMock()
        self.process_nn_actions = MagicMock()


def test_process_action_default_walk_left_calls_world_interface() -> None:
    wi = _StubWorldInterface()
    server = WebSocketGameServer(world_interface=wi)
    ws = AsyncMock()

    async def _run() -> None:
        await server.process_action({"action_name": "walk_left_entity", "entity_id": 1}, ws)

    asyncio.run(_run())

    wi.walk_left_entity.assert_called_once_with(wi.world.entity)
    ws.send.assert_not_called()


def test_process_action_user_override_replaces_default() -> None:
    wi = _StubWorldInterface()
    custom_called = False

    async def custom_walk(_msg: dict, _ws: AsyncMock) -> None:
        nonlocal custom_called
        custom_called = True

    server = WebSocketGameServer(world_interface=wi, action_handlers={"walk_left_entity": custom_walk})
    ws = AsyncMock()

    async def _run() -> None:
        await server.process_action({"action_name": "walk_left_entity", "entity_id": 1}, ws)

    asyncio.run(_run())

    assert custom_called is True
    wi.walk_left_entity.assert_not_called()


def test_process_action_unknown_action_sends_error() -> None:
    server = WebSocketGameServer(world_interface=_StubWorldInterface())
    ws = AsyncMock()

    async def _run() -> None:
        await server.process_action({"action_name": "not_a_real_action"}, ws)

    asyncio.run(_run())

    ws.send.assert_called_once()
    payload = msgpack.unpackb(ws.send.call_args[0][0], raw=False)
    assert payload["type"] == "error"
    assert "Unknown action" in payload["message"]


def test_process_action_missing_action_name_sends_error() -> None:
    server = WebSocketGameServer(world_interface=_StubWorldInterface())
    ws = AsyncMock()

    async def _run() -> None:
        await server.process_action({}, ws)

    asyncio.run(_run())

    ws.send.assert_called_once()
    payload = msgpack.unpackb(ws.send.call_args[0][0], raw=False)
    assert payload["type"] == "error"
    assert "action_name" in payload["message"].lower()
