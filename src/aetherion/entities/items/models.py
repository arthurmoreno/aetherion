from typing import Any

from pydantic import BaseModel, ConfigDict, Field

from aetherion import VecStr
from aetherion.entities.base import Classification


class ItemConfiguration(BaseModel):
    model_config = ConfigDict(arbitrary_types_allowed=True)

    id: Classification
    in_game_textures: VecStr = Field(default_factory=VecStr)
    inventory_textures: VecStr = Field(default_factory=VecStr)
    default_values: dict[str, Any] = Field(default_factory=dict)
