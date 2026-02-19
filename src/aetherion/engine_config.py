from typing import Any, Callable

from pydantic import BaseModel

from aetherion.events.action_event import InputEventActionType
from aetherion.networking.ai_manager import AIProcessManager
from aetherion.networking.connection import BeastConnection
from aetherion.world.constants import WorldInstanceTypes
from aetherion.events.handlers.types import WorldEventHandlersMap


class EngineConfig(BaseModel):
    """Encapsulate startup configuration for the game engine."""

    screen_width: int = 1280
    screen_height: int = 720
    fps: int = 60

    user_input_controller_class: type[Any] | None = None
    player_input_map_factory: Callable[[BeastConnection], dict[InputEventActionType, Any]] | None = None
    ai_manager_factory: Callable[[Any, WorldInstanceTypes], AIProcessManager] | None = None
    worldmanager_event_handlers: WorldEventHandlersMap | None = None
