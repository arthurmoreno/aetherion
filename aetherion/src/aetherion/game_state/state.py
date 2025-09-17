from typing import Any

from pydantic import BaseModel, ConfigDict

from aetherion import (
    EntityInterface,
    GameClock,
    PerceptionResponseFlatB,
)
from aetherion.game_state.connections import BeastConnectionMetadata
from aetherion.game_state.world_interface import WorldInterfaceMetadata


class SharedState(BaseModel):
    model_config = ConfigDict(arbitrary_types_allowed=True)

    ready_to_render_imgui: bool = False
    response: PerceptionResponseFlatB | None = None
    mouse_state: dict[str, int | bool] = {"x": 0, "y": 0, "left": False, "right": False}
    fastforward_count: int = 0
    selected_entity_just_set: bool = False
    selected_entity: int | None = None
    hovered_entity: int | None = None
    selected_entity_interface: EntityInterface | None = None
    hovered_entity_interface: EntityInterface | None = None

    needs_render_imgui: bool = False
    commands: list[str] = []
    game_clock: GameClock | None = None  # Placeholder for game clock, should be set to an actual clock object
    world_ticks: int = 0

    water_camera_stats: bool = False
    terrain_gradient_camera_stats: bool = False

    fps: float = 0.0
    console_logs: list[str] = []

    statistics: dict[str, Any] = {}

    all_world_metadata: dict[str, WorldInterfaceMetadata] = {}
    connected_world: str | None = None
    all_beast_connection_metadata: dict[str, BeastConnectionMetadata] = {}
    connected_beast: str | None = None
