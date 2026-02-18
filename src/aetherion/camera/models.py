from __future__ import annotations

from typing import Any

from pydantic import BaseModel


class CameraModel(BaseModel):
    render_queue: Any
    terrain_group_0: Any
    terrain_group_1: Any
    terrain_group_2: Any
    gui_group: Any
    effect_group: Any
    entities_group_0: Any
    entities_group_1: Any
    entities_group_2: Any
    light_group: Any
    light_intensities: list[float]
    views: dict[str, Any]
    LAYERS_TO_DRAW: int
    settings: Any | None


class CameraSettings(BaseModel):
    blocks_in_screen: dict[str, int]
    player_block_position: dict[str, int]
    layers_to_draw: int
    layers_bellow_player: int
    sprite_size: int
    sprite_scale: int
    tile_size_on_screen: int
    right_offset: int
    up_offset: int
    game_screen_width: int
    game_screen_height: int
    camera_screen_width_adjust_offset: int
    camera_screen_height_adjust_offset: int
    empty_tile_debugging: bool
    camera_iterate_right_to_left: bool
    camera_iterate_bottom_to_top: bool
