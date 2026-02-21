from __future__ import annotations

from typing import Any, Dict, Optional

import aetherion
from aetherion import EntityInterface, RenderQueue, TextureQuadrant, WorldView
from aetherion.camera.models import CameraSettings
from aetherion.camera.mouse import get_and_draw_selected_entity

# from camera.occlusion_culling import is_occluding_entity_perspective
from aetherion.entities.base import Classification
from aetherion.world.constants import TERRAIN_VARIATION_TYPE_MAP


def should_draw_terrain(terrain: EntityInterface, camera_settings: CameraSettings) -> bool:
    return (
        terrain.get_entity_type().sub_type0 == aetherion.TerrainEnum_EMPTY and camera_settings.empty_tile_debugging
    ) or terrain.get_entity_type().sub_type0 != aetherion.TerrainEnum_EMPTY


def emtpy_water_terrain(terrain: EntityInterface) -> bool:
    return (
        terrain.get_entity_type().main_type == aetherion.EntityEnum_TERRAIN
        and terrain.get_entity_type().sub_type0 == aetherion.TerrainEnum_WATER
        and terrain.get_matter_container().water_matter == 0
        and terrain.get_matter_container().water_vapor == 0
    )


def set_grass_terrain_view_sprite(terrain: EntityInterface, view_object: Any) -> None:
    view_object.set_terrain_variation_sprite(
        TERRAIN_VARIATION_TYPE_MAP.get(terrain.get_entity_type().sub_type1, "full")
    )


def set_water_terrain_view_sprite(terrain: EntityInterface, view_object: Any) -> None:
    if terrain.get_matter_container().water_matter > 0 and terrain.get_matter_container().water_matter <= 4:
        view_object.set_terrain_variation_sprite("level-1")
    elif terrain.get_matter_container().water_matter > 4 and terrain.get_matter_container().water_matter <= 12:
        view_object.set_terrain_variation_sprite("level-2")
    elif terrain.get_matter_container().water_vapor > 0 and terrain.get_matter_container().water_vapor <= 4:
        view_object.set_terrain_variation_sprite("vapor-level-1")
    elif terrain.get_matter_container().water_vapor > 4 and terrain.get_matter_container().water_vapor <= 12:
        view_object.set_terrain_variation_sprite("vapor-level-2")
    elif terrain.get_matter_container().water_vapor > 12:
        view_object.set_terrain_variation_sprite("cloud-1")
    else:
        view_object.set_terrain_variation_sprite("full")


def is_occluding_some_entity(world_view: WorldView, occluding_entity: EntityInterface) -> bool:
    """
    Check if the occluding entity is blocking any other entity in the world.

    Args:
        world_view: The world view containing all entities
        occluding_entity: The entity to check for occlusion

    Returns:
        True if the entity is occluding some other entity, False otherwise
    """
    # Get the position of the occluding entity
    occluding_entity_pos = occluding_entity.get_position()
    occluding_entity_x = occluding_entity_pos.x
    occluding_entity_y = occluding_entity_pos.y
    occluding_entity_z = occluding_entity_pos.z

    # Get the occluding entity type
    occluding_entity_type = occluding_entity.get_entity_type()

    # Check if the occluding entity is of type TERRAIN
    if occluding_entity_type.main_type == aetherion.EntityEnum_TERRAIN:
        # Check the 3 critical voxel positions for terrain occlusion at same Z level:
        # 1. Direct overlap position
        if world_view.check_if_entity_exist(occluding_entity_x, occluding_entity_y, occluding_entity_z):
            return True

        # 2. Southeast diagonal offset positions
        if (
            world_view.check_if_entity_exist(occluding_entity_x - 1, occluding_entity_y - 1, occluding_entity_z)
            or world_view.check_if_entity_exist(occluding_entity_x - 1, occluding_entity_y, occluding_entity_z)
            or world_view.check_if_entity_exist(occluding_entity_x - 1, occluding_entity_y + 1, occluding_entity_z)
        ):
            return True

    # Check for entities at lower Z levels that could be occluded
    # 1. Direct overlap at lower Z
    if world_view.check_if_entity_exist(occluding_entity_x, occluding_entity_y, occluding_entity_z - 1):
        return True

    # 2. Diagonal overlap at lower Z
    if world_view.check_if_entity_exist(occluding_entity_x - 1, occluding_entity_y - 1, occluding_entity_z - 1):
        return True

    return False


def draw_gradient_vector(
    render_queue: RenderQueue,
    layer_index: int,
    effect_group: str,
    gradient_view_object: Any,
    terrain_gradient_camera_stats: bool,
    terrain: EntityInterface,
    light_intensity: float,
    oppacity: float,
    screen_x: int,
    screen_y: int,
) -> None:
    should_draw_gradient = False
    if terrain_gradient_camera_stats:
        if terrain.get_entity_type().sub_type0 == 0:
            direction = terrain.get_position().get_direction_as_int()
            if direction == aetherion.DirectionEnum_DOWN:
                should_draw_gradient = True
                gradient_view_object.set_sprite(aetherion.DirectionEnum_DOWN)
            elif direction == aetherion.DirectionEnum_UP:
                should_draw_gradient = True
                gradient_view_object.set_sprite(aetherion.DirectionEnum_UP)
            elif direction == aetherion.DirectionEnum_LEFT:
                should_draw_gradient = True
                gradient_view_object.set_sprite(aetherion.DirectionEnum_LEFT)
            elif direction == aetherion.DirectionEnum_RIGHT:
                should_draw_gradient = True
                gradient_view_object.set_sprite(aetherion.DirectionEnum_RIGHT)

    if terrain_gradient_camera_stats and should_draw_gradient:
        render_queue.add_task_by_id(
            layer_index,
            effect_group,
            gradient_view_object.sprite.sprite_id,
            screen_x,
            screen_y,
            float(light_intensity),
            float(oppacity),
        )


