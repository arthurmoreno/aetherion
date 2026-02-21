from __future__ import annotations

from typing import Callable, TypeAlias

from aetherion.audio.models import AudioManagerProtocol
from aetherion.engine.models import GameEngineProtocol
from aetherion.events import GameEvent, GameEventType
from aetherion.world.models import WorldManagerProtocol

GameEngineEventHandler: TypeAlias = Callable[[GameEngineProtocol, GameEvent[GameEventType]], None]
GameEngineEventHandlersMap: TypeAlias = dict[GameEventType, GameEngineEventHandler | None]

AudioManagerEventHandler: TypeAlias = Callable[[AudioManagerProtocol, GameEvent[GameEventType]], None]
AudioManagerEventHandlersMap: TypeAlias = dict[GameEventType, AudioManagerEventHandler | None]

WorldEventHandler: TypeAlias = Callable[[WorldManagerProtocol, GameEvent[GameEventType]], None]
WorldEventHandlersMap: TypeAlias = dict[GameEventType, WorldEventHandler | None]


__all__ = ["WorldEventHandler", "WorldEventHandlersMap"]
