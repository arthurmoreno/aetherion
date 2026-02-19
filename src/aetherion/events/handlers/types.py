from __future__ import annotations

from typing import Callable, TypeAlias

from aetherion.events import GameEvent, GameEventType
from aetherion.world.models import WorldManagerProtocol


WorldEventHandler: TypeAlias = Callable[[WorldManagerProtocol, GameEvent[GameEventType]], None]
WorldEventHandlersMap: TypeAlias = dict[GameEventType, WorldEventHandler | None]



__all__ = ["WorldEventHandler", "WorldEventHandlersMap"]