def draw_terrain(
    terrain: EntityInterface,
    view_object: Any,
    camera_settings: CameraSettings,
    selected_entity: Optional[int],
    world_view: WorldView,
    mouse_state: Dict[str, Any],
    screen_x: int,
    screen_y: int,
    render_queue: RenderQueue,
    layer_index: int,
    terrain_group_0: str,
    initial_light_intensity: float,
    gui_group: str,
    terrain_group_1: str,
    effect_group: str,
    sun_light: float,
    player: EntityInterface,
    gradient_view_object: Any,
    terrain_gradient_camera_stats: bool,
    water_view: Any,
    views: Dict[str, Any],
    water_camera_stats: bool,
    entity_hovered: Optional[EntityInterface] | None,
) -> bool:
    if terrain.get_entity_type().sub_type0 == aetherion.TerrainEnum_GRASS:
        set_grass_terrain_view_sprite(terrain, view_object)
    elif terrain.get_entity_type().sub_type0 == aetherion.TerrainEnum_WATER:
        set_water_terrain_view_sprite(terrain, view_object)
    elif terrain.get_entity_type().sub_type0 == aetherion.TerrainEnum_EMPTY:
        view_object.set_terrain_variation_sprite("full")

    current_entity_hovered = False
    if entity_hovered is None or (terrain is not None and terrain.get_entity_id() == selected_entity):
        current_entity_hovered = get_and_draw_selected_entity(
            world_view,
            terrain,
            mouse_state,
            screen_x,
            screen_y,
            render_queue,
            layer_index,
            terrain_group_0,
            selected_entity,
            camera_settings,
        )

    light_intensity = initial_light_intensity
    if current_entity_hovered:
        entity_hovered = terrain
        light_intensity = max(light_intensity - 0.2, 0)
    light_intensity += sun_light

    oppacity = 1.0
    render_group: str = terrain_group_0
    is_occluding_player_perspective: bool = aetherion.is_occluding_entity_perspective(player, world_view, terrain)
    if is_occluding_player_perspective:
        oppacity = 0.4

    render_queue.add_task_by_id(
        layer_index,
        render_group,
        view_object.sprite.sprite_id,
        screen_x,
        screen_y,
        float(light_intensity),
        float(oppacity),
    )
    if is_occluding_player_perspective or is_occluding_some_entity(world_view, terrain):
        render_queue.add_task_by_id_quadrant(
            layer_index,
            terrain_group_1,
            view_object.sprite.sprite_id,
            screen_x,
            screen_y,
            float(light_intensity),
            float(oppacity),
            TextureQuadrant.TOP_LEFT.value,
        )

    if (
        water_view is not None
        and terrain.get_entity_type().sub_type0 == 0
        and (terrain.get_matter_container().water_matter > 0 or terrain.get_matter_container().water_vapor > 0)
    ):
        water_view.set_terrain_variation_sprite("full")
        render_queue.add_task_by_id(
            layer_index,
            render_group,
            water_view.sprite.sprite_id,
            screen_x,
            screen_y,
            float(light_intensity),
            float(oppacity),
        )

    draw_gradient_vector(
        render_queue,
        layer_index,
        effect_group,
        gradient_view_object,
        terrain_gradient_camera_stats,
        terrain,
        light_intensity,
        oppacity,
        screen_x,
        screen_y,
    )

    if terrain.get_inventory() and len(terrain.get_inventory().item_ids) > 0:
        for item_id in terrain.get_inventory().item_ids:
            item = world_view.get_entity_by_id(item_id)

            item_enum_id = Classification(
                item.get_item_type_comp().main_type,
                item.get_item_type_comp().sub_type0,
            )
            item_view_object = views.get("items").get(item_enum_id)
            item_view_object.set_sprite("in_game_texture")

            render_queue.add_task_by_id(
                layer_index,
                render_group,
                item_view_object.sprite.sprite_id,
                screen_x - int(camera_settings.tile_size_on_screen),
                screen_y - int(camera_settings.tile_size_on_screen),
                float(light_intensity),
                float(oppacity),
            )

    if water_camera_stats:
        if terrain.get_matter_container().water_matter > 0 or terrain.get_entity_type().sub_type0 == 1:
            water_volume = str(terrain.get_matter_container().water_matter)
            # TODO: Fix this hardcoded font and color.
            sdl_color = (255, 255, 255)
            render_queue.add_task_text(layer_index, gui_group, water_volume, "my_font", sdl_color, screen_x, screen_y)
        if terrain.get_matter_container().water_vapor > 0 or terrain.get_entity_type().sub_type0 == 1:
            water_volume = str(terrain.get_matter_container().water_vapor)
            sdl_color = (255, 255, 255)
            render_queue.add_task_text(
                layer_index,
                gui_group,
                water_volume,
                "my_font",
                sdl_color,
                screen_x + 20,
                screen_y + 20,
            )

    return current_entity_hovered
