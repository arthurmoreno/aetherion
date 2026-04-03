"""Networking-layer types shared across transports (e.g. websocket)."""

from __future__ import annotations

from collections.abc import Awaitable, Callable
from typing import Any

from websockets.asyncio.server import ServerConnection

WebsocketActionHandler = Callable[[dict[str, Any], ServerConnection], Awaitable[None]]
