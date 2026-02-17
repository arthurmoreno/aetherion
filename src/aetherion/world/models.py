from __future__ import annotations

from pydantic import BaseModel, ConfigDict

from aetherion import EntityInterface


class AIEntityMetadataResponse(BaseModel):
    entity_interface: EntityInterface | None
    brain_created: bool
    model_config = ConfigDict(arbitrary_types_allowed=True)


class AIMetadataResponse(BaseModel):
    model_config = ConfigDict(arbitrary_types_allowed=True)

    # Mapping of entity ID to AI metadata
    ai_metadata: dict[int, AIEntityMetadataResponse] = {}
