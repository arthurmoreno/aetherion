from __future__ import annotations

from abc import ABC, abstractmethod
from typing import Any, Protocol

from pydantic import BaseModel, ConfigDict

from aetherion import EntityInterface, World


class WorldFactory(ABC):
    """Interface for anything that can build a World instance."""

    def __init__(self, width: int = 3, height: int = 3, depth: int = 3):
        self.width: int = width
        self.height: int = height
        self.depth: int = depth

    def create_world(self) -> World:
        """Creates a new world instance."""

        world: World = World(self.width, self.height, self.depth)
        world.width = self.width
        world.height = self.height
        world.depth = self.depth

        return world

    @abstractmethod
    def generate_world(self) -> World:
        """Constructs, populates, and returns a fully-initialized World."""
        pass


class WorldManagerProtocol(Protocol):
    event_bus: Any
    worlds: dict[str, Any]
    worlds_metadata: dict[str, Any]
    world_snapshots: dict[str, list[str]]
    recorder_manager_: Any
    current_key: str | None


class AIEntityMetadataResponse(BaseModel):
    entity_interface: EntityInterface | None
    brain_created: bool
    model_config = ConfigDict(arbitrary_types_allowed=True)


class AIMetadataResponse(BaseModel):
    model_config = ConfigDict(arbitrary_types_allowed=True)

    # Mapping of entity ID to AI metadata
    ai_metadata: dict[int, AIEntityMetadataResponse] = {}
