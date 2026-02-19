from __future__ import annotations

from pydantic import BaseModel, ConfigDict

from aetherion import EntityInterface

from typing import Any, Protocol

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